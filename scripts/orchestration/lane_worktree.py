"""Create and remove jakx lane worktrees safely.

Usage:
  python lane_worktree.py create <name> <branch> [<base>]
  python lane_worktree.py remove <name> [--force]

create: `git worktree add` under <primary>/.worktrees/<name> on a new branch <branch>
(from <base> if given, else the repo's current HEAD), then junctions
<worktree>\\iso_data\\jakx and <worktree>\\decompiler_out to the primary checkout's own
copies (junctions, not symlinks, so no admin rights are needed; created via `cmd /c mklink
/J` with absolute Windows paths, since mklink fails silently or errors on relative ones from
a bash-style invocation), then robocopy-seeds <worktree>\\out\\jakx from the primary so the
first `(mi)` in the new worktree is incremental rather than a from-scratch build. Every step
is verified: the junctions' recorded targets are read back and compared, and the iso_data
junction's file count is checked nonzero.

remove: refuses outright if the worktree directory does not exist. Unlinks every junction
found under the tree first (a plain `git worktree remove` or a recursive delete would
otherwise walk INTO the junction and either loop into or delete the primary's real files
depending on the tool), then runs `git worktree remove`, then verifies the directory actually
left disk. Windows sometimes leaves a locked, empty husk behind after a worktree remove
because some other handle (an editor, an indexer, a lingering build) still has a file open
under it; that is reported, not treated as a script failure, since this tool has no way to
find or close that handle. Finally verifies the PRIMARY's own iso_data\\jakx and
decompiler_out are untouched (nonzero file count / still a real directory), since a junction
removal that went wrong in the other direction would delete the primary's actual data.

This script never checks whether a live agent still has the worktree open; that rule belongs
to the runbook (a human or orchestrator decision), not to this mechanical script.

The repo location is derived from this script's own path (a `git rev-parse
--git-common-dir` from wherever this file happens to be checked out), so it never hardcodes
a drive letter or username, and every git operation this tool runs uses that location as its
`-C`, not the caller's cwd.
"""
import argparse
import os
import subprocess
import sys


def git_common_dir(anchor):
    r = subprocess.run(
        ["git", "-C", anchor, "rev-parse", "--path-format=absolute", "--git-common-dir"],
        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"git rev-parse --git-common-dir failed under {anchor}: {r.stderr.strip()}")
    return r.stdout.strip()


def primary_root_from_here():
    anchor = os.path.dirname(os.path.abspath(__file__))
    common = git_common_dir(anchor)
    # common is .../<primary>/.git for a normal (non-bare) repo
    root = os.path.dirname(common)
    if not os.path.isdir(os.path.join(root, ".git")):
        raise RuntimeError(f"derived primary root '{root}' has no .git; git-common-dir was '{common}'")
    return root, anchor


def run(cmd, check=True, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if check and r.returncode != 0:
        raise RuntimeError(f"command failed ({r.returncode}): {' '.join(cmd)}\n{r.stdout}\n{r.stderr}")
    return r


def junction_target(path):
    """Recorded target of a junction, or None if path is not a junction."""
    r = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command",
         f"(Get-Item -LiteralPath '{path}' -ErrorAction Stop).Target"],
        capture_output=True, text=True)
    if r.returncode != 0:
        return None
    out = r.stdout.strip()
    return out or None


def make_junction(link, target):
    if os.path.exists(link):
        raise RuntimeError(f"refusing to create junction, path already exists: {link}")
    if not os.path.isdir(target):
        raise RuntimeError(f"junction target does not exist or is not a directory: {target}")
    link_w = os.path.normpath(link)
    target_w = os.path.normpath(target)
    r = subprocess.run(["cmd", "/c", "mklink", "/J", link_w, target_w], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"mklink /J failed: {link_w} -> {target_w}\n{r.stdout}\n{r.stderr}")


def count_files(path):
    n = 0
    for _dp, _dn, files in os.walk(path):
        n += len(files)
    return n


def find_junctions(root):
    """Every reparse-point directory under root, without descending into them."""
    found = []

    def walk(d):
        try:
            entries = list(os.scandir(d))
        except OSError:
            return
        for e in entries:
            if os.path.isjunction(e.path):
                found.append(e.path)
                continue
            if e.is_dir(follow_symlinks=False):
                walk(e.path)

    walk(root)
    return found


