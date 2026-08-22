#!/usr/bin/env python3
"""Fail a change that defers work without leaving a tracked owner.

Why this exists: a block of code was commented out with a note saying to uncomment
it before a later change landed. The note was correct, nobody acted on it, and the
resulting crash took a day to trace. The comment was not the problem; relying on
somebody remembering it was. A marker that names an issue survives the person who
wrote it, which is the same reasoning AGENTS.md already applies to build output and
generated files.

Two modes:

  diff    Check only lines ADDED relative to a base ref, and exit non-zero if any
          defer work without an issue reference. This is a ratchet: pre-existing
          markers are not the concern of the change that happens to touch the file
          next, or nobody could land anything.

  report  Inventory every marker in the tree, split by whether it is tracked. Use
          this to triage the backlog, not in CI.

Both modes read COMMITTED content through git, NOT the working tree, so an
uncommitted change is invisible here. A warning is printed when the tree is
dirty.

A marker is "tracked" if it cites an issue (#123) or a forge URL. Deleting the code
counts as fixing it too: git remembers, and a commented-out block does not.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

# Paths that are not ours to police. Vendored sources carry their own TODOs by the
# thousand and flagging them would train everyone to ignore this check.
EXCLUDED_PREFIXES = (
    "third-party/",
    "decompiler_out/",
    ".git/",
)

# Only source we actually author or maintain. all-types.gc is excluded on purpose:
# it is decompiler-generated configuration where "uncommented" appears as a factual
# record of what was done, not as a deferred action.
INCLUDED_SUFFIXES = (".gc", ".gs", ".cpp", ".h", ".hpp", ".cc", ".py", ".yaml", ".yml")
EXCLUDED_BASENAMES = (
    "all-types.gc",
    # The checkers necessarily contain the very words they search for, in their
    # patterns and in the explanation of why they exist. Without this they fail on
    # their own source, which is a fine way to have the check deleted on day one.
    "check_deferred_markers.py",
    "find_unarmed_levers.py",
    "gen_inert_ledger.py",
)

# Classic markers, whole-word so READDATA does not match re-add.
# Hyphens count as word-interior here: retail GOAL symbols legitimately embed
# marker words ('guided-missile-update-HACK ships in wvehicle-weapons-proj.go's
# own symbol table), and a human deferral note never hyphenates straight into
# its marker word. Without this, verbatim transcription of retail code reddens
# the gate.
MARKER_WORDS = re.compile(r"(?<![A-Za-z-])(TODO|FIXME|XXX|HACK)(?![A-Za-z-])")

# The dangerous class: an instruction to restore or enable something later. These
# are worse than a TODO because the code reads as complete while being inert.
DEFERRAL_PHRASES = re.compile(
    r"\buncomment\b"
    r"|\bre-?enable\b"
    r"|\bput (?:this |it )?back\b"
    r"|\brestore (?:this|it|these)\b"
    r"|\bbefore (?:we|you|merging|landing|shipping|releasing)\b"
    r"|\bdon'?t forget\b"
    r"|\bremember to\b"
    r"|\btemporarily (?:disabled|removed|commented)\b",
    re.IGNORECASE,
)

ISSUE_REF = re.compile(r"#\d+|/issues/\d+")

# A comment line whose payload still looks like code. Catches a live line converted
# into a commented-out one, which is how inert code gets introduced in the first place.
COMMENT_PREFIX = re.compile(r"^\s*(?:;;+|//+|#)\s?(.*)$")
CODE_SHAPED = re.compile(
    r"^\(.*\)\s*$"                      # GOAL form
    r"|^\(\w[\w!?*<>=/-]*\s"            # GOAL call
    r"|;\s*$"                           # C++ statement
    r"|^\s*(?:if|for|while|return|set!|defun|defmethod)\b"
)


def is_relevant(path: str) -> bool:
    if any(path.startswith(p) for p in EXCLUDED_PREFIXES):
        return False
    if Path(path).name in EXCLUDED_BASENAMES:
        return False
    return path.endswith(INCLUDED_SUFFIXES)


def classify(line: str) -> str | None:
    """Return the reason a line is a deferral, or None."""
    if DEFERRAL_PHRASES.search(line):
        return "deferral"
    if MARKER_WORDS.search(line):
        return "marker"
    return None


def commented_code(line: str) -> bool:
    m = COMMENT_PREFIX.match(line)
    if not m:
        return False
    payload = m.group(1).strip()
    return len(payload) > 8 and bool(CODE_SHAPED.search(payload))


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


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


def added_lines(base: str):
    """Yield (path, line) for every line added relative to base.

    Two-dot diff, NOT three-dot, because CI checks out with --depth=1 and a shallow
    HEAD has no ancestry, so `base...HEAD` cannot compute a merge base. Two-dot
    compares the two trees directly and needs no common ancestor. It is safe here:
    anything on base but not on HEAD appears as a REMOVAL, and only added lines are
    inspected.
    """
    diff = git("diff", "--unified=0", base, "HEAD")
    path = None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            path = line[6:]
        elif line.startswith("+") and not line.startswith("+++") and path:
            if is_relevant(path):
                yield path, line[1:]


def mode_diff(base: str) -> int:
    warn_if_dirty()
    offences, disabled = [], []
    for path, line in added_lines(base):
        kind = classify(line)
        if kind and not ISSUE_REF.search(line):
            offences.append((path, kind, line.strip()))
        if commented_code(line):
            disabled.append((path, line.strip()))

    if disabled:
        # Advisory, not fatal: commenting code out is sometimes right. Printing it
        # back means it is never a silent act.
        print(f"note: this change adds {len(disabled)} commented-out code line(s):")
        for path, line in disabled[:15]:
            print(f"    {path}: {line[:100]}")
        if len(disabled) > 15:
            print(f"    ... and {len(disabled) - 15} more")
        print()

    if not offences:
        print("deferred-marker check: OK (no untracked deferrals added)")
        return 0

    print(f"deferred-marker check FAILED: {len(offences)} untracked deferral(s) added\n")
    for path, kind, line in offences:
        print(f"  {path}\n    [{kind}] {line[:140]}")
    print(
        "\nEach line above defers work with nothing tracking it. Either:\n"
        "  - cite an issue, e.g. 'TODO(#123): ...' or 'uncomment before X, see #123', or\n"
        "  - delete the code instead of commenting it out (git remembers it), or\n"
        "  - just do the thing now.\n"
        "This exists because an untracked 'uncomment before' note cost a day of\n"
        "bug-hunting once already."
    )
    return 1


def mode_report(ref: str) -> int:
    files = [p for p in git("ls-tree", "-r", "--name-only", ref).splitlines() if is_relevant(p)]
    tracked, untracked = [], []
    for path in files:
        content = git("show", f"{ref}:{path}")
        for n, line in enumerate(content.splitlines(), 1):
            if classify(line):
                (tracked if ISSUE_REF.search(line) else untracked).append((path, n, line.strip()))

    print(f"scanned {len(files)} files on {ref}")
    print(f"  tracked (cite an issue): {len(tracked)}")
    print(f"  UNTRACKED              : {len(untracked)}\n")
    by_file: dict[str, int] = {}
    for path, _, _ in untracked:
        by_file[path] = by_file.get(path, 0) + 1
    print("worst files:")
    for path, count in sorted(by_file.items(), key=lambda kv: -kv[1])[:15]:
        print(f"  {count:>3}  {path}")
    print("\nsample untracked:")
    for path, n, line in untracked[:25]:
        print(f"  {path}:{n}: {line[:120]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", choices=("diff", "report"), default="diff")
    ap.add_argument("--base", default="origin/develop",
                    help="base ref for diff mode (default: origin/develop)")
    ap.add_argument("--ref", default="HEAD", help="ref to inventory in report mode")
    args = ap.parse_args()
    return mode_diff(args.base) if args.mode == "diff" else mode_report(args.ref)


if __name__ == "__main__":
    sys.exit(main())
