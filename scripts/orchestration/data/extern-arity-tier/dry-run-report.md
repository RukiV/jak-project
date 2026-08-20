# Wave 10 extern mechanical-arity tier: dry-run and fixpoint report

Issue 515/516. Part 1 below (phase 1) is the original 20-sample dry run:
generate + verify + measure only, `all-types.gc` was touched temporarily under
measurement and reverted, untouched in phase 1's own commits. Part 2 (phase 2,
appended after a coordinator amendment that explicitly superseded the "don't
extrapolate from 20" caveat with a full-batch measurement) lands the real
result: all 490 verified-clean proposals applied and fixpoint-attributed
against the real corpus and real gates, `all-types.gc` DOES change in the
final commit for phase 2, to the 328-symbol survivor set.

## Part 1: phase 1 dry run (superseded by part 2's full-batch measurement)

The 20-sample projection below undercounted the true regression rate (see
part 2): the worst-case cascade-score sample regressed at 50%, but the real
full-batch measurement regressed 155 of 490 (31.6%) via corpus_mover_diff,
composed overwhelmingly of cascade-0 "isolated" symbols the phase-1 hypothesis
guessed would be safer. That hypothesis was wrong; part 2 states the corrected
finding.

## Chain that produced these numbers

1. `extern_ledger.py` regenerated against the WAVE 9 CLOSE decode snapshot
   (`D:\jakx-nightly-watch\decode\jakx`, 758 objects) at this worktree's own
   `decompiler/config/jakx/all-types.gc`: 1082 corpus-blocked symbols, 1585 total
   ledger entries, 104 twin-bearing.
2. `extern_propose.py` phase 1: 998 corpus-blocked symbols in scope (the 84
   `stale_resolved_excluded` symbols are not in the ledger's `entries` list at
   all). 490 candidates, 102 twin-held, 44 behavior-held, 53 arity-contradicted,
   309 no-signal.
3. `extern_verify.py` over all 490 candidates: 490 clean, 0 contradicted, 0
   unparseable, 0 no-call-sites (14m12s wall time, full corpus re-scanned once
   per proposal).
4. `extern_propose.py` phase 2: 490 verified-clean, 0 verify-contradicted, 0
   unexpected verdicts. `proposals.json` is exactly the phase-1 candidate set;
   this tier's own consistency check and extern_verify.py's independent
   full-rescan agreed on every symbol.

## Counting-convention proof (brief's own instruction: hand-check before trusting)

The brief's text read "arity N means N-1 argument slots plus the return", which
does not match `extern_verify.py`'s own `proposed_arity()`:

```
proposed_arity("(function object object none)") == 2
```

(`rest = [object, object, none]`, `len(rest) - 1 == 2`.) That is 2 argument
tokens for an arity-2 proposal, not 1. Hand-checked against a real, already-landed
reference before writing `extern_propose.py`: `extern_corpus.py`'s own docstring
already cites `closest-pt-in-triangle`'s call site at
`collide-cache_ir2.asm:724`, `register_arity == 4` (a0 through a3 each carry
their own `(set! aN ...)` annotation). jak1's landed signature for that exact
symbol (`decompiler/config/jak1/all-types.gc:3154`) is
`(function vector vector matrix vector none)`: four argument tokens for an
observed arity of 4, not three. `extern_propose.py` follows N object tokens for
arity N, matching the landed reference and `proposed_arity()`, not the brief's
literal "N-1" wording. See `extern_propose.py`'s own module docstring for the
same proof, kept alongside the code it governs.

## Round 1: top 20 verified proposals by cascade score, scoped decode

