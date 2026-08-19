"""Emit the current checker baselines for a worktree, as json and as a one-line summary.

Usage: python baselines.py <worktree> [--json OUT.json]

Invokes this worktree's own scripts/check_method_slots.py, scripts/check_spawn_init.py and
scripts/check_state_inherit.py (never a copy from elsewhere) and parses their standing
verdict lines. This exists so a lane brief states baseline counts by running this tool
against the worktree in question, instead of hand-copying numbers from a previous lane's
report into a new one, which is exactly how stale numbers propagate.

Prints the json to stdout (or writes it to --json if given) followed by a one-line summary.
Exit code reflects only whether the three checkers could be run and parsed; it says nothing
about whether the counts are "good", since a rising baseline is sometimes the expected result
of a rung and sometimes a regression, and only a human or a lane report can tell those apart.
"""
import argparse
import json
import os
import re
import subprocess
import sys

METHOD_SLOTS_RE = re.compile(
    r"check_method_slots \((\w+)\): (\d+) game-DGO FAIL\(s\), (\d+) level-DGO note\(s\)")
SPAWN_INIT_RE = re.compile(
    r"check_spawn_init \((\w+)\): (\d+) game-DGO FAIL\(s\), (\d+) level-DGO note\(s\), "
    r"(\d+) dynamic site\(s\) skipped")
STATE_INHERIT_OK_RE = re.compile(
    r"virtual-state inheritance check \((\w+)\): OK, (\d+) defstates against (\d+) linked "
    r"objects, (\d+) level-order note\(s\)")
STATE_INHERIT_FAIL_RE = re.compile(
    r"virtual-state inheritance check \((\w+)\): (\d+) violation\(s\), (\d+) level-order note\(s\)")


def run_checker(script, worktree, extra_args=()):
    cmd = [sys.executable, script, "--root", worktree, *extra_args]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("worktree")
    ap.add_argument("--json", default=None, help="also write the json blob here")
    args = ap.parse_args()

    worktree = os.path.abspath(args.worktree)
    scripts_dir = os.path.join(worktree, "scripts")

    method_slots_script = os.path.join(scripts_dir, "check_method_slots.py")
    spawn_init_script = os.path.join(scripts_dir, "check_spawn_init.py")
    state_inherit_script = os.path.join(scripts_dir, "check_state_inherit.py")
    for s in (method_slots_script, spawn_init_script, state_inherit_script):
        if not os.path.isfile(s):
            print(f"missing checker script: {s}", file=sys.stderr)
            return 2

    result = {"worktree": worktree}
    problems = []

    rc, out, err = run_checker(method_slots_script, worktree)
    m = METHOD_SLOTS_RE.search(out)
    if not m:
        problems.append(f"check_method_slots.py: could not parse verdict line (rc={rc})\n{out}\n{err}")
    else:
        result["method_slots"] = {
            "game": m.group(1),
            "game_dgo_fails": int(m.group(2)),
            "level_dgo_notes": int(m.group(3)),
        }

    rc, out, err = run_checker(spawn_init_script, worktree)
    m = SPAWN_INIT_RE.search(out)
    if not m:
        problems.append(f"check_spawn_init.py: could not parse verdict line (rc={rc})\n{out}\n{err}")
    else:
        result["spawn_init"] = {
            "game": m.group(1),
            "game_dgo_fails": int(m.group(2)),
            "level_dgo_notes": int(m.group(3)),
            "dynamic_sites_skipped": int(m.group(4)),
        }

    rc, out, err = run_checker(state_inherit_script, worktree)
    m = STATE_INHERIT_OK_RE.search(out)
    if m:
        result["state_inherit"] = {
            "game": m.group(1),
            "defstates": int(m.group(2)),
            "linked_objects": int(m.group(3)),
            "level_order_notes": int(m.group(4)),
            "violations": 0,
        }
    else:
        m = STATE_INHERIT_FAIL_RE.search(out)
        if m:
            result["state_inherit"] = {
                "game": m.group(1),
                "defstates": None,
                "linked_objects": None,
                "level_order_notes": int(m.group(3)),
                "violations": int(m.group(2)),
            }
        else:
            problems.append(f"check_state_inherit.py: could not parse verdict line (rc={rc})\n{out}\n{err}")

    blob = json.dumps(result, indent=2)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            fh.write(blob + "\n")
        print(f"wrote {args.json}")
    print(blob)

    ms = result.get("method_slots", {})
    si = result.get("spawn_init", {})
    st = result.get("state_inherit", {})
    summary = (
        f"SUMMARY: method-slots {ms.get('game_dgo_fails', '?')}/{ms.get('level_dgo_notes', '?')} "
        f"spawn-init {si.get('game_dgo_fails', '?')}/{si.get('level_dgo_notes', '?')} "
        f"state-inherit {st.get('defstates', '?')}/{st.get('level_order_notes', '?')} "
        f"(violations {st.get('violations', '?')})"
    )
    print(summary)

    if problems:
        for p in problems:
            print(p, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
