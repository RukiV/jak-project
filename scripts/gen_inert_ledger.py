#!/usr/bin/env python3
"""Generate the inert-mechanism ledger: the first place to look when something breaks.

The problem this solves is not that someone wrote a bad comment. It is that a
mechanism can be landed, wired and inert, and nothing anywhere says so. When the
resulting crash arrives a week later there is no list to consult, so the search
starts from the crash and works outward, which is the expensive direction. Issue
#171 cost a day that way.

A comment-based check cannot fix that, because the dangerous case is the one where
nobody wrote a comment at all. This is state-based instead: it reads the tree and
reports what IS inert, whether or not anyone noted it.

The output is committed. CI regenerates it and fails when it differs from what is
checked in, which is the same rule AGENTS.md section 2 already applies to every
other generated file: stale until proven otherwise. Two consequences worth having:

  - A pull request that disarms something shows it AS A DIFF, in review, next to
    the change that caused it. "This branch moved *foo* from armed to unarmed" is
    a sentence review can act on; a CI failure saying "you added a TODO" is not.
  - Landing a switch also shows up, as a line leaving the ledger. Progress is
    visible without anyone maintaining a list by hand, which is what #176 is.

This reads COMMITTED content through git, not the working tree, so uncommitted
changes are invisible to it; a warning is printed when the tree is dirty.

Regenerate with:  python3 scripts/gen_inert_ledger.py --write
Check in CI with: python3 scripts/gen_inert_ledger.py --check
"""
from __future__ import annotations

import argparse
import difflib
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from find_unarmed_levers import git, scan  # noqa: E402


def warn_if_dirty() -> None:
    """Say so when uncommitted work exists, because it is invisible to this check.

    Both of these tools read committed content through git, not the working tree.
    Run one before committing and it reports on the previous commit while looking
    like it reported on your change. That produced two wrong readings in one
    sitting, so it gets said out loud rather than documented and forgotten. CI
    checks out clean, so this never fires there.
    """
    dirty = [ln for ln in git("status", "--porcelain").splitlines() if ln.strip()]
    if dirty:
        print(
            f"NOTE: {len(dirty)} uncommitted change(s) in the working tree.\n"
            "      This check reads COMMITTED content (HEAD), so those are NOT\n"
            "      included. Commit first, or this result describes the previous\n"
            "      commit rather than your change.\n",
            file=sys.stderr,
        )


GAMES = ("jakx", "jak3", "jak2", "jak1")
LEDGER = Path("docs/inert-inventory.md")

HEADER = """<!-- GENERATED FILE. Do not edit by hand.
     Regenerate with: python3 scripts/gen_inert_ledger.py --write
     CI fails if this file is out of date with the tree. -->

# Inert mechanism inventory

**When something behaves as though it is simply not happening, look here first.**

This lists machinery that is present and consumed by live code but that nothing can
ever switch on. It is generated from the source, so it does not depend on anyone
having written a note. See AGENTS.md section 12 for why this class is expensive, and
issue #176 for the hand-maintained ledger this automates.

Two classes, most serious first:

- **Switch commented out**: live code reads the symbol and its only writer is sitting
  in a comment. This is the shape of issue #171, which cost a day of tracing.
- **Never written**: live code reads the symbol and nothing writes it anywhere, in
  comments or otherwise. Usually means the setter was never ported.

Only scalar-initialised globals are listed. A static struct or buffer that is never
written is read-only by design and not a finding.

"""


def emit(ref: str) -> str:
    out = [HEADER]
    for game in GAMES:
        prefix = f"goal_src/{game}/"
        defines, reads, writes, commented_writes, init_kind = scan(ref, prefix)
        disabled, unarmed = [], []
        for sym in set(defines) | set(reads) | set(writes) | set(commented_writes):
            r, w = len(reads.get(sym, ())), len(writes.get(sym, ()))
            if r >= 1 and w == 0:
                if commented_writes.get(sym):
                    disabled.append((sym, r, sorted(commented_writes[sym])))
                elif init_kind.get(sym) == "scalar":
                    unarmed.append((sym, r, defines.get(sym, "?")))
        if not disabled and not unarmed:
            continue
        out.append(f"## {game}\n")
        if disabled:
            out.append("### Switch commented out\n")
            out.append("| symbol | reading files | switch commented in |")
            out.append("|---|---:|---|")
            for sym, n, where in sorted(disabled, key=lambda t: (-t[1], t[0])):
                out.append(f"| `{sym}` | {n} | {', '.join(f'`{w}`' for w in where)} |")
            out.append("")
        if unarmed:
            out.append(f"### Never written ({len(unarmed)})\n")
            out.append("| symbol | reading files | defined in |")
            out.append("|---|---:|---|")
            for sym, n, where in sorted(unarmed, key=lambda t: (-t[1], t[0])):
                out.append(f"| `{sym}` | {n} | `{where}` |")
            out.append("")
    return "\n".join(out).rstrip() + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default="HEAD")
    ap.add_argument("--write", action="store_true", help="write the ledger")
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if the committed ledger is out of date")
    args = ap.parse_args()

    warn_if_dirty()
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                          capture_output=True, text=True).stdout.strip()
    path = Path(root) / LEDGER
    fresh = emit(args.ref)

    if args.write:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(fresh, encoding="utf-8")
        print(f"wrote {LEDGER}")
        return 0

    if args.check:
        current = path.read_text(encoding="utf-8") if path.exists() else ""
        if current == fresh:
            print(f"{LEDGER} is up to date")
            return 0
        print(f"{LEDGER} is OUT OF DATE with the tree.\n")
        diff = difflib.unified_diff(current.splitlines(), fresh.splitlines(),
                                    fromfile="committed", tofile="regenerated", lineterm="")
        for line in list(diff)[:60]:
            print(line)
        print("\nA line ARRIVING means this change left a mechanism inert; a line "
              "LEAVING means it armed one.\nEither is fine, but it has to be a "
              "visible decision. Run:\n\n    python3 scripts/gen_inert_ledger.py --write\n\n"
              "then commit the result with your change.")
        return 1

    sys.stdout.write(fresh)
    return 0


if __name__ == "__main__":
    sys.exit(main())