def cmd_create(args):
    root, anchor = primary_root_from_here()
    wt_path = os.path.join(root, ".worktrees", args.name)
    if os.path.exists(wt_path):
        print(f"refusing: worktree path already exists: {wt_path}", file=sys.stderr)
        return 2

    add_cmd = ["git", "-C", anchor, "worktree", "add", wt_path, "-b", args.branch]
    if args.base:
        add_cmd.append(args.base)
    print("+", " ".join(add_cmd))
    run(add_cmd)

    check = run(["git", "-C", wt_path, "rev-parse", "--is-inside-work-tree"], check=False)
    if check.returncode != 0 or check.stdout.strip() != "true":
        print(f"worktree add reported success but {wt_path} does not look like a worktree", file=sys.stderr)
        return 1
    print(f"worktree registered at {wt_path}")

    iso_link = os.path.join(wt_path, "iso_data", "jakx")
    iso_target = os.path.join(root, "iso_data", "jakx")
    os.makedirs(os.path.dirname(iso_link), exist_ok=True)
    make_junction(iso_link, iso_target)

    decomp_link = os.path.join(wt_path, "decompiler_out")
    decomp_target = os.path.join(root, "decompiler_out")
    if not os.path.isdir(decomp_target):
        # nothing decoded into the primary yet; still fine to skip, only the two
        # generated dirs are junctioned by design and decompiler_out may not exist
        print(f"note: primary has no decompiler_out yet ({decomp_target}); skipping that junction")
    else:
        make_junction(decomp_link, decomp_target)

    out_src = os.path.join(root, "out", "jakx")
    out_dst = os.path.join(wt_path, "out", "jakx")
    if os.path.isdir(out_src):
        os.makedirs(os.path.dirname(out_dst), exist_ok=True)
        rc_cmd = ["robocopy", out_src, out_dst, "/E", "/NFL", "/NDL", "/NJH", "/NJS"]
        print("+", " ".join(rc_cmd))
        r = subprocess.run(rc_cmd, capture_output=True, text=True)
        # robocopy's exit codes are a bitmask; 0-7 are all "no error", 8+ is real failure
        if r.returncode >= 8:
            print(f"robocopy seed failed (exit {r.returncode})\n{r.stdout}\n{r.stderr}", file=sys.stderr)
            return 1
        print(f"seeded out/jakx (robocopy exit {r.returncode})")
    else:
        print(f"note: primary has no out/jakx yet ({out_src}); new worktree will build from scratch")

    problems = []
    t = junction_target(iso_link)
    if not t or os.path.normcase(os.path.normpath(t)) != os.path.normcase(os.path.normpath(iso_target)):
        problems.append(f"iso_data\\jakx junction target mismatch: got '{t}', want '{iso_target}'")
    n = count_files(iso_link)
    if n == 0:
        problems.append(f"iso_data\\jakx has zero files through the junction")
    print(f"verify: iso_data\\jakx -> {t} ({n} files)")

    if os.path.isdir(decomp_target):
        t2 = junction_target(decomp_link)
        if not t2 or os.path.normcase(os.path.normpath(t2)) != os.path.normcase(os.path.normpath(decomp_target)):
            problems.append(f"decompiler_out junction target mismatch: got '{t2}', want '{decomp_target}'")
        print(f"verify: decompiler_out -> {t2}")

    if problems:
        for p in problems:
            print("PROBLEM:", p, file=sys.stderr)
        return 1
    print(f"create OK: {wt_path}")
    return 0


def cmd_remove(args):
    root, anchor = primary_root_from_here()
    wt_path = os.path.join(root, ".worktrees", args.name)
    if not os.path.isdir(wt_path):
        print(f"refusing: worktree path does not exist: {wt_path}", file=sys.stderr)
        return 2

    junctions = find_junctions(wt_path)
    print(f"unlinking {len(junctions)} junction(s) under {wt_path}")
    for j in junctions:
        try:
            os.rmdir(j)
            print(f"  unlinked {j}")
        except OSError as e:
            r = subprocess.run(["cmd", "/c", "rmdir", os.path.normpath(j)], capture_output=True, text=True)
            if r.returncode != 0:
                print(f"  FAILED to unlink {j}: {e}; rmdir fallback: {r.stdout}{r.stderr}", file=sys.stderr)
                return 1
            print(f"  unlinked {j} (via cmd rmdir fallback)")

    rm_cmd = ["git", "-C", anchor, "worktree", "remove", wt_path]
    if args.force:
        rm_cmd.append("--force")
    print("+", " ".join(rm_cmd))
    r = subprocess.run(rm_cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"git worktree remove failed:\n{r.stdout}\n{r.stderr}", file=sys.stderr)
        return 1
    print(r.stdout.strip() or "git worktree remove: ok")

    if os.path.exists(wt_path):
        print(f"REPORT: {wt_path} is unregistered but a locked husk remains on disk "
              f"(some other handle still has a file open under it); this is not a script "
              f"failure, but the directory needs a human or a later cleanup pass to delete it")
    else:
        print(f"{wt_path} left disk cleanly")

    iso_target = os.path.join(root, "iso_data", "jakx")
    decomp_target = os.path.join(root, "decompiler_out")
    problems = []
    if not os.path.isdir(iso_target):
        problems.append(f"primary iso_data\\jakx is missing after removal: {iso_target}")
    else:
        n = count_files(iso_target)
        print(f"verify: primary iso_data\\jakx still has {n} file(s)")
        if n == 0:
            problems.append("primary iso_data\\jakx has zero files after removal")
    if os.path.isdir(decomp_target):
        print(f"verify: primary decompiler_out still exists: {decomp_target}")
    # decompiler_out not existing on the primary is not itself a problem; it may
    # never have been generated there, same as during create

    if problems:
        for p in problems:
            print("PROBLEM:", p, file=sys.stderr)
        return 1
    print("remove OK")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="action", required=True)

    ap_create = sub.add_parser("create")
    ap_create.add_argument("name")
    ap_create.add_argument("branch")
    ap_create.add_argument("base", nargs="?", default=None)

    ap_remove = sub.add_parser("remove")
    ap_remove.add_argument("name")
    ap_remove.add_argument("--force", action="store_true")

    args = ap.parse_args()
    if args.action == "create":
        return cmd_create(args)
    return cmd_remove(args)


if __name__ == "__main__":
    sys.exit(main())
