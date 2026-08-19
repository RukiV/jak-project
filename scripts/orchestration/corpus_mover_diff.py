"""Standardize the mover-diff procedure: decode a worktree's full corpus at its current
config, then again at a base commit's config, and report every object whose ";; ERROR:"
count moved between the two.

Usage:
  python corpus_mover_diff.py <repo_root> <base_sha> <decompiler_exe> --scratch <dir>
      [--game jakx] [--version ntsc_v1] [--iso-data <path>]

<repo_root> must be a worktree of the jak-project repo (never the primary checkout used as
a shared reference; this tool temporarily rewrites and restores decompiler/config there, and
that is only safe against a tree nobody else is reading at the same time). <base_sha> is the
commit to diff against; passing the worktree's own HEAD is the self-test that proves the
restore logic (see below).

Mechanism: decompiler.exe reads all-types.gc and the ntsc_v1/*.jsonc files through
--proj-path, not through the config-path argument, so the only way to decode against a
DIFFERENT commit's config is to actually put that commit's files on disk momentarily. This
tool does that against <repo_root>'s own working tree:

  1. refuse unless `git status --short` in <repo_root> is clean (nothing to safely restore
     to otherwise);
  2. decode the corpus once at the CURRENT (HEAD) config into <scratch>/out-after;
  3. `git checkout <base_sha> -- decompiler/config/<game>` in <repo_root>;
  4. decode again into <scratch>/out-before;
  5. `git checkout HEAD -- decompiler/config/<game>` to restore, then verify `git status
     --short` is clean again; a dirty tree here is a script bug or an interrupted run, not a
     mover to report, and is treated as an operational failure;
  6. diff every "<obj>_ir2.asm" file's ";; ERROR:" count between the two output dirs.

Both decodes go through a scratch proj dir (<scratch>/proj) whose decompiler/config is
junctioned to <repo_root>'s own, per the standing rule that a decompiler run's --proj-path
must never point at a real tree directly (it stubs out/<game>/fr3/GAME.fr3 and rewrites the
output dir's bookkeeping under whatever --proj-path it is given).

Exit codes: this script's OWN operational failures (dirty tree at the start, a checkout that
fails, a restore that does not come back clean, decompiler.exe returning nonzero) exit
nonzero with a clear message. A successful run that finds movers still exits 0: whether a
mover is "explained" by the commits under test is a judgment call for whoever is reading the
printed table, not something this script can decide, so it always prints the table (movers,
plus any object present in only one of the two corpora) and lets the caller judge.
"""
import argparse
import os
import re
import subprocess
import sys

RE_ERR = re.compile(r"^;; ERROR:", re.M)


def run(cmd, cwd=None, check=True):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if check and r.returncode != 0:
        raise RuntimeError(f"command failed ({r.returncode}): {' '.join(cmd)}\ncwd={cwd}\n{r.stdout}\n{r.stderr}")
    return r


def git_status_short(repo_root):
    r = run(["git", "-C", repo_root, "status", "--short"])
    return r.stdout


def make_junction(link, target):
    if os.path.exists(link):
        return
    r = subprocess.run(["cmd", "/c", "mklink", "/J", os.path.normpath(link), os.path.normpath(target)],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"mklink /J failed: {link} -> {target}\n{r.stdout}\n{r.stderr}")


def error_counts(out_dir, game):
    """{object: error_count} from every <obj>_ir2.asm under out_dir/<game>."""
    d = os.path.join(out_dir, game)
    counts = {}
    if not os.path.isdir(d):
        return counts
    for f in os.listdir(d):
        if not f.endswith("_ir2.asm"):
            continue
        obj = f[: -len("_ir2.asm")]
        text = open(os.path.join(d, f), encoding="utf-8", errors="replace").read()
        counts[obj] = len(RE_ERR.findall(text))
    return counts


