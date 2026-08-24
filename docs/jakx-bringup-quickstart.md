# Jak X bring-up quickstart

**Status:** Living document for the bring-up era. Everything here was executed and
verified on 2026-08-07/08/09; if a step stops matching reality, fix the document in
the same change that changed the behavior.

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

`task boot-game` runs `gk -v --game jakx -- -boot -fakeiso -debug`. On a fresh
checkout it dies first at
`game/overlord/jak3/iso.cpp:894`, `ASSERT(mbx_cmd->file_def)`: `(mi)` builds no
soundbanks, and the jak3 overlord that Jak X borrows asserts on the missing `.sbk`.

Cure: copy the sound files into the fakeiso directory. Minimum is one file,
`iso_data/jakx/SBK/COMMON.SBK` into `out/jakx/iso/`; copying all of `SBK/` plus the
other sound-adjacent files from a known-good tree's `out/jakx/iso` matches what a
long-lived checkout accumulates. This is a build-config gap, not a data gap; the
sound files exist in `iso_data`, nothing copies them.

## Boot modes: retail vs freeroam

Past the soundbank assert, a plain boot no longer parks at the bring-up lever level.
`*jakx-boot-mode*` (`goal_src/jakx/engine/level/level-h.gc`, issue 699) defaults to
`'retail`, and `gk -v --game jakx -- -boot -fakeiso` now takes retail's own cold-boot
road end to end: `fmvlev`, the Dolby card (`DOSCREEN.STR`), the THX and INTRO movies,
the menu2-start continue, menu2, and the main menu (`lobby-menu-manager-state-140`
bridges over the still-unlanded profile/memory-card screen). The bring-up want-set
levers (`*jakx-boot-level*` `'icea`, `*jakx-boot-continue*` "ice-icea-1",
`*jakx-boot-task*` "ice-race-task") and the want driver (`*jakx-want-driver*`) all go
dead on a `'retail` boot, and neither the boot-activation camera warp nor main.gc's
boot-time external-cam arm fires.

The THX and INTRO movies need `out/jakx/fmv/THX.MJV` and `out/jakx/fmv/INTRO.MJV` to
exist; `out/` is gitignored and no task target produces them. Generate them from the
disc sources:

```text
python scripts/jakx/gen_mjv.py iso_data/jakx/STR/THX.M2V   -o out/jakx/fmv/THX.MJV
python scripts/jakx/gen_mjv.py iso_data/jakx/STR/INTRO.M2V -o out/jakx/fmv/INTRO.MJV
```

Absent them, `MjvVideoReader::open` just logs `[fmv] <path> not found` and the movie
never plays; it is not a crash.

Add `-freeroam` after the `--` to opt back into the old bring-up boot: the
icea/ice-icea-1/ice-race-task want-set load, the want driver armed, the boot-
activation camera warp, and (under `-debug`) the external cam armed. `-freeroam`
follows `-cam-fly`'s own shape exactly: kmachine.cpp's `InitParms` sets a C++
global, `InitMachineScheme` interns it as `*kernel-boot-freeroam*`, and level-h.gc
reads it once into `*jakx-boot-mode*`.

