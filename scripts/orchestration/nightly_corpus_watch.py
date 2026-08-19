"""Nightly drift watch: run the full jakx offline suite single-threaded and a full-corpus
decode, compare per-object ";; ERROR:" counts against the most recently stored snapshot, and
report drift as markdown.

Usage:
  python nightly_corpus_watch.py <worktree> --bin-dir <dir> --state-dir <dir>
      [--offline-test-exe PATH] [--decompiler-exe PATH] [--game jakx] [--version ntsc_v1]

Every external binary is resolvable from an argument: --bin-dir supplies both offline-test.exe
and decompiler.exe by convention (<bin-dir>\\offline-test.exe, <bin-dir>\\decompiler.exe), and
--offline-test-exe / --decompiler-exe override either one individually for a mixed setup.
Nothing here is hardcoded to a particular checkout or username, so this can be pointed at any
worktree and dropped into a Windows Task Scheduler job (this script does not register one
itself; that is the orchestrator's step, not this tool's).

State: <state-dir>/snapshot.json holds the most recent run's per-object error counts and a
timestamp. The FIRST run against a given --state-dir has nothing to compare against: it
bootstraps the snapshot, reports that plainly, and its exit code reflects only whether the
offline suite passed. Every run after that diffs against the stored snapshot, reports movers
(objects whose count went up or down) and any object that appeared or disappeared, then
overwrites the snapshot with the current run's counts, so tomorrow's run diffs against today
rather than against a fixed baseline forever.

Exit code is nonzero when the offline suite failed OR at least one object moved; 0 when the
suite passed and (on a non-bootstrap run) nothing moved. Output is plain markdown on stdout,
with no emoji anywhere, so it is safe to mail, log, or paste into a forge comment unedited.
"""
import argparse
import datetime
import json
import os
import re
import subprocess
import sys

RE_ERR = re.compile(r"^;; ERROR:", re.M)


def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def make_junction(link, target):
    if os.path.exists(link):
        return
    r = subprocess.run(["cmd", "/c", "mklink", "/J", os.path.normpath(link), os.path.normpath(target)],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"mklink /J failed: {link} -> {target}\n{r.stdout}\n{r.stderr}")


def run_offline_suite(offline_test_exe, worktree, game):
    iso = os.path.join(worktree, "iso_data", game)
    cmd = [offline_test_exe, "--iso_data_path", iso, "--game", game, "--proj-path", worktree,
           "--num_threads", "1"]
    r = run(cmd)
    text = r.stdout + r.stderr
    passed = bool(re.search(r"^pass!\s*$", text, re.M)) and "Failing files" not in text
    return passed, text


