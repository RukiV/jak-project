# Wave 11 extern twin tier: pre-verify, apply, and fixpoint report

Issue 515. Applies the wave 10 twin-held bucket (102 symbols with a real
`(function ...)` signature under the same name in jak2/jak3 all-types.gc, held out
of the wave 10 arity tier by design -- see `extern_propose.py`'s TWIN SKIP section)
through the same empirical fixpoint method the arity tier's own report
(`data/extern-arity-tier/dry-run-report.md`) used as this lane's procedural model:
apply, measure, remove regressors, iterate to zero, never extrapolate.

## Fresh twin count

`extern_ledger.py` regenerated against the wave 10 close decode snapshot
(`D:\jakx-nightly-watch\decode\jakx`, 758 objects, this worktree's own
`decompiler/config/{jakx,jak2,jak3}/all-types.gc` at branch tip `95c2900ee`, which
already carries the 328 landed arity-tier signatures): 764 corpus-blocked symbols,
1256 total ledger entries, 104 twin-bearing entries overall. `extern_propose.py`
phase 1 (which further restricts to corpus-blocked entries, matching the twin-held
bucket's own definition) found 672 corpus-blocked symbols in scope and **102
twin-held**, byte-identical as a set to the wave 10 buckets.json's own 102 (verified
by direct set comparison, zero added, zero removed): none of the wave 10 twins have
resolved since, and the twin-bearing population held steady across two decode
snapshots and 328 unrelated signature landings.

## Pipeline and disposition

```
102 fresh twin-held
  -> type-gap check (extern_twin.py, new tool this wave)         2 type_gap_held
  -> 100 type-clean candidates
  -> extern_verify.py arity pre-gate (behavior-tag fix applied)  13 rejected_pre
  -> 87 pre-verify survivors (35 clean, 52 no-call-sites)
  -> apply to all-types.gc, corpus_mover_diff fixpoint           15 rejected_fixpoint (iter 1)
  -> 72 survive corpus_mover_diff (0 regressed objects, iter 2/3 confirm)
  -> goalc-test JakXTypeConsistency: PASSED both, 0 overlap with survivors
  -> full offline suite: 1 REF-drift regression                  1 rejected_fixpoint (ref_drift)
  -> 71 applied_survived (FINAL)
```

| bucket | count | sample evidence |
|---|---:|---|
| applied-survived | 71 | see `proposals.json`; net **-41** `;; ERROR:` lines across 20 objects, zero regressions, three gate layers |
| rejected-pre (arity-contradicted) | 13 | `kernel-read-function`: proposed arity 0, observed `{1: 1, 2: 12}` at pskernel:1196/1209/1222/1266 |
| rejected-fixpoint (mover-diff) | 15 | `projectile-update-velocity-space-wars`: own block 1 -> 4, reveals `Could not figure out load` plus 2x unsupported inline-asm `sllv` |
| rejected-fixpoint (ref-drift) | 1 | `draw-debug-text-box`: text.gc REF gains a +217-line junk body, nearly every local bare `none` |
| type-gap-held | 2 | `vehicle-init-by-other`: twin references `traffic-object-spawn-params`, which does not deftype/defenum anywhere in jakx's all-types.gc |

## Tooling extended this wave

Two changes to the merged extern tooling, both hand-checked against real evidence
before being trusted, matching the arity tier's own standard for touching shared
code:

**`extern_verify.py`: `:behavior TAG` arity miscounting, fixed.** 13 of the 102
twin signatures carry a trailing `:behavior TAG` (e.g. `num-func-none`'s own
existing landed declaration, `(function joint-control-channel float float float
float :behavior process)`, a real 4-argument function, not 6). `proposed_arity()`
previously counted every top-level token after `function` including the tag
name/value pair, which would have overcounted every tagged twin's arity by 2 and
produced false contradictions or false clean verdicts. Hand-checked against
`common/type_system/deftype.cpp:762-799`'s real `parse_typespec`: any token
starting with `:` consumes itself and the following token as a tag/value pair,
contributing nothing to the arg list; `:behavior` is the only tag the parser
accepts today. Fixed by stripping `:TAG VALUE` pairs before counting
(`_strip_type_tags`), generically on the `:` marker rather than hardcoding
`"behavior"`, matching the parser's own dispatch. Verified against the two
concrete cases already in this bucket: `num-func-none`-shaped signatures now
count 4, not 6; `vehicle-init-by-other`'s `(function int
traffic-object-spawn-params object :behavior vehicle)` now counts 2, not 4.

**`extern_twin.py`: new tool, the "transplant-verify tier" `extern_propose.py`'s
own TWIN SKIP section named but never built.** Turns a ledger/buckets twin-held
entry into an `extern_verify.py`-shaped candidate, holding out any twin whose
signature references a type jakx's own all-types.gc does not know (`type_gaps()`),
before that candidate ever reaches `extern_verify.py` or `all-types.gc`: a mangled
twin (an unresolvable type reference) is a fatal decompiler type-loader parse
failure for the WHOLE file, not a per-symbol problem, so this check has to run
first. `KNOWN_TYPES` is every `deftype`/`defenum` in jakx's all-types.gc, unioned
with the fixed compiler-bootstrap set that is never deftype'd anywhere (hand-checked
against `common/type_system/TypeSystem.cpp` lines 36-37 and 1085-1276, confirmed
empirically too: `float`/`int`/`object`/etc. all return zero `deftype` hits). First
pass wrongly flagged 9 real jakx types (`bucket-id`, `collide-status`,
`gui-status`, `speech-type`, `font-color`) as gaps because they are `defenum`, not
`deftype`; caught by hand-checking against jakx's own already-landed evidence
rather than trusted on the first pass -- `all-types.gc:63922-63930` already carries
a prior lane's own `og:preserve-this` note for `vehicle-init-by-other`'s twin type
stating in so many words that "traffic-object-spawn-params... does not exist in
jakx", independent confirmation the corrected check's two remaining real gaps
(`traffic-object-spawn-params`, `game-save-elt`) are right and the defenum fix
removed exactly the false positives, nothing else.

**A blind spot the type-gap check cannot see, found only by the fixpoint.**
`auto-save-init-by-other` and `auto-save-post` both passed the type-gap check
(`auto-save` deftypes in jakx) but both regressed in fixpoint iteration 1 with
"Type auto-save is not fully defined" -- the type exists but is only
forward-declared, not fully defined. Existence and full-definition are different
questions; this class is a genuine, undocumented gap in `extern_twin.py`'s own
method, left as a `rejected_fixpoint` case rather than silently fixed by a second
static check, since the fixpoint already catches it correctly and a
full-definition prober would be new, unexercised surface with only two data points
behind it.

## Fixpoint

Method: `git checkout 95c2900ee -- decompiler/config/jakx/all-types.gc` restores
pristine, apply the current survivor set (in-place edit for a symbol with an
existing bare declaration, append otherwise -- 2 in-place: `debug-percent-bar`,
`keybd-get-data`; 85 append, in one new block titled `;; wave-11 extern-twin-tier
(issue 515)` at the end of the file, alphabetical, each tagged `;; twin: GAME`),
`corpus_mover_diff.py` against `95c2900ee`, full corpus (`allowed_objects: []`, all
758 objects). Every mover across all three iterations was attributed by content, not
just by count: a positional diff between before/after `;; ERROR:` lines anchored on
"Function SYMBOL has unknown type" removals (own-body), plus a manual trace of any
error appearing with no anchor in the same diff group to whichever applied symbol
the containing function calls (cascade), the same method the arity tier's own report
describes. Two masked-at-the-object-level regressions were only visible at this
finer grain: `draw-prototype-inline-array-shrub` (shrubbery's own object delta was
net -2 despite this +1 regression, offset by an unrelated win in the same file) and
`find-instance-by-name`/`print-collide-stats` (both clear their own call site
cleanly but each reveals a new error one level up their caller, inside drawable,
which also nets negative at the object level).

**Iteration 1 (87 applied):** 23 movers, 5 regressed objects at face value
(collide-cache, pskernel, lightning, game-save, projectile) plus 2 more found only
by re-checking every one of the 18 nominally-improving movers for a masked
regression (drawable, shrubbery). 15 symbols removed (14 own-body, 2 cascade-only,
1 own-body case -- `draw-prototype-inline-array-shrub` -- also masked at the object
level; see `rejected.json` for the full per-symbol evidence).

**Iteration 2 (72 applied):** 20 movers, 0 regressed objects, confirmed by the same
per-mover re-check (one apparent cascade oddity in shrubbery, an unattributed
removed/added pair at different op numbers in the same function,
`draw-drawable-tree-instance-shrub`, resolved by direct before/after per-function
error counts: 1 error before, 1 after, a same-function swap, not a regression).
`(mi)`: Successfully built all 677 targets. `goalc-test JakXTypeConsistency`:
PASSED both, zero overlap between the 50 pre-existing "define-extern has redefined"
warnings in the log and the current survivor set, zero fatal `Compilation Error`.
Full offline suite: 1 failing file (`text`), a REF-drift case corpus_mover_diff and
goalc-test cannot see (see above), traced to `draw-debug-text-box`.

**Iteration 3 (71 applied, final):** `draw-debug-text-box` removed.
`corpus_mover_diff` re-run in full: identical 20-mover table to iteration 2, zero
new movers from the removal itself. `goalc-test JakXTypeConsistency`: PASSED both.
Full offline suite: `pass!`, zero REF diffs.

### Final mover table (iteration 3, every row an improvement)

| object | before | after | delta |
|---|---:|---:|---:|
| actor-link-h | 13 | 8 | -5 |
| game-info | 28 | 24 | -4 |
| logic-target | 8 | 4 | -4 |
| projectile | 31 | 28 | -3 |
| wvehicle-effects | 66 | 63 | -3 |
| drawable | 49 | 47 | -2 |
| light-trails | 126 | 124 | -2 |
| lightning | 11 | 9 | -2 |
| lightning-new | 68 | 66 | -2 |
| shrubbery | 13 | 11 | -2 |
| speech | 23 | 21 | -2 |
| tfrag-near | 2 | 0 | -2 |
| bsp-h | 1 | 0 | -1 |
| cloth | 56 | 55 | -1 |
| credits-cloth | 56 | 55 | -1 |
| lobby-menu-manager-h | 9 | 8 | -1 |
| main | 14 | 13 | -1 |
| pad | 3 | 2 | -1 |
| ragdoll-test | 25 | 24 | -1 |
| vehicle-h | 7 | 6 | -1 |
| **TOTAL** | | | **-41** |

## Baseline reconciliation (pristine vs applied, temp-file swap, never git stash)

`decompiler/config/jakx/all-types.gc` copied to a scratch file, `git checkout
95c2900ee -- decompiler/config/jakx/all-types.gc` for the pristine side, checked,
then the scratch copy restored over the working tree and re-staged (`git add`);
`git diff HEAD` empty afterward, confirming byte-identical restore. Both sides run
with the same invocation:

| check | pristine (95c2900ee) | applied (71 survivors) |
|---|---|---|
| method-slots | 152 game-DGO FAIL(s), 48 level-DGO note(s) | identical |
| spawn-init | 1 game-DGO FAIL(s), 4 level-DGO note(s), 5 dynamic skipped | identical |
| state-inherit | 193 defstates / 1219 linked objects, 0 notes | identical |
| check_extern_defun | 152/11/771/122/170 | identical (extern_twin declarations are decompiler-config-only; check_extern_defun scans goal_src's own inline define-extern forms, a disjoint population this tier never touches, so byte-identical is exact, not a coincidence) |
| check_guard_flip | 0/0/27 DORMANT/0 | identical |

Zero drift on every checker beyond `all-types.gc`'s own diff.

## Gates at final commit state (verbatim)

```
(mi): Successfully built all 677 targets in 5.046s (exit 0)
goalc-test JakXTypeConsistency: [ PASSED ] 2 tests
shadowing (--mode absolute): OK, no conflicting duplicates
state-inherit: OK, 193 defstates against 1219 linked objects, 0 level-order note(s)
deferred markers (--mode diff --base 95c2900ee): OK (no untracked deferrals added)
inert ledger (--check): docs\inert-inventory.md is up to date
method-slots: 152 game-DGO FAIL(s), 48 level-DGO note(s) (byte-identical to pristine)
spawn-init: 1 game-DGO FAIL(s), 4 level-DGO note(s), 5 dynamic site(s) skipped (byte-identical to pristine)
full offline suite (--num_threads 1): pass!
check_extern_defun: 152 game-DGO FAIL(s), 11 level-DGO note(s), 771 total, 122 unresolved, 170 kernel builtin(s) (byte-identical to pristine)
check_guard_flip: 0 game-DGO FAIL(s), 0 level-DGO note(s), 27 DORMANT guard(s) (byte-identical to pristine)
corpus_mover_diff vs 95c2900ee: 20 movers, 0 regressed objects, net -41 error lines
trailing whitespace / conflict markers in the all-types.gc diff: none
```

## Honest final yield

- **71 of 102 fresh twin-held symbols (69.6%) survive** every gate: the type-gap
  check, `extern_verify.py`'s arity pre-gate (with the `:behavior` fix), three
  full-corpus `corpus_mover_diff` iterations with zero regressions, `(mi)`,
  `JakXTypeConsistency` (both tests, zero overlap with pre-existing redefine
  warnings), the full offline suite with zero REF drift, shadowing, state-inherit,
  deferred markers, inert ledger, method-slots/spawn-init/check_extern_defun/
  check_guard_flip (all four confirmed byte-identical pristine vs applied via
  temp-file swap).
- Net corpus yield: **-41 `;; ERROR:` lines** across the full 758-object decode
  corpus, measured directly across three fixpoint iterations, not projected.
- 31 held back total: 2 type-gap (one corroborated by a prior lane's own
  preserved comment on the exact same type), 13 arity-contradicted at pre-verify
  (mostly kernel-level PS2 syscall trampolines and lightning/blerc functions
  actually called at a different arity than their jak2/jak3 twin), 15 mover-diff
  regressions found only by the real decode (a mix of own-body decode failures
  and two cascade-only cases visible only by re-checking nominally-improving
  objects for a masked regression), 1 REF-drift case (a junk-shaped new body,
  the same easy-call class 4 of the arity tier's 5 REF-drift cases were).
- No genuinely-coherent REF-drift case turned up this wave (unlike the arity
  tier's `lobby-start`): `draw-debug-text-box`'s new body is unambiguously
  unnarrowed junk (nearly every local `none`), so no resync candidate is being
  held open here.

## Could-not-verify

- The 6 twin-held symbols with a real landed `defun-debug` body in
  `goal_src/jakx/engine/debug/debug.gc` and `goal_src/jakx/engine/sound/speech.gc`
  (`get-debug-line`, `get-debug-text-3d`, `gui-status->string`,
  `internal-draw-debug-line`, `internal-draw-debug-text-3d`,
  `speech-type->string`) were checked by hand against their real bodies' argument
  lists (all match the twin signature's own argument types exactly; one,
  `internal-draw-debug-text-3d`, proposes a wider `pointer` return where the real
  body returns `draw-string`'s own `draw-string-result`, plausible as a
  compatible supertype but not confirmed identical) and did not appear in
  `goalc-test`'s "define-extern has redefined" warning list. `defun-debug`
  compiles to a real, type-checked lambda only when `*debug-segment*` is true at
  compile time; the standing gates (`(mi)`, `goalc-test`, `offline-test`) all
  compile with it false, so a real conflict under a debug build (`*debug-segment*`
  true) was not directly exercised here, only inferred safe by hand-matching
  every argument type against the real body.
- The 52 pre-verify "no-call-sites" survivors (no `lw t9, SYMBOL(s7)` +
  `jalr ra, t9` pair found anywhere in the corpus for that symbol) were not
  rejected, since nothing in the corpus contradicts them, but their arity is
  genuinely unconfirmed by any call-site evidence; several are known to be
  `:behavior`-invoked (reached via `run-function-in-process`, a different call
  mechanism `extern_verify.py`'s scanner does not follow) or PS2 kernel-level
  functions referenced only as values, not direct calls, which is why they carry
  no signal rather than being a gap in this tier's own search.
- Whether any of the 15 mover-diff-rejected symbols would resolve cleanly with a
  narrower (non-`object`-wide, non-twin) signature was not explored; this tier
  only tests the twin hypothesis as-is, per the brief's own scope.