`-debug` keeps the REPL attached in either mode; on its own it no longer changes
which level or continue point a boot lands on. The old shortcut that swapped in
"menu2-start" under `*debug-segment*` (game-info.gc's `initialize!`) is now scoped
to `'freeroam` boots only, so a `-debug` retail boot still runs the full
Dolby-card/THX/intro chain, exactly like a non-debug retail boot; on retail,
`-debug` exists purely to keep the REPL attached for acceptance work.

A REPL harness attached to a retail boot has no `"cam-warp: released"` line to key
on: that format string only fires from the freeroam want-driver's release path
(`cam-start.gc`'s `jakx-cam-warp-tick`), which a retail boot never reaches. Key on
`"GAMEPLAY: enter fmvlev"` instead (`target-handler.gc`'s `'level-enter` handler,
which fires under either mode on entering the level the event names), or wait out
the harness's full timeout.

**Acceptance discipline:** a `-debug` boot is not a substitute for the `-boot`
smoke on camera or debug plumbing, and the reverse holds too. The two runs differ
in which segments link (DebugSegment is symbol 0 under a plain `-boot` boot,
linked once `-debug` is added; see the debug-segment call class under Static gates
below), so a passing `-debug` acceptance run proves nothing about the
`-boot`-only path.

Known gaps a reader will hit on the retail road: the lobby camera never reaches
`interface-cam-init-by-other`'s repositioning target (`hanginglamp-part-9` lives
in `rustyh`, not resident under menu2) and settles on cam-free-floating at the
main menu (issue 724); the THX and INTRO movies play at about half real-time
(issue 753).

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

**`task extract` carries no such flag at all.** The Taskfile target never forwards
`--proj-path`, so run from inside a worktree it still walks up from the decompiler
binary and resolves the primary checkout through the `decompiler_out` junction,
writing the *primary* checkout's `out/jakx/fr3` while reporting success (proven on
PR #130's evidence trail). Per-checkout settings compound it: `task set-game-jakx`
and `task set-decomp-ntscv1` are persisted per checkout, so a fresh worktree runs
`task extract` as jak1 until both are re-run inside it. Since #126 wired the
extraction slot map into the texture animator, a leaked run then leaves the primary
holding fr3s slotted for the worktree's `texture_slots.cpp`, and the primary's own
gk throws on level load; recover by re-running `task extract` in the primary. The
only extract that actually targets a worktree is the manual decompiler invocation:

```text
<worktree>\out\build\Release\bin\decompiler.exe <worktree>\decompiler\config\jakx\jakx_config.jsonc <worktree>\iso_data <worktree>\decompiler_out --version ntsc_v1 --config-override '{"decompile_code": false, "levels_extract": true, "allowed_objects": []}' --proj-path <worktree>
```

Verify by mtime *and* size that the worktree's own `GAME.fr3` changed, not just that
the command exited 0.

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

Use `mklink /J` (a Windows directory junction) for every link above, not Git Bash's
`ln -s`: the decompiler cannot traverse an `ln -s` link through a second hop, so a
symlinked `decompiler_out` or `iso_data\jakx` fails to resolve with no clear error
(proven during the race-start leg, issue #122).

## Scoped decodes and mips2c ports

Beyond the full `task extract`, two workflows cover the case of decompiling or
porting a handful of objects without touching a live worktree or the primary
checkout.

**Scratch-only scoped decodes.** Never edit a worktree's `decompiler/config` in
place and never junction scratch into a live worktree mid-work. Instead, copy
`decompiler/config` into a scratch directory, edit the copy's `allowed_objects`,
and run the primary `decompiler.exe` with the scratch directory as output and
`--proj-path` pointed at the scratch proj dir. A run scoped to a handful of objects
takes about 2 seconds. The generated mips2c C++ lands between the emitted
`<object>_ir2.asm`'s `;;-*-MIPS2C-Start-*-` markers (proven in issue #133's
factbase and PR #134).

Same junction rule as the worktree setup above applies to the rest of a scratch
tree (decompiler_out, iso_data\jakx): `mklink /J`, not `ln -s` (race-start leg,
issue #122); only the edited `decompiler/config` copy itself needs to be a real
copy rather than a link, since its content changes.

**The mips2c port recipe and its gates.** Issue #133 is the canonical statement.
Generate a function's mips2c section with a scoped decode as above, paste it per
`game/mips2c/readme.md`, then register it: a `CMakeLists.txt` row, the
`// FWD DEC:` declaration, and a `gMips2CLinkCallbacks` row keyed by the object
file name. Land it only once it clears five gates: provenance (the exact scoped
decode command, reproducible), a twin diff line-justified against every available
Jak 1/2/3 port of the same function, a hazard sweep for `Unknown instr`,
`ASSERT(false)`, `PUT_STACK_SIZE_HERE` and any unconsumed `call_addr`, registration
plus a clean link, and a stated behavioural ceiling wherever the path is not yet
reachable. See #133 for the full gate definitions and PR #134 for a worked example.

**The +4 offset trap.** When re-deriving a struct's field offsets from a scoped
decode's `_ir2.asm`, the machine offsets run 4 below the struct's declared offset
for basic-derived types, because the machine offset excludes the basic's type-tag
word that the declared offset counts from. The tell is in the instruction
alignment: an `ld`/`sd` at a machine offset that is 4 mod 8, or an `lq`/`sq` at one
that is 12 mod 16, marks a basic-offset artifact rather than a genuine sub-word
access; add 4 before writing the field into the deftype (race-start leg,
issue #122, rung 1).

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

## Debug menu activation

The debug menu (issue 571's UX successor) is fully landed: the framework
(`goal_src/jakx/engine/debug/menu.gc`), the render bucket (`DEBUG_MENU=795` in
`OpenGLRenderer.cpp:560`), the hook plumbing (`set-master-mode`'s
`menu-respond-to-pause` call, `*menu-hook*`), and the menu content itself
(`goal_src/jakx/engine/debug/default-menu.gc`).

- **Activate from the REPL:**
  ```
  (set-master-mode 'menu)
  (debug-menu-context-send-msg *debug-menu-context* (debug-menu-msg activate) (debug-menu-dest activation))
  ```
- **Pad 0 drives it in-game:** dpad to navigate, X to select, square to back
  out.
- **The L3+Start chord also works** once `*master-mode*` is `'menu`
  (`menu-respond-to-pause`'s own dispatch), without going through the REPL at
  all. L3+Select activates the popup menu instead, and pad 1's Start
  activates the editable-player menu (only while `*editable*` is live).
- **The Continue submenu's press path depends on the net-start landing** (a
  separate lane, R5 of the debug-menu scoping plan); pressing it before that
  lane lands is a held no-op, not a crash.

## Captures

In-game captures come from the game's own GPU readback (F2, or `(pc-screen-shot)`
from the REPL) and land in `%APPDATA%\OpenGOAL\jakx\screenshots`. Desktop screen
captures are not evidence; see AGENTS.md rule 6 for which capture answers which
question and what the caption must name.

## Static gates

**check_debug_segment_calls.py** (wired into the forge lint job) flags a
main-segment call or dereference that reaches a symbol whose only definition
is a plain `defun`/`defbehavior`/`defmethod`/`define` inside a whole-file
`(declare-file (debug))` object; DebugSegment is symbol 0 in every `-boot`
boot, so that symbol has no `-boot`-time value and an unguarded call into it
dispatches to goal 0 (issue 745; `defun-debug`-shaped symbols are exempt,
they always bind to `nothing` under `-boot`). Run it with `python
scripts/check_debug_segment_calls.py --root . --game jakx`; known
pre-existing sites are held in `scripts/check_debug_segment_calls_allow.json`,
keyed by file and callee rather than line, each citing an issue (issue 745,
issue 749).