Selected by `(-max_same_function_cascade, -distinct_object_count, symbol)`,
covering 22 objects. `decompiler.exe --config-override
'{"allowed_objects": [...]}'` against a scratch proj junctioned to this
worktree's own `decompiler/config` (per the standing junction recipe), before
decode against the unmodified worktree, after decode with all 20 proposals
appended (2 of the 20 -- `calc-particle-average-color` and `lobby-start` --
already carried an active bare declaration and were edited in place instead of
appended, since the decompiler treats a same-name redefinition to a DIFFERENT
type as a fatal parse error, not "last active wins"; goalc's own compile-time
semantic that the ledger tooling documents does not hold for the decompiler's
type loader, which crashed with `Type redefinition when parsing decompiler type
file` on the first attempt (exit `-1073740791`) until this was corrected).

| object | before | after | delta |
|---|---:|---:|---:|
| ctf-obs | 78 | 78 | 0 |
| drawable | 49 | 49 | 0 |
| helmet | 7 | 7 | 0 |
| keyboard | 20 | 21 | +1 |
| lobby-adventure | 143 | 143 | 0 |
| lobby-clans | 96 | 96 | 0 |
| lobby-games | 46 | 48 | +2 |
| lobby-menu-manager | 153 | 152 | -1 |
| lobby-net-startup | 122 | 122 | 0 |
| lobby-patch | 27 | 28 | +1 |
| lobby-results | 43 | 43 | 0 |
| net-game-mgr | 99 | 99 | 0 |
| net-race | 139 | 139 | 0 |
| net-training | 150 | 154 | +4 |
| pad | 4 | 3 | -1 |
| title-obs | 15 | 13 | -2 |
| vehicle-reticle | 119 | 120 | +1 |
| wvehicle-effects | 66 | 67 | +1 |
| wvehicle-net | 25 | 25 | 0 |
| wvehicle-util | 102 | 102 | 0 |
| wvehicle-weapons-aux | 95 | 96 | +1 |
| wvehicle-weapons-util | 37 | 40 | +3 |
| **TOTAL** | **1635** | **1645** | **+10** |

Object-level net delta flags 8 regressed objects (keyboard, lobby-games,
lobby-patch, net-training, vehicle-reticle, wvehicle-effects,
wvehicle-weapons-aux, wvehicle-weapons-util). That is the brief's literal gate.
Applied at that granularity alone it would over-reject (co-located clean
proposals in the same object) and under-reject (a regression fully offset by an
unrelated clean win in the same object, e.g. `lobby-adventure` and
`lobby-net-startup` net to 0 despite a real regression inside each). Every
symbol's own function block (and, where the symbol's own block cleared cleanly,
every one of its call sites' containing functions) was diffed by content, not
just by count, to attribute each mover to the specific proposal responsible.
That is the table below; it is a strict refinement of the brief's rule applied
at the correct grain, not a different rule.

## Every mover explained (per-proposal attribution)

All 20 proposals clear their OWN "Function X has unknown type" direct error.
What differs is what happens next.

**Fully resolved, zero remaining own-body error (2):**

| symbol | object | before | after |
|---|---|---:|---:|
| `keybd-read-ascii` | pad | 1 | 0 |
| `get-max-password-chars` | lobby-net-startup | 1 | 0 |

**Clean swap, net 0, no cascade regression found (8):** the direct error clears
and is replaced by exactly one different, no-worse blocker in the same body
(often itself a new corpus-blocked candidate for a future round), with no
increase anywhere else the symbol's call sites reach.

