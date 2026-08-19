"""Compact census: per object, ir2 ERROR count + landed/stub/verified-empty + band, from a
full-corpus decode dir and a goal_src tree. Read-only against both inputs. Writes
census-<label>.json and prints a per-band table of unlanded objects.

Usage: python census.py <corpus_out_jakx_dir> <repo_root> <label> [--out-dir DIR]

<corpus_out_jakx_dir> is a decompiler output directory for one full ("allowed_objects": [])
decode: it is read for every "<obj>_ir2.asm" (error count) and "<obj>_disasm.gc" (decoded
line count) pair. <repo_root> is walked for goal_src/jakx/**/*.gc to classify each object and
to derive its coarse engine band. --out-dir controls where census-<label>.json lands (default:
current directory); this is deliberately not "next to this script" so running the tool never
writes into the tracked scripts/orchestration/ directory by accident.

This is a single self-contained port of three formerly separate scratch scripts
(census3.py plus census2/scripts/landed.py's classify_file/family_of and leverage.py's
band_of); only those three functions survived the port, unchanged in logic.

States: "landed" (code below the ";; DECOMP BEGINS" marker), "stub" (nothing below it, or no
non-comment content at all), and "verified-empty": a stub-shaped file whose only content is
the literal ";; No code!" comment line, the landed convention (matching jak3's own) for an
object that genuinely compiles to nothing. verified-empty is reported separately from both
landed and stub because it represents a deliberate, gate-verified landing decision, not
missing work; counting it as "stub" would make it look like backlog, and counting it as
"landed" would hide that its only content is the marker comment itself.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

RE_ERR = re.compile(r"^;; ERROR:", re.M)
RE_NO_CODE = re.compile(r"^\s*;;\s*No code!\s*$", re.M)


def classify_file(path):
    """-> (state, code_lines) where state in {landed, stub, verified-empty}.

    Discriminator (verified on samples): a stub file is header comments only and
    ends at the `;; DECOMP BEGINS` line with no forms after it. A landed file has
    at least one non-blank, non-comment line after that marker. A file that would
    otherwise classify as a stub, but carries a literal ";; No code!" comment
    line, is reclassified verified-empty instead: eleven jakx objects (four
    byte-identical twin headers plus siblings landed the same way) are landed with
    exactly this content, on the jak3 convention for an object with no code of its
    own to compile.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        raw = fh.read()
    lines = raw.split("\n")
    started = False
    code = 0
    for ln in lines:
        s = ln.strip()
        if not started:
            if s.startswith(";; DECOMP BEGINS"):
                started = True
            continue
        if not s or s.startswith(";"):
            continue
        code += 1
    if not started:
        # no marker at all: count every non-comment line outside the header
        for ln in lines:
            s = ln.strip()
            if not s or s.startswith(";"):
                continue
            if s.startswith("(in-package"):
                continue
            code += 1
    if code > 0:
        return "landed", code
    if RE_NO_CODE.search(raw):
        return "verified-empty", code
    return "stub", code


def family_of(relpath, obj):
    """Coarse engine band from the goal_src path, else from the name."""
    if relpath:
        parts = relpath.replace("\\", "/").split("/")
        # goal_src/jakx/<a>/<b>/...
        if len(parts) >= 4:
            return "/".join(parts[2:4])
        if len(parts) >= 3:
            return parts[2]
    # name-based fallback
    for pre, fam in (("net-", "net"), ("lobby", "net/lobby"),
                     ("wvehicle", "wvehicle"), ("hud", "hud"),
                     ("nav-", "nav"), ("menu", "menu"), ("speech", "speech"),
                     ("cam-", "camera")):
        if obj.startswith(pre):
            return fam
    return "unmapped"