def decode(decompiler_exe, config_path, iso_data, out_dir, proj_path, version, game):
    os.makedirs(out_dir, exist_ok=True)
    cmd = [
        decompiler_exe, config_path, iso_data, out_dir,
        "--version", version,
        "--proj-path", proj_path,
        "--disable-ansi",
        "--config-override",
        '{"levels_extract": false, "disassemble_code": true, "decompile_code": true, "allowed_objects": []}',
    ]
    r = run(cmd, check=False)
    if r.returncode != 0:
        raise RuntimeError(f"decompiler.exe exited {r.returncode} decoding into {out_dir}\n"
                            f"{r.stdout[-4000:]}\n{r.stderr[-4000:]}")
    return r


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("repo_root")
    ap.add_argument("base_sha")
    ap.add_argument("decompiler_exe")
    ap.add_argument("--scratch", required=True)
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--version", default="ntsc_v1")
    ap.add_argument("--iso-data", default=None, help="default: <repo_root>/iso_data")
    args = ap.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    scratch = os.path.abspath(args.scratch)
    iso_data = os.path.abspath(args.iso_data) if args.iso_data else os.path.join(repo_root, "iso_data")
    game = args.game
    config_rel = f"decompiler/config/{game}"
    config_path = os.path.join(repo_root, config_rel.replace("/", os.sep), f"{game}_config.jsonc")

    status = git_status_short(repo_root)
    if status.strip():
        print(f"refusing: {repo_root} is not clean; nothing safe to restore to\n{status}", file=sys.stderr)
        return 2

    proj = os.path.join(scratch, "proj")
    os.makedirs(os.path.join(proj, "decompiler"), exist_ok=True)
    make_junction(os.path.join(proj, "decompiler", "config"), os.path.join(repo_root, "decompiler", "config"))

    out_after = os.path.join(scratch, "out-after")
    out_before = os.path.join(scratch, "out-before")

    print(f"decoding at HEAD config -> {out_after}")
    decode(args.decompiler_exe, config_path, iso_data, out_after, proj, args.version, game)

    print(f"checking out {args.base_sha}:{config_rel} into the working tree")
    run(["git", "-C", repo_root, "checkout", args.base_sha, "--", config_rel])

    try:
        print(f"decoding at {args.base_sha} config -> {out_before}")
        decode(args.decompiler_exe, config_path, iso_data, out_before, proj, args.version, game)
    finally:
        print(f"restoring HEAD:{config_rel}")
        run(["git", "-C", repo_root, "checkout", "HEAD", "--", config_rel])
        post = git_status_short(repo_root)
        if post.strip():
            print(f"RESTORE FAILED: {repo_root} is not clean after restore\n{post}", file=sys.stderr)
            return 2
        print("restore verified clean")

    before = error_counts(out_before, game)
    after = error_counts(out_after, game)
    all_objs = sorted(set(before) | set(after))
    movers = [(o, before.get(o), after.get(o)) for o in all_objs if before.get(o) != after.get(o)]
    only_before = sorted(set(before) - set(after))
    only_after = sorted(set(after) - set(before))

    print(f"\n{len(all_objs)} objects in the union, {len(movers)} mover(s)")
    print(f"{'object':40s} {'before':>8s} {'after':>8s} {'delta':>8s}")
    for o, b, a in sorted(movers, key=lambda t: (t[2] or 0) - (t[1] or 0)):
        bs = "-" if b is None else str(b)
        as_ = "-" if a is None else str(a)
        delta = "-" if (a is None or b is None) else str(a - b)
        print(f"{o:40s} {bs:>8s} {as_:>8s} {delta:>8s}")

    if only_before:
        print(f"\nonly in base ({args.base_sha}) corpus, missing from HEAD corpus: {only_before}")
    if only_after:
        print(f"\nonly in HEAD corpus, missing from base ({args.base_sha}) corpus: {only_after}")

    print("\ncorpus_mover_diff: run completed and the tree was provably restored; "
          "whether the movers above are explained by the commits under test is for the "
          "caller to judge from this table, not this script.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
