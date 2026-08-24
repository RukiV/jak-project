#!/usr/bin/env bash
# Run the standing landing gates for a worktree at its current tip.
#
# Usage: run_gates.sh <worktree-posix-path> <base-sha> <log-dir> [--bin-dir <dir>]
#
# <worktree-posix-path> is a git-bash style path (/d/jak-project/.worktrees/foo);
# it is converted to a Windows path internally for the .exe invocations.
# <base-sha> is passed straight through to check_deferred_markers.py --base.
# <log-dir> receives the raw goalc/goalc-test/offline-test logs; the summary
# lines below are also printed to stdout so the caller does not have to open
# them for the common case.
#
# Binaries: --bin-dir, then the BIN_DIR environment variable, then the
# worktree's own out/build/Release/bin, in that priority order. Point this at
# a scratch copy of a known-good build (see AGENTS.md / the bring-up
# quickstart) rather than a shared build directory something else might
# rebuild mid-run.
#
# Each gate runs as its own command and its verdict line is printed; nothing
# is chained with a commit. Read every verdict; a missing success line is a
# failure, not a pass by omission.
set -u

if [ "$#" -lt 3 ]; then
  echo "usage: run_gates.sh <worktree> <base-sha> <log-dir> [--bin-dir <dir>]" >&2
  exit 2
fi
WT="$1"; BASE="$2"; LOG="$3"; shift 3

BIN_DIR="${BIN_DIR:-}"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --bin-dir) BIN_DIR="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

cd "$WT" || { echo "cannot cd to worktree: $WT" >&2; exit 2; }

if [ -z "$BIN_DIR" ]; then
  BIN_DIR="$WT/out/build/Release/bin"
fi
if [ ! -f "$BIN_DIR/goalc.exe" ]; then
  echo "no goalc.exe under bin dir: $BIN_DIR (pass --bin-dir or set BIN_DIR)" >&2
  exit 2
fi

WTW=$(cygpath -w "$WT")
mkdir -p "$LOG"

echo "== tip: $(git log -1 --oneline)"

echo "== (mi)"
"$BIN_DIR/goalc.exe" --game jakx --proj-path "$WTW" --cmd "(mi)" > "$LOG/mi.log" 2>&1
grep -m1 "Using project path\|Using explicitly" "$LOG/mi.log"
grep -m1 "Successfully built" "$LOG/mi.log" || echo "MI: NO SUCCESS LINE"

echo "== goalc-test JakXTypeConsistency"
"$BIN_DIR/goalc-test.exe" --proj-path "$WTW" --gtest_filter="JakXTypeConsistency.*" > "$LOG/gtest.log" 2>&1
grep -m1 "Using project path\|Using explicitly" "$LOG/gtest.log"
grep -E "^\[  (PASSED|FAILED)" "$LOG/gtest.log" | head -3

echo "== shadowing"
python scripts/check_alltypes_shadowing.py --mode absolute 2>&1 | tail -1

echo "== state-inherit"
python scripts/check_state_inherit.py 2>&1 | tail -1

echo "== deferred markers"
python scripts/check_deferred_markers.py --mode diff --base "$BASE" 2>&1 | tail -1

echo "== inert ledger"
python scripts/gen_inert_ledger.py --check 2>&1 | tail -1

echo "== method slots"
python scripts/check_method_slots.py 2>&1 | grep -E "^FAIL|^check_method_slots \("

echo "== spawn init"
python scripts/check_spawn_init.py 2>&1 | tail -1

echo "== full offline suite (single thread)"
"$BIN_DIR/offline-test.exe" --iso_data_path "$WTW\\iso_data\\jakx" --game jakx --proj-path "$WTW" --num_threads 1 > "$LOG/offline.log" 2>&1
grep -m1 "Using project path\|Using explicitly" "$LOG/offline.log"
grep -E "Compiled [0-9]+ lines|^pass!|Failing files|FAIL" "$LOG/offline.log" | head -8

echo "== done"
