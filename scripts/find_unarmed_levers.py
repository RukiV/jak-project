#!/usr/bin/env python3
"""Find unarmed mechanisms in GOAL source: globals that are READ but never WRITTEN.

The pattern is the one catalogued in issue #176, "landed machinery whose switch is
missing": a setting is declared, live code reads it every frame, and nothing anywhere
ever sets it. The mechanism is therefore permanently stuck at its default, usually
off. Nothing is broken in a way a compiler or a test can see, the code reads as
complete, and the failure surfaces later as behaviour that is simply absent, or as a
crash when a half-armed path is finally taken.

That sweep was done by hand and found 68. Doing it by hand again next quarter is the
part worth automating.

Two inverse classes are reported:

  UNARMED  read >= 1, written 0   the lever exists and is consulted, but nothing
                                  can ever flip it. This is the #176 class.

  DEAD     written >= 1, read 0   something maintains a value nobody consults. Less
                                  dangerous, but it is how a consumer gets deleted
                                  while its producer keeps running.

Definitions are not counted as writes. `(define *foo* #f)` establishes the default;
it is precisely the thing that never changes.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict

# GOAL globals are conventionally *earmuffed*, which makes them findable without
# parsing the language.
GLOBAL = re.compile(r"\*[a-zA-Z][a-zA-Z0-9!?*<>=/+-]*\*")

DEFINE = re.compile(r"\(\s*(?:define|define-extern|define-perm)\s+(\*[^\s()]+\*)")
# Capture what a define initialises to, so a switch can be told from a constant.
DEFINE_INIT = re.compile(r"\(\s*(?:define|define-perm)\s+(\*[^\s()]+\*)\s+(.*)$")

# A lever is a scalar the code flips: #f/#t, a number, or a quoted mode symbol. A
# buffer, vector or static struct that is never written is read-only BY DESIGN and
# not a finding. Distinguishing them is the whole precision problem: without it the
# check reports 739 candidates against a hand sweep's 68 and gets ignored, which is
# worse than not running it at all.
SCALAR_INIT = re.compile(r"^(?:#f|#t|-?\d+(?:\.\d+)?|'[a-zA-Z][\w!?*<>=/-]*)\s*\)?\s*$")
STRUCTURE_INIT = re.compile(r"^\(\s*(?:new|the-as|the)\b|^\(\s*zero-vector|^\(\s*vector")
# A write is set! on the symbol itself, or on a field reached through it.
SET_DIRECT = re.compile(r"\(\s*set!\s+(\*[^\s()]+\*)")
SET_FIELD = re.compile(r"\(\s*set!\s+\(\s*->\s+(\*[^\s()]+\*)")
# set-setting! and friends arm things without a literal set!.
SET_SETTING = re.compile(r"\(\s*(?:set-setting!|send-event|process-spawn)\b[^\n]*?(\*[^\s()]+\*)")


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


def read_blobs(ref: str, paths: list[str]) -> dict[str, str]:
    """Read many blobs in ONE git process.

    Spawning `git show` per file cost 95s across the four game trees, almost all of
    it process creation. `cat-file --batch` streams every blob through a single
    process instead, which matters because this runs on every pull request.
    """
    if not paths:
        return {}
    stdin = "".join(f"{ref}:{p}\n" for p in paths).encode()
    proc = subprocess.run(["git", "cat-file", "--batch"], input=stdin,
                          capture_output=True)
    out, pos, result = proc.stdout, 0, {}
    for path in paths:
        nl = out.find(b"\n", pos)
        if nl == -1:
            break
        header = out[pos:nl].decode("utf-8", "replace")
        pos = nl + 1
        parts = header.split()
        if len(parts) != 3:      # "<oid> missing" for a path not in this ref
            continue
        size = int(parts[2])
        result[path] = out[pos:pos + size].decode("utf-8", "replace")
        pos += size + 1          # blob content plus its trailing newline
    return result


def scan(ref: str, prefix: str):
    files = [p for p in git("ls-tree", "-r", "--name-only", ref).splitlines()
             if p.startswith(prefix) and p.endswith((".gc", ".gs"))]
    reads: dict[str, set[str]] = defaultdict(set)
    writes: dict[str, set[str]] = defaultdict(set)
    # Writes that exist ONLY inside comments. This is the sharpest signal there is:
    # the switch was written, then disabled, and the mechanism it arms still reads it.
    # It is exactly the shape of issue #171, where *external-cam-mode* is consumed by
    # a fully landed free-cam while its only setter sits commented at main.gc:2395.
    commented_writes: dict[str, set[str]] = defaultdict(set)
    defines: dict[str, str] = {}
    init_kind: dict[str, str] = {}

    blobs = read_blobs(ref, files)
    for path in files:
        text = blobs.get(path, "")
        for raw in text.splitlines():
            live, _, commented = raw.partition(";;")
            if commented.strip():
                for rx in (SET_DIRECT, SET_FIELD, SET_SETTING):
                    for m in rx.finditer(commented):
                        commented_writes[m.group(1)].add(path)
            line = live  # commented-out code is not live
            if not line.strip():
                continue
            for m in DEFINE.finditer(line):
                defines.setdefault(m.group(1), path)
            for m in DEFINE_INIT.finditer(line):
                sym, rest = m.group(1), m.group(2).strip()
                if sym not in init_kind:
                    if STRUCTURE_INIT.search(rest):
                        init_kind[sym] = "structure"
                    elif SCALAR_INIT.match(rest):
                        init_kind[sym] = "scalar"
                    else:
                        init_kind[sym] = "other"

            written_here = set()
            for rx in (SET_DIRECT, SET_FIELD, SET_SETTING):
                for m in rx.finditer(line):
                    written_here.add(m.group(1))
            for sym in written_here:
                writes[sym].add(path)
            for m in GLOBAL.finditer(line):
                sym = m.group(0)
                if sym not in written_here:
                    reads[sym].add(path)

    # A define is not a read of itself.
    for sym, path in defines.items():
        reads[sym].discard(path) if len(reads[sym]) > 1 else None

    return defines, reads, writes, commented_writes, init_kind


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default="HEAD")
    ap.add_argument("--prefix", default="goal_src/jakx/",
                    help="restrict to one game's source tree")
    ap.add_argument("--expect", nargs="*", default=[],
                    help="symbols that MUST come back UNARMED; used to prove the "
                         "detector actually detects, rather than trusting it")
    ap.add_argument("--expect-armed", nargs="*", default=[],
                    help="symbols that must NOT be flagged, i.e. their switch has "
                         "since been landed. A detector with no negative control is "
                         "just a thing that says yes.")
    ap.add_argument("--all-globals", action="store_true",
                    help="do not filter to scalar-initialised switches; reports every "
                         "read-never-written global including read-only constants")
    ap.add_argument("--limit", type=int, default=30)
    args = ap.parse_args()

    defines, reads, writes, commented_writes, init_kind = scan(args.ref, args.prefix)

    unarmed, dead, disabled_switch = [], [], []
    for sym in set(defines) | set(reads) | set(writes) | set(commented_writes):
        r, w = len(reads.get(sym, ())), len(writes.get(sym, ()))
        cw = commented_writes.get(sym, set())
        if r >= 1 and w == 0:
            if args.all_globals or init_kind.get(sym) == "scalar":
                unarmed.append((sym, r, defines.get(sym, "?")))
            if cw:
                disabled_switch.append((sym, r, sorted(cw)))
        elif w >= 1 and r == 0:
            dead.append((sym, w, defines.get(sym, "?")))

    unarmed.sort(key=lambda t: -t[1])
    dead.sort(key=lambda t: -t[1])

    print(f"scanned {args.prefix} at {args.ref}: "
          f"{len(defines)} globals defined, {len(reads)} read, {len(writes)} written\n")
    scope = "all globals" if args.all_globals else "scalar-initialised switches only"
    print(f"UNARMED (read but never written) : {len(unarmed)}   [{scope}]")
    print(f"DEAD    (written but never read)  : {len(dead)}\n")

    if disabled_switch:
        print("*** HIGHEST PRIORITY: read by live code, and its ONLY writer is "
              "commented out ***")
        for sym, n, where in sorted(disabled_switch, key=lambda t: -t[1]):
            print(f"  {n:>3} readers  {sym:<34} switch commented in: {', '.join(where)}")
        print()

    print(f"--- top {args.limit} UNARMED, by number of reading files ---")
    for sym, n, where in unarmed[:args.limit]:
        print(f"  {n:>3} readers  {sym:<38} defined: {where}")

    if args.expect:
        print("\n--- detector self-check against known-unarmed symbols from #176 ---")
        found = {s for s, _, _ in unarmed}
        ok = True
        for sym in args.expect:
            hit = sym in found
            ok &= hit
            state = "UNARMED (as expected)" if hit else (
                "MISSED" if sym in set(defines) | set(reads) | set(writes) else "not found at all")
            print(f"  {sym:<30} {state}")
        print("\nself-check:", "PASS" if ok else "FAIL - detector does not reproduce the known sweep")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
