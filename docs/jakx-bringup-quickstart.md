# Jak X bring-up quickstart

**Status:** Living document for the bring-up era. Everything here was executed and
verified on 2026-08-07/08; if a step stops matching reality, fix the document in the
same change that changed the behavior.

What this covers: getting a fresh checkout or a worktree from zero to a booting,
verifiable Jak X. It exists because every one of these steps has silently failed for
someone; AGENTS.md carries the policy, this file carries the operations.

## Prerequisites

- `iso_data/jakx/` populated from your own disc dump (CGO/CNF/DGO trees plus `SBK/`)
- [Task](https://taskfile.dev) installed; prefer `task` targets over raw cmake/ninja
- `task --list-all`, never `task --list`: the latter hides every `set-game-*` and
  `set-decomp-*` target because they declare no `desc:`
- Select the game and region once: `task set-game-jakx`, `task set-decomp-ntscv1`;
  `task settings` shows what is persisted

## First build, in order

```text
task gen-cmake-release   # configure
task build-release       # C++: gk, goalc, decompiler
task extract             # decompiler over iso_data; ALSO produces out/jakx/fr3/*.fr3
```

Then GOAL, via the REPL: run `task repl` and evaluate `(mi)`. Two couplings that are
not obvious:

- **`(mi)` and `gk` build different halves.** Rebuilding gk.exe does nothing to GOAL
  code; after editing any `.gc`, run `(mi)` or the boot runs the previous GOAL build
  while looking current. The coupling cuts the other way too: after any merge that
  lands mips2c C++ (`game/mips2c/jakx_functions/*.cpp`), a gk.exe built before it
  dies at boot link with `mips2c function <name> is unknown`
  (`mips2c_table.cpp`, a fatal assert). Rebuild gk after pulling such a merge.
- **Every jakx decompiler run stubs `out/jakx/fr3/GAME.fr3`.** Re-run `task extract`
  to completion before booting, or gk dies on a `!tex->is_placeholder` assert.

## Booting, and the fresh-checkout assert

`task boot-game` runs `gk -v --game jakx -- -boot -fakeiso -debug` and parks at the
lever level. On a fresh checkout it dies first at
`game/overlord/jak3/iso.cpp:894`, `ASSERT(mbx_cmd->file_def)`: `(mi)` builds no
soundbanks, and the jak3 overlord that Jak X borrows asserts on the missing `.sbk`.

Cure: copy the sound files into the fakeiso directory. Minimum is one file,
`iso_data/jakx/SBK/COMMON.SBK` into `out/jakx/iso/`; copying all of `SBK/` plus the
other sound-adjacent files from a known-good tree's `out/jakx/iso` matches what a
long-lived checkout accumulates. This is a build-config gap, not a data gap; the
sound files exist in `iso_data`, nothing copies them.

## Worktrees: the full recipe

The standard flow puts feature branches in `.worktrees/<name>`, but a worktree is a
fresh checkout for every purpose above, plus one trap of its own.

**The trap: every binary resolves the primary checkout unless told otherwise.** gk,
goalc, and the decompiler default to walking up from the executable path, and that
walk canonicalizes through junctions, so even a junctioned `out/build` inside the
worktree resolves `D:\jak-project`. Verification then silently tests the wrong build
with every tool reporting success (#47 and #32 track the tooling fix; goalc-test now
accepts --proj-path like goalc, gk and offline-test). Always pass the flag:

```text
goalc --user-auto --game jakx --proj-path <worktree>    # (mi) against the worktree
gk -v --game jakx --proj-path <worktree> -- -boot -fakeiso -debug
```

gk's `--proj-path` is real even though the first `--help` screen truncates before it.
The boot log's `Using development repo path:` line names the resolved root; read it.

Setup, from the repo root (junctions share the immutable inputs; `rmdir` on a
junction unlinks without touching the target, and unlink them before
`git worktree remove`):

```text
git worktree add .worktrees/<name> -b <type>/<slug> develop
cd .worktrees/<name>
mklink /J iso_data\jakx      D:\jak-project\iso_data\jakx     (cmd)
mklink /J decompiler_out     D:\jak-project\decompiler_out    (cmd)
goalc --user-auto --game jakx --proj-path <worktree>   then (mi)   # creates out/jakx
mklink /J out\jakx\fr3       D:\jak-project\out\jakx\fr3      (cmd)
copy the sound files into out\jakx\iso  (see the assert above)
```

## Verifying against a running game

- **One listener per boot.** The DECI server accepts exactly one goalc connection per
  gk lifetime; after any disconnect, no new goalc can attach until gk restarts.
- **Attach order matters:** in goalc, `(mi)` before `(lt)`.
- **Every clean goalc exit reboots the target**, including stdin EOF. To leave the
  game running after a poke session, hard-kill goalc; do not let it exit.
- **`format` output from listener-executed code is swallowed.** The readback channel
  is expression values, so probe with value-returning forms, for example
  `(the int (* 1000.0 (-> *time-of-day-context* current-prt-color x)))`.
- **A one-shot poke of `(-> *time-of-day-context* time)` does not stick**; the tick
  process rewrites it every frame. Forcing time of day needs the tick-source poke or
  a temporary force inside `update-time-of-day`.

## Captures

In-game captures come from the game's own GPU readback (F2, or `(pc-screen-shot)`
from the REPL) and land in `%APPDATA%\OpenGOAL\jakx\screenshots`. Desktop screen
captures are not evidence; see AGENTS.md rule 6 for which capture answers which
question and what the caption must name.
