"""Boot-smoke a worktree's jakx build for N seconds in -debug mode and print the
standing verdict signals.

Usage: python boot_smoke.py <seconds> <logfile> <worktree> [--gk-exe PATH | --bin-dir DIR]

Verdict signals (the standing rule for stack-tip smokes): crash markers == 0,
"[link and exec]" count (663 on develop at the time this was written; that
number grows as more objects land, so read the printed count rather than
assuming), "Adding level" seen at least once, a campath heartbeat present, and
"index 56" printed twice. The link/adding/index56 thresholds (600 / 1 / 2) and
the zero-crash-marker rule are carried over unchanged from the original
smoke240.py; do not edit them to make a run pass.

The campath signal is a DELIBERATE change from that original at port time: the
old script computed and printed the campath count but never gated on it, even
though its own docstring always listed "campath heartbeat present" as one of
the verdict signals. That gap was found and closed here: the verdict now also
requires campath >= 1 (presence, not a magnitude threshold, so this does not
add flakiness; measured campath counts on real green runs range from 92 to
684). The reasoning: a boot that reaches the frame loop with a dead camera
heartbeat should not pass, and nothing about "print it but don't gate on it"
was ever a considered decision in the original, just an oversight this port
does not want to carry forward silently.

gk.exe defaults to the PRIMARY checkout's build
(D:\\jak-project\\out\\build\\Release\\bin\\gk.exe), not the worktree's own,
because a gk.exe launched from a new filesystem path stalls on a Windows
Defender firewall prompt the first time anything runs from that path, and the
primary's path has already been through that prompt once. Only pass --gk-exe
or --bin-dir to point at a different binary after staging that path through
the prompt yourself; otherwise the run will hang until someone answers it by
hand. --proj-path makes gk.exe load the WORKTREE's own out/jakx regardless of
which gk.exe binary is doing the running, so the primary-build default does
not mean this tests the primary's own state.

This boots the real game runtime. Per house lane rules that is
jakx-runtime-observer's job, not a landing lane's; a landing/orchestration
lane should dispatch this script to that agent rather than invoke it directly.
"""
import argparse
import os
import re
import subprocess
import sys
import time

DEFAULT_GK = r"D:\jak-project\out\build\Release\bin\gk.exe"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("seconds", type=int)
    ap.add_argument("logfile")
    ap.add_argument("worktree", help="windows-style path, passed to gk.exe as --proj-path")
    ap.add_argument("--gk-exe", default=None, help="explicit gk.exe path, overrides --bin-dir and the default")
    ap.add_argument("--bin-dir", default=None, help="directory containing gk.exe")
    args = ap.parse_args()

    if args.gk_exe:
        gk_exe = args.gk_exe
    elif args.bin_dir:
        gk_exe = os.path.join(args.bin_dir, "gk.exe")
    else:
        gk_exe = DEFAULT_GK

    gk_args = [gk_exe, "-v", "--game", "jakx", "--proj-path", args.worktree,
               "--", "-boot", "-fakeiso", "-debug"]
    with open(args.logfile, "w", encoding="utf-8", errors="replace") as log:
        p = subprocess.Popen(gk_args, cwd=args.worktree, stdout=log, stderr=subprocess.STDOUT)
        t0 = time.time()
        try:
            p.wait(timeout=args.seconds)
            print(f"gk exited early after {time.time() - t0:.0f}s, code {p.returncode}")
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            print(f"ran the full {args.seconds}s window")

    text = open(args.logfile, encoding="utf-8", errors="replace").read()
    pl = [l for l in text.splitlines() if "project path" in l]
    print("PROJ-PATH:", pl[0].strip() if pl else "NOT FOUND")
    # "assert" is word-bounded (case-insensitive) so a substring inside an unrelated
    # word (a symbol name, a path, an unrelated log line) cannot fire a false crash marker.
    markers = re.findall(r"(?i)crash report|\bassert\b|Unknown mips2c|kmalloc fail|unmapped object|rip=0x", text)
    links = len(re.findall(r"\[link and exec\]", text))
    adding = len(re.findall(r"Adding level", text))
    campath = len(re.findall(r"campath", text))
    idx56 = len(re.findall(r"index 56", text))
    print(f"crash_markers={len(markers)} link_lines={links} adding_level={adding} campath={campath} index56={idx56}")
    if markers:
        print("first marker context:")
        i = text.lower().find(markers[0].lower())
        print(text[max(0, i - 400):i + 600])
    # campath >= 1 gates on presence, not magnitude, so it does not add flakiness
    # (measured campath counts on real green runs range from 92 to 684); a boot
    # that reaches the frame loop with a dead camera heartbeat should not pass.
    ok = not markers and links >= 600 and adding >= 1 and campath >= 1 and idx56 >= 2
    print("VERDICT:", "green" if ok else "RED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
