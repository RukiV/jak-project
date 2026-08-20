# Wave 10 extern mechanical-arity tier: dry-run report

Issue 515/516. Phase 1 only (generate + verify + measure); `all-types.gc` is
untouched in the final commit, this report is evidence that it was touched only
temporarily, under measurement, and reverted.

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
