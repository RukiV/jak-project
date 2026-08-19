# Orchestration tooling

Scripts used to run and manage the jakx bring-up campaign's parallel lanes. Everything here
used to live only in a session scratchpad and died with the session; this directory is the
permanent, parameterized home for it. Every tool takes its paths as arguments (or from a
documented environment variable with a stated default) rather than a hardcoded scratchpad
path, so any of them can be run from any worktree, any machine account, any session.

Nothing here needs anything beyond the Python standard library. The `.sh` scripts are plain
POSIX shell (git-bash on this project's Windows machines); they use `cygpath -w` internally
wherever a Windows-style path has to be handed to a `.exe`.

## The tools

- **run_gates.sh** `<worktree> <base-sha> <log-dir> [--bin-dir <dir>]`: runs the full standing
  landing-gate sequence against a worktree's current tip ((mi), goalc-test
  JakXTypeConsistency, the shadowing/state-inherit/deferred-marker/inert-ledger/method-slot/
  spawn-init checkers, then the full offline suite single-threaded) and prints each gate's
  verdict line. Binaries come from `--bin-dir`, then `$BIN_DIR`, then the worktree's own
  `out/build/Release/bin`.

- **boot_smoke.py** `<seconds> <logfile> <worktree> [--gk-exe PATH | --bin-dir DIR]`: boots
  `gk.exe` in `-debug` mode for a fixed window and prints the standing verdict signals (crash
  markers, link-and-exec count, "Adding level", campath heartbeat, "index 56" twice). The
  thresholds are carried over unchanged from the original smoke test and must not be edited to
  make a run pass. Defaults to the PRIMARY checkout's `gk.exe` because a binary launched from a
  brand new path stalls on a Windows Defender prompt the first time; `--proj-path` still makes
  it load the worktree's own build regardless of which `gk.exe` is doing the running. This
  boots the real runtime, which is jakx-runtime-observer's job under the house lane rules, not
  a landing or orchestration lane's; dispatch this script to that agent rather than run it
  directly from another lane.

- **rebase_lane.sh** `<worktree> <onto-sha>`, with **resolve_append.py** `<path>` and
  **check_commits.py** `<repo_dir> <base>..<tip>`: the validated-rebase trio. `rebase_lane.sh`
  drives `git rebase`, auto-resolving jsonc list-append conflicts through `resolve_append.py`
  (which only writes when the resolved file still parses as JSON with comments stripped;
  anything it cannot prove is left for a human) and regenerating `docs/inert-inventory.md`
  conflicts via the repo's own `scripts/gen_inert_ledger.py --write`. Any other conflicted file
  stops the rebase. At the end it runs `check_commits.py` over the new range, which fails
  loudly on conflict markers or invalid jsonc left behind in any commit in that range, not just
  the tip. `resolve_append.py` and `check_commits.py` are also useful standalone.

- **census.py** `<corpus_out_dir> <repo_root> <label> [--out-dir DIR]`: a self-contained port
  of the old census3.py plus its landed.py/leverage.py imports. Classifies every object in a
  full-corpus decode directory as landed, stub, or verified-empty (a stub-shaped file whose
  only content is the literal `;; No code!` landed convention), tags it with a coarse engine
  band, and writes `census-<label>.json` plus a per-band table of unlanded objects to stdout.
  Read-only against both inputs.

- **corpus_mover_diff.py** `<repo_root> <base_sha> <decompiler_exe> --scratch <dir>`: the
  standard mover-diff procedure. Decodes the full corpus (`allowed_objects: []`) at the
  worktree's current config, temporarily checks out `<base_sha>`'s
  `decompiler/config/<game>` into the SAME worktree, decodes again, restores, and diffs
  per-object `;; ERROR:` counts. Refuses to start against a dirty tree and verifies the tree
  is clean again after the restore; an interrupted or failed restore is an operational failure
  (nonzero exit), never silently swallowed. A successful run always exits 0 regardless of how
  many movers it found: whether a mover is "explained" by the commits under test is for
  whoever reads the printed table to judge, not something this script can decide.

- **baselines.py** `<worktree> [--json OUT.json]`: runs the worktree's own
  `check_method_slots.py`, `check_spawn_init.py` and `check_state_inherit.py`, parses their
  verdict lines, and emits the counts as json plus a one-line summary. Exists so a lane brief
  states its baseline by running this against the worktree in question, instead of hand-copying
  a number from a previous lane's report (which is exactly how stale numbers propagate).

- **lane_worktree.py** `create <name> <branch> [<base>]` / `remove <name> [--force]`: creates
  or removes a lane worktree under `<primary>/.worktrees/<name>`, including the
  `iso_data\jakx` and `decompiler_out` junctions to the primary's own copies (via `cmd /c
  mklink /J` with absolute paths; relative paths fail from a bash-style invocation) and an
  `out\jakx` robocopy seed from the primary so the first `(mi)` is incremental. `remove`
  unlinks every junction under the tree before calling `git worktree remove`, so neither a
  plain recursive delete nor `git worktree remove` itself ever walks into a junction and
  touches the primary's real files; it reports (does not fail on) a locked husk directory
  Windows sometimes leaves behind, and verifies the primary's own copies are untouched
  afterward. It does not check whether a live agent still has the worktree open; that check
  belongs to whoever is driving the removal, not to this mechanical script.

- **nightly_corpus_watch.py** `<worktree> --bin-dir <dir> --state-dir <dir>`: runs the full
  offline suite single-threaded and a full-corpus decode, diffs the decode's per-object error
  counts against the most recent snapshot under `--state-dir`, and prints a markdown drift
  report. The first run against a given state dir bootstraps the snapshot (nothing to compare
  yet); every run after that reports movers and updates the snapshot to the current run, so the
  next run diffs against today rather than a fixed baseline forever. Exits nonzero when the
  suite fails or anything moved. No emoji anywhere and every binary path is an argument, so
  this is safe to drop into a Windows Task Scheduler job; this tool does not register one
  itself.

## Standard lane cascade

1. `lane_worktree.py create <name> <branch> <base>` off the current stack tip.
2. The lane does its work in that worktree.
3. `rebase_lane.sh <worktree> <stack-tip-sha>` before merge, if the stack has moved.
4. `run_gates.sh <worktree> <base-sha> <log-dir>` and, once it is green, dispatch
   `boot_smoke.py` to jakx-runtime-observer for anything that touched the boot path.
5. Push, file the PR, get it merged.
6. `lane_worktree.py remove <name>` once nothing still has the worktree open.

## Baseline refresh rule

Never hand-copy a baseline number (method-slots, spawn-init, state-inherit) from an older lane
report into a new one. Run `baselines.py` against the worktree the new lane will actually use,
at the base commit it will actually build from, and quote what it printed. A checker's own
backlog count changes as lanes land work; a stale copied number is indistinguishable from a
real regression until someone re-derives it by hand, which is the whole reason this script
exists.
