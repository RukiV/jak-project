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

## Captures

In-game captures come from the game's own GPU readback (F2, or `(pc-screen-shot)`
from the REPL) and land in `%APPDATA%\OpenGOAL\jakx\screenshots`. Desktop screen
captures are not evidence; see AGENTS.md rule 6 for which capture answers which
question and what the caption must name.