| symbol | own-body swap |
|---|---|
| `v-wpn-type->gunmount-config` | "has unknown type" -> `failed type prop at 1: add failed: object <integer -2>` |
| `spawn-intro-hud` | "has unknown type" -> `failed type prop at 4: Called a function, but we do not know its type` |
| `adventure-map-fade-out` | "has unknown type" -> `failed type prop at 23: Could not figure out load` (PLUS a -1 cascade win in `lobby-menu-manager`'s `(anon-function 175 ...)`) |
| `lobby-start` | "has unknown type" -> `invalid function type: function` at op 26 (PLUS a -2 cascade win in `title-obs`'s `(enter target-title)`) |
| `ctf-base-b-spawn` | "has unknown type" -> `invalid function type: function` at op 24 |
| `filter-highscore-venues` | "has unknown type" -> `failed type prop at 16: Could not figure out load` |
| `helmet-hud-spawn` | "has unknown type" -> `failed type prop at 2: Could not figure out load` |
| `get-next-training-task` | "has unknown type" -> `Error while inserting lets: invalid vector subscript` (PLUS a -1 cascade win in `lobby-adventure`'s `(anon-function 58 ...)`) |

**Regressed, rejected (10):** own-body or caller-cascade error count increased.

| symbol | signature proposed | where it regressed | evidence |
|---|---|---|---|
| `training-trans` | `(function object)` | net-training +4 (caller cascade) | `(anon-function 17 net-training)`: 7 -> 11; own call at op 14 unblocks, caller reaches 11 new "Failed store" struct-field writes at ops 79-227 |
| `v-wpn-unpack-msg!` | `(function object object object object none)` | wvehicle-weapons-util +3 (own body) | own block 1 -> 4; 4 new "Failed store" errors on `arg0`/`a0`/`v0` byte writes: the args are struct pointers, not opaque objects |
| `dnas-do-shutdown` | `(function object none)` | lobby-patch +1 (own body) and lobby-net-startup +1 (caller cascade) | own block 1 -> 2, reveals unresolved global `*dnas-is-started*`; caller `(anon-function 91 lobby-net-startup)` 1 -> 2, unrelated "method with id 75 of type process could not be found" |
| `keyboard-control-spawn` | `(function object object object object object)` | keyboard +1 (own body) | own block 1 -> 2, reveals a SECOND untyped extern reached via run-function-in-process at op 30 |
| `init-event-filter-array` | `(function none)` | lobby-games +1 (own body) | own block 1 -> 2, reveals unresolved global `*menu-events-filter-ct*` |
| `refresh-games-lan-or-internet` | `(function none)` | lobby-games +1 (caller cascade) | own block clears clean (1 -> 1); caller `(anon-function 7 lobby-games)` 3 -> 4, unrelated "Unknown symbol: do-pending-operation" |
| `vehicle-reticle-base-trans` | `(function none)` | vehicle-reticle +1 (own body) | own block 1 -> 2, reveals an unresolved s6-relative load plus an unsupported inline-asm instruction |
| `drone-base-post` | `(function none)` | wvehicle-weapons-aux +1 (own body) | own block 1 -> 2, "method with id 64 of type process could not be found" (reported twice) |
| `adventure-task-init` | `(function none)` | lobby-adventure +1 (own body) | own block 1 -> 2, reveals unresolved global `*adventure-task-info-array*`; masked at the object level by `get-next-training-task`'s unrelated -1 in the same file |
| `calc-particle-average-color` | `(function object object object none)` | wvehicle-effects +1 (caller cascade) | own block (drawable) clears clean (1 -> 1); caller `(method 113 wvehicle)` in wvehicle-effects 4 -> 5, unrelated unresolved load |

## Round 2: re-run with the 10 rejected proposals removed

Same 22-object scope, same unmodified baseline, only the 10 surviving proposals
applied (`lobby-start` in place, the other 9 appended).

| object | before | after | delta |
|---|---:|---:|---:|
| lobby-adventure | 143 | 142 | -1 |
| lobby-menu-manager | 153 | 152 | -1 |
| lobby-net-startup | 122 | 121 | -1 |
| pad | 4 | 3 | -1 |
| title-obs | 15 | 13 | -2 |
| every other object in scope | -- | -- | 0 |
| **TOTAL** | **1635** | **1629** | **-6** |

Zero regressed objects. This confirms the 10 kept proposals as clean under the
brief's own object-level gate, not just under this report's finer-grained
attribution.

## Yield

- Round 2's 10 surviving proposals: net -6 markers across the 22-object scope,
  zero regressions, `all-types.gc` reverted and verified clean after each round
  (`git diff --stat` empty both times).
- All 20 of the top-cascade sample retire their own DIRECT "unknown type"
  error (that error class is gone from all 20 symbols' own bodies); only 2 of
  20 reach zero remaining own-body error, 8 of 20 swap to a different, no-worse
  blocker, and 10 of 20 regress.

## Projected full-batch yield (estimate, not measured)

`proposals.json` has 490 verified-clean entries. Their cascade-score
distribution: 440 at 0 (isolated tier, no other error line shares the
symbol's containing function anywhere in the corpus), 27 at 1, 15 at 2, and 8
above 2 (3/2/2/1/1/1 at cascade 3/4/4/5/6/16). This dry run sampled 20 of the
top 23 highest-cascade symbols specifically, i.e. a deliberately worst-case,
heavily-connected tail, not a random sample: high cascade score means "this
function's own body or its callers already have other things going on", which
is exactly what correlates with a downstream regression being reached. Every
regression found traces to that pattern (a struct written field-by-field
through an "object"-typed arg, a second untyped extern one call deeper, an
unresolved global one load deeper).

The brief's own literal framing -- sum of the 490 verified proposals' attributed
ledger occurrences -- is 658 raw `;; ERROR:` lines directly attributed to these
symbols (1.34 per proposal on average). This is the number if every attributed
line simply vanished with no downstream consequence; the dry run above shows
that is not what happens even for the symbols that resolve cleanly (8 of the 10
clean survivors swap to a DIFFERENT line, net 0, not a vanish), so 658 is a raw
upper bound on directly-attributed lines, not a marker-count reduction estimate.

Given that, this report states three numbers, in decreasing order of how much
they should be trusted, not a single validated projection:

- Floor: the DIRECT "unknown type" error class retires from all 490 symbols'
  own bodies the moment their declaration lands, unconditionally (arity_verify
  already proved every call site agrees, before any decompile is run).
- The worst-case sample's 50% (10/20) net-non-regressing rate, applied
  uniformly to all 490, would put a floor of roughly 245 proposals landing
  with zero new error anywhere they touch. This is presented as a
  conservative floor derived from the worst-case tail, not a central estimate.
- Hypothesis, NOT verified here: the 440 isolated-tier (cascade 0) symbols are
  plausibly far less likely to regress than the sampled tail, since "isolated"
  by the ledger's own definition means no other error line shares that
  occurrence's containing function -- there is no independent evidence in this
  report for what fraction of THAT population is clean; a landing lane should
  dry-run a sample of the isolated tier before trusting a number for it.

## Could-not-verify

- No sample of the 440 cascade-0 "isolated" proposals was dry-run; the 50%
  worst-case-sample rate above must not be read as their expected rate.
- The behavior-held (44) and twin-held (102) buckets were not exercised by
  this dry run at all; they are out of scope for this tier by design (see
  `extern_propose.py`'s module docstring for why each is skipped).
- The brief's check for a `:behavior` marker on a symbol's "existing bare
  declaration" is structurally unreachable given how the ledger defines
  "bare" (literally the tokens `function`/`object` only, never a qualified
  form); only the call-site `invalid-function-type` signal was checked. See
  `extern_propose.py`'s module docstring for the full argument.

## Part 2: fixpoint (full batch, coordinator amendment)

Rebased `feat/jakx-extern-arity-tier` onto `origin/develop` (`d7aaf11f7`, Lane
A's `base-menu/menu2-h` merge; `76ca29bb4` was its parent, rebase was clean,
no conflicts, only new files carried forward). Re-checked all 490 proposals
against the post-rebase `all-types.gc`: 17 still bare (in-place edit target),
473 undeclared (append target), 0 already given a real type by Lane A.

### Procedure

Each iteration: `git checkout d7aaf11f7 -- decompiler/config/jakx/all-types.gc`
(pristine base), apply the current survivor set (in-place for symbols with an
existing bare declaration, append otherwise; the fatal-redefinition finding
from part 1 held throughout: goalc's decompiler layer, not just goalc itself,
crashes on a same-name redefinition to a different type, never silently "last
wins"), amend the single evolving `wip(jakx)` commit (chosen over a fresh
commit per iteration since these are not individually meaningful history; the
final commit at fixpoint replaces it), then `corpus_mover_diff.py` against
`d7aaf11f7` with a unique scratch dir per iteration, full corpus
(`allowed_objects: []`, all 758 objects, both directions).

### Iteration 1: apply all 490

81 movers, 52 regressed. Content-level attribution (same method as part 1's
per-symbol block diffing, generalized: own-body block name equals an applied
symbol, or a changed block's BEFORE text calls an applied symbol via
`lw t9, SYMBOL(s7)`) found zero structural drift (no anon-function
renumbering) and zero unattributed regressions: every one of the 52 traced
cleanly to 154 symbols (140 own-body, 14 cascade-only; 18 cascade
occurrences were ambiguous, more than one applied symbol referenced in the
same regressed block, all treated as implicated per "ambiguous, remove
both"). Cascade-score distribution of the 154: 139 at score 0, 1 at each of
1/2/3/4/5/6/16 roughly. **This refutes part 1's own hypothesis** that the
440 cascade-0 symbols would be safer than the sampled tail: 139 of 154
regressions (90%) came from exactly that population. Own-body struct-field
stores and second-degree untyped-extern reveals (the same failure classes
part 1 found by hand) turned out to correlate with the symbol itself, not
with cascade score.

### Iteration 2: apply 336 (154 removed)

41 movers, 1 regressed: `net-mgr-muis` (+1), cleanly attributed to
`set-muis-hostname`'s own body (1 -> 2 errors, a new unresolved blocker
revealed one level in).

### Iteration 3: apply 335 (155 removed) -- ZERO regressions in corpus_mover_diff

40 movers, all improvements (net -86 across the sampled scope at that point).
This is corpus_mover_diff's fixpoint. Before landing on it, `(mi)` and
`goalc-test JakXTypeConsistency` were run for the first time against this
state and surfaced two failure classes corpus_mover_diff cannot see at all
(it only runs decompiler.exe, never goalc):

**Fatal: a real implementation already exists.** `goalc-test` failed both
`JakXTypeConsistency` tests. The fatal one: "define-extern would redefine the
type of symbol set-continue-point-from-task from (function net-world int) to
(function object none)" while compiling `all-types.gc:87775`, the exact line
this tier appended. `goal_src/jakx/engine/net/net-world.gc` has a real
`(defun set-continue-point-from-task ...)`; the wave-9 decode corpus this
symbol was proposed from predates that landing. A proactive grep of ALL 335
then-applied symbols against every `(defun ...)`/`(defbehavior ...)` in
`goal_src/jakx` (not just a reactive fix of the one goalc happened to reach
first, since goalc-test stops at its first fatal error and could be masking
more) found exactly one more: `view-set-active-camera`
(`goal_src/jakx/engine/gfx/view.gc`), which only produced a WARNING
("previously: (function basic basic), now: (function object object)") rather
than a fatal error, but represents the identical root cause (a stale ledger
entry for an already-implemented function) and was removed on the same
principle regardless of severity. A broader sweep for `(define SYM (lambda`
and `(set! SYM (lambda` patterns across all 335 found zero further hits.
**This is a real gap in `extern_ledger.py`'s own methodology**: its
`typed_active` staleness check only compares against `all-types.gc`'s OWN
`define-extern` declarations, never against goal_src's actual function
definitions, so a symbol that already has a real implementation (but whose
extern placeholder was simply never cleaned up) slips through as
"corpus-blocked" when it should have been excluded the same way
`excluded_stale_resolved` already excludes symbols with a stale-but-real
all-types.gc entry. Worth fixing in the ledger tooling itself for the next
wave.

Removed: `set-continue-point-from-task`, `view-set-active-camera` (157
cumulative). Re-ran `goalc-test`: both `JakXTypeConsistency` tests passed.

### REF drift: 5 more removed after the full offline suite

With `JakXTypeConsistency` green, the full offline suite (`offline-test.exe
--num_threads 1`) still failed 5 files: `loader`, `process-drawable`,
`title-obs`, `vehicle-debris`, `vehicle-manager`. corpus_mover_diff cannot see
this class either (offline-test round-trips through goalc against the real
landed `.gc` sources and compares to a checked-in REF; decompiler.exe never
touches landed source at all). Every one attributed cleanly to a single
applied symbol:

| symbol | landed file | shape of the REF diff |
|---|---|---|
| `target-nearest-dist` | loader.gc | an artificial `(the-as none ...)` cast disappears now that the callee is typed with a real (if wide) signature |
| `process-drawable-local-trans-for-joint!` | process-drawable.gc | pure addition: a brand new `(defun ...)` block, local-vars almost entirely bare `none`, ends in an unresolved `(the-as none (call!))` with no arguments |
| `vehicle-debris-spawn` | vehicle-debris.gc | pure addition: a brand new `(defun ...)` block, mixed real and bare-`function`/`none` locals |
| `vehicle-in-level?` | vehicle-manager.gc | pure addition: a brand new `(defun ...)` block, same mixed-quality pattern |
| `lobby-start` | title-obs.gc | a bare `:enter L40` forward label becomes a full, coherent `:enter (behavior () ...)` body (deactivate / mask flags / send-event / a `(lobby-start)` call) |

Per the wave 9b doctrine stated in the amendment (a landed-REF regression
gets the causal proposal REMOVED, not the REF resynced to worse text), all
five were removed rather than resyncing any REF. Four of the five are an easy
call (unnarrowed, junk-shaped new bodies: bare `none` locals, an
argument-less `(call!)`). `lobby-start`'s case is the honest exception: the
new title-obs content reads as genuinely coherent, correct GOAL code, not
junk, and is the strongest candidate in this batch for an actual REF resync.
It was still removed here rather than resynced, because resyncing a REF is
its own landing action ("the dump cycle") that this tier was not briefed to
perform and this session did not have enough hands-on context with that
procedure to run it correctly under time pressure. **Could-not-verify,
flagged rather than decided unilaterally**: whether `lobby-start`'s
title-obs REF should be resynced in a dedicated follow-up rung is left open
for the coordinator or a landing lane with the resync tooling.

Removed: 5 more (162 cumulative, 328 survivors). Re-ran the full gate
sequence once more: offline suite reports `pass!`, zero REF diffs.

### One operational note: an accidental stash pop

Mid-investigation, a `git stash` / `git stash pop` pair (intended to swap in
a pristine baseline all-types.gc for a method-slots comparison) found "no
local changes to save" on the push side but then popped a PRE-EXISTING,
unrelated stash entry that must already have been sitting in this worktree
(camera-fly/warp scratch content unrelated to this task, referencing an
unrelated issue). It landed one unrelated change on
`goal_src/jakx/engine/level/level.gc`. Caught immediately via `git status`,
reverted with `git checkout -- goal_src/jakx/engine/level/level.gc` before
anything was staged or committed; the stash list is empty and the tree was
never left in that state across a commit boundary. Every later baseline
comparison in this report used a safer temp-file swap instead of
`git stash`.

### Fixpoint verification (328 survivors, tip before the final commit)

```
(mi): Successfully built all 649 targets in 4.815s (exit 0)
goalc-test JakXTypeConsistency: [ PASSED ] 2 tests
shadowing (--mode absolute): OK, no conflicting duplicates
state-inherit: OK, 193 defstates against 1219 linked objects, 0 level-order note(s)
deferred markers (--mode diff --base d7aaf11f7): OK (no untracked deferrals added)
inert ledger (--check): docs\inert-inventory.md is up to date
method-slots: 152 game-DGO FAIL(s), 48 level-DGO note(s)
spawn-init: 1 game-DGO FAIL(s), 4 level-DGO note(s), 5 dynamic site(s) skipped
full offline suite (--num_threads 1): pass!
check_extern_defun: 152 game-DGO FAIL(s), 11 level-DGO note(s), 771 total, 122 unresolved, 170 kernel builtin(s)
check_guard_flip: 0 game-DGO FAIL(s), 0 level-DGO note(s), 27 DORMANT guard(s)
trailing whitespace / conflict markers in the all-types.gc diff: none
```

**Baseline reconciliation.** The amendment quoted method-slots as 150/48.
Measured fresh against the pristine `d7aaf11f7` tree (temp-file swap, not
`git stash`): method-slots is 152/48 there too, byte-identical to the
328-survivor state. The 150/48 figure was stale (likely predating some
commit already on `develop` by the time this lane rebased); this lane causes
**zero** method-slots drift, confirmed by a direct pristine-vs-applied diff,
not by trusting either number. spawn-init (1/4) and state-inherit (193/0,
violations 0) matched the amendment's stated baseline exactly.
`check_extern_defun` was diffed pristine-vs-applied directly too: byte
identical (152/11/771/122/170 both sides), confirming the amendment's own
prediction that these signatures, adding no defuns, should not move it.

### Final fixpoint: 3 corpus_mover_diff iterations, 328 survivors

38 movers in the final `corpus_mover_diff` run, every one an improvement, net
**-82** `;; ERROR:` lines across the full 758-object corpus, zero regressions
anywhere. This is the real, full-batch-measured yield, not an extrapolation:

| iteration | applied | corpus_mover_diff regressed objects | additional removals after | cumulative removed |
|---|---:|---:|---|---:|
| 1 | 490 | 52 | -- | 0 |
| 2 | 336 | 1 | -- | 154 |
| 3 | 335 | 0 | 2 real-implementation conflicts (goalc-test) | 155 |
| 4 (final) | 328 | 0 (confirmed) | 5 REF-drift cases (offline-test) | 157 |
| final | 328 | 0 | -- | 162 |

### Honest final yield

- **328 of 490 proposals (66.9%) survive** every gate run in this session:
  arity-verify (part 1), full-corpus mover-diff with zero regressions
  (3 iterations), `(mi)`, `JakXTypeConsistency` (both tests), the full
  offline suite with zero REF drift, shadowing, state-inherit, deferred
  markers, inert ledger, `check_extern_defun` (confirmed unmoved),
  `check_guard_flip` (confirmed unmoved).
- Net corpus yield: **-82 `;; ERROR:` lines** across the full 758-object
  decode corpus, measured directly, not projected.
- 162 rejected, in three evidenced categories (see `rejected.json`):
  155 mover-diff regressions (140 own-body, 14 cascade-only, both classes
  spanning nearly every cascade-score band, not concentrated in the
  high-cascade tail part 1 guessed), 2 real-implementation conflicts (a
  ledger-tooling blind spot: goal_src already had a real defun), 5 landed-REF
  drift cases (4 clearly unnarrowed junk, 1 genuinely plausible improvement
  held back for a dedicated resync review rather than decided here).
- Part 1's extrapolated floor (roughly 245 of 490) undercounted the true
  survivor rate (328). Its worst-case-sample method was directionally
  cautious in the right way but wrong on which population was risky: it
  guessed the high-cascade tail was the danger zone, when in measured fact
  the regression rate was roughly uniform across cascade scores and the
  isolated (cascade-0) population supplied 90% of the mover-diff rejects by
  sheer size, not by elevated risk.