def run_full_decode(decompiler_exe, worktree, scratch, game, version):
    proj = os.path.join(scratch, "proj")
    os.makedirs(os.path.join(proj, "decompiler"), exist_ok=True)
    make_junction(os.path.join(proj, "decompiler", "config"), os.path.join(worktree, "decompiler", "config"))
    out_dir = os.path.join(scratch, "decode")
    os.makedirs(out_dir, exist_ok=True)
    config_path = os.path.join(worktree, "decompiler", "config", game, f"{game}_config.jsonc")
    iso_data = os.path.join(worktree, "iso_data")
    cmd = [
        decompiler_exe, config_path, iso_data, out_dir,
        "--version", version, "--proj-path", proj, "--disable-ansi",
        "--config-override",
        '{"levels_extract": false, "disassemble_code": true, "decompile_code": true, "allowed_objects": []}',
    ]
    r = run(cmd)
    d = os.path.join(out_dir, game)
    counts = {}
    if os.path.isdir(d):
        for f in os.listdir(d):
            if f.endswith("_ir2.asm"):
                obj = f[: -len("_ir2.asm")]
                text = open(os.path.join(d, f), encoding="utf-8", errors="replace").read()
                counts[obj] = len(RE_ERR.findall(text))
    return r.returncode == 0, counts, (r.stdout[-4000:] + r.stderr[-4000:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("worktree")
    ap.add_argument("--bin-dir", default=None)
    ap.add_argument("--offline-test-exe", default=None)
    ap.add_argument("--decompiler-exe", default=None)
    ap.add_argument("--state-dir", required=True)
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--version", default="ntsc_v1")
    args = ap.parse_args()

    worktree = os.path.abspath(args.worktree)
    state_dir = os.path.abspath(args.state_dir)
    os.makedirs(state_dir, exist_ok=True)

    offline_test_exe = args.offline_test_exe or (
        os.path.join(args.bin_dir, "offline-test.exe") if args.bin_dir else None)
    decompiler_exe = args.decompiler_exe or (
        os.path.join(args.bin_dir, "decompiler.exe") if args.bin_dir else None)
    if not offline_test_exe or not decompiler_exe:
        print("need --bin-dir, or both --offline-test-exe and --decompiler-exe", file=sys.stderr)
        return 2
    for exe in (offline_test_exe, decompiler_exe):
        if not os.path.isfile(exe):
            print(f"binary not found: {exe}", file=sys.stderr)
            return 2

    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    snapshot_path = os.path.join(state_dir, "snapshot.json")

    suite_passed, suite_text = run_offline_suite(offline_test_exe, worktree, args.game)
    decode_ok, counts, decode_tail = run_full_decode(
        decompiler_exe, worktree, state_dir, args.game, args.version)

    lines = []
    lines.append(f"# Nightly corpus watch: {now}")
    lines.append("")
    lines.append(f"Worktree: {worktree}")
    lines.append(f"Offline suite (single thread): {'PASS' if suite_passed else 'FAIL'}")
    if not suite_passed:
        lines.append("")
        lines.append("Offline suite tail:")
        lines.append("```")
        lines.append(suite_text[-3000:])
        lines.append("```")
    lines.append(f"Full-corpus decode: {'ok' if decode_ok else 'decompiler.exe FAILED'}, "
                 f"{len(counts)} object(s), {sum(counts.values())} total ;; ERROR: marker(s)")
    if not decode_ok:
        lines.append("")
        lines.append("Decode tail:")
        lines.append("```")
        lines.append(decode_tail)
        lines.append("```")

    had_snapshot = os.path.isfile(snapshot_path)
    movers = []
    only_prev = []
    only_now = []
    if had_snapshot:
        prev = json.load(open(snapshot_path, encoding="utf-8"))
        prev_counts = prev.get("counts", {})
        all_objs = sorted(set(prev_counts) | set(counts))
        for o in all_objs:
            b = prev_counts.get(o)
            a = counts.get(o)
            if b != a:
                movers.append((o, b, a))
        only_prev = sorted(set(prev_counts) - set(counts))
        only_now = sorted(set(counts) - set(prev_counts))
        lines.append("")
        lines.append(f"## Snapshot comparison")
        lines.append(f"Previous snapshot: {prev.get('timestamp', '?')}")
        lines.append(f"Movers: {len(movers)}")
        if movers:
            lines.append("")
            lines.append("| object | before | after | delta |")
            lines.append("|---|---:|---:|---:|")
            for o, b, a in sorted(movers, key=lambda t: (t[2] if t[2] is not None else 0) -
                                                          (t[1] if t[1] is not None else 0)):
                bs = "-" if b is None else str(b)
                as_ = "-" if a is None else str(a)
                delta = "-" if (a is None or b is None) else str(a - b)
                lines.append(f"| {o} | {bs} | {as_} | {delta} |")
        if only_prev:
            lines.append("")
            lines.append(f"Objects only in the previous snapshot (missing now): {only_prev}")
        if only_now:
            lines.append("")
            lines.append(f"Objects only in this decode (new since the previous snapshot): {only_now}")
    else:
        lines.append("")
        lines.append("## Snapshot comparison")
        lines.append(f"No prior snapshot under {state_dir}; this run bootstraps it. "
                     f"{len(counts)} object(s) recorded, nothing to compare yet.")

    json.dump({"timestamp": now, "counts": counts}, open(snapshot_path, "w", encoding="utf-8"), indent=1)
    lines.append("")
    lines.append(f"Snapshot written to {snapshot_path}")

    print("\n".join(lines))

    moved = bool(movers) if had_snapshot else False
    if not suite_passed or moved:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