BANDS = [
    ("net/lobby", lambda o, f: o.startswith("lobby") or o.startswith("net-")
     or o.startswith("medius") or o.startswith("mysql") or "netmgr" in o),
    ("wvehicle", lambda o, f: o.startswith("wvehicle") or o.startswith("wcar")
     or o.startswith("vehicle") or o.startswith("v-wpn") or o.startswith("racer")
     or o.startswith("rigid-body") or o.startswith("wtank") or o.startswith("wbike")),
    ("hud", lambda o, f: o.startswith("hud")),
    ("nav", lambda o, f: o.startswith("nav") or o.startswith("find-nearest")),
    ("menu/ui", lambda o, f: o.startswith("menu") or o.startswith("3d-menu")
     or "options" in o or o.startswith("gui-") or o.startswith("progress")),
    ("camera", lambda o, f: o.startswith("cam")),
    ("speech/talker", lambda o, f: o.startswith("speech") or o.startswith("talker")),
    ("particles", lambda o, f: o.endswith("-part") or o.endswith("-part2")
     or o.startswith("part-")),
    ("level-obs", lambda o, f: o.endswith("-obs") or o.endswith("-obs-2")
     or o.endswith("-effects") or o.endswith("-ocean") or o.endswith("-scenes")),
]


def band_of(obj, fam):
    for name, pred in BANDS:
        if pred(obj, fam):
            return name
    if fam and fam.startswith("levels/"):
        return "levels/other"
    if fam and fam.startswith("engine/"):
        return fam
    return fam or "unmapped"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("corpus")
    ap.add_argument("root")
    ap.add_argument("label")
    ap.add_argument("--out-dir", default=".")
    args = ap.parse_args()

    corpus, root, label = args.corpus, args.root, args.label
    src = os.path.join(root, "goal_src", "jakx")

    idx = {}
    for dp, _d, files in os.walk(src):
        for f in files:
            if f.endswith(".gc") and not f.endswith("_REF.gc"):
                idx[f[:-3]] = os.path.join(dp, f)

    objs = {}
    for f in sorted(os.listdir(corpus)):
        if not f.endswith("_ir2.asm"):
            continue
        o = f[:-len("_ir2.asm")]
        ir2 = len(RE_ERR.findall(open(os.path.join(corpus, f), encoding="utf-8", errors="replace").read()))
        p = idx.get(o)
        if p:
            state, code = classify_file(p)
            rel = os.path.relpath(p, root)
        else:
            state, code, rel = "no-source-file", 0, None
        fam = family_of(rel, o)
        dis = os.path.join(corpus, o + "_disasm.gc")
        dlines = 0
        if os.path.exists(dis):
            dlines = sum(1 for ln in open(dis, encoding="utf-8", errors="replace")
                         if ln.strip() and not ln.strip().startswith(";"))
        objs[o] = dict(object=o, path=rel, band=band_of(o, fam), state=state, ir2=ir2,
                       code_lines=code, decoded_code_lines=dlines)

    os.makedirs(args.out_dir, exist_ok=True)
    out_path = os.path.join(args.out_dir, f"census-{label}.json")
    json.dump(dict(label=label, objects=objs), open(out_path, "w"), indent=1)

    landed = sum(1 for o in objs.values() if o["state"] == "landed")
    verified_empty = sum(1 for o in objs.values() if o["state"] == "verified-empty")
    stub = sum(1 for o in objs.values() if o["state"] == "stub")
    total_ir2 = sum(o["ir2"] for o in objs.values())
    print(f"{label}: {landed} landed / {verified_empty} verified-empty / {stub} stub / "
          f"{len(objs)} objects; total ir2 errors {total_ir2}")
    bands = defaultdict(list)
    for o in objs.values():
        if o["state"] not in ("landed", "verified-empty"):
            bands[o["band"]].append(o)
    for b, l in sorted(bands.items(), key=lambda kv: -len(kv[1])):
        print(f"\n== {b} ({len(l)} unlanded, {sum(x['ir2'] for x in l)} errors)")
        for x in sorted(l, key=lambda x: x["ir2"]):
            print(f"  {x['object']:34s} ir2={x['ir2']:4d} decoded_lines={x['decoded_code_lines']}")
    print(f"\n-> {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
