"""Propose mechanical wide-arity signatures for corpus-blocked untyped-extern ledger
symbols (wave 10 tooling lane, issues #515/#516). Reuses extern_corpus.py for every
piece of corpus parsing; this module only adds the proposal and bucketing logic on top.

THESIS (see the lane brief): for a symbol whose observed call-site arity is
consistent across every annotated call site, a wide `(function object ... object
RET)` signature at that arity unblocks the decode without any LLM round. Type
NARROWING (object -> a real type) is a later factory pass; this tier only clears the
"unknown type" class of decompiler error.

Usage:
  python extern_propose.py <ledger.json> <decode_dir> --out-dir <dir>

  # phase 2, after running extern_verify.py on <out-dir>/proposal_candidates.json:
  python extern_propose.py <ledger.json> <decode_dir> --out-dir <dir> \
      --verify-results <verify_output.json>

Phase 1 (no --verify-results) scans the ledger's corpus-blocked population, buckets
every symbol into twin_held / behavior_held / arity_contradicted / no_signal /
candidate, and writes:
  <out-dir>/proposal_candidates.json  -- arity-consistent candidates, in extern_verify.py's
                                          own input shape (plus extra evidence fields,
                                          which extern_verify.py ignores) -- feed this
                                          straight into extern_verify.py.
  <out-dir>/buckets.json              -- twin_held, behavior_held, arity_contradicted,
                                          no_signal, with per-entry evidence and counts.
                                          candidate is NOT final here: extern_verify.py
                                          still has to clear every candidate before it
                                          is a real proposal (phase 2's job).

Phase 2 (--verify-results points at extern_verify.py's own --out file) reads back
<out-dir>/proposal_candidates.json, splits it by extern_verify's per-symbol verdict,
and writes the FINAL:
  <out-dir>/proposals.json  -- the verified-clean subset only. This is the tier's
                                actual deliverable.
  <out-dir>/buckets.json    -- rewritten with the verify-contradicted candidates folded
                                into arity_contradicted (tagged "source":
                                "extern_verify", since these disagree with this
                                script's own consistency check -- a real event worth
                                surfacing, not silently reconciling) and any verify
                                "unparseable"/"no-call-sites" verdicts folded into their
                                own buckets, loudly, since neither should occur given
                                how proposal_candidates.json is built.

ARITY-TO-SIGNATURE COUNTING CONVENTION (hand-checked, see below for why this needed
checking at all): a call site whose annotated argument-setup instructions show N
registers used (extern_corpus.register_arity's own return value -- a0..a3, t0..t3,
in canonical order, off the individual "(set! aN ...)" IR annotations, never the
jalr's own "(call! ...)" summary) gets a proposed signature with exactly N "object"
argument tokens followed by one return-type token:

    N=0  ->  (function none)                    or  (function object)  if consumed
    N=2  ->  (function object object none)       or  (function object object object)
    N=4  ->  (function object object object object none)   [...]

This is the SAME arity extern_verify.py's own proposed_arity() computes from a
signature (len(tokens after "function") - 1, the last token always being the return
type), so a candidate built this way is exactly what extern_verify.py needs to see to
call it "clean" against N: proposed_arity() == N reduces to
len([N object tokens, RET]) - 1 == N, which holds by construction.

The lane brief's own framing of this ("arity N means N-1 argument slots plus the
return") does not match this and was not taken on faith; it was checked by hand
against a real, already-landed reference signature before writing this module.
extern_corpus.py's own docstring already cites the concrete corpus example:
collide-cache_ir2.asm's closest-pt-in-triangle call site at line 724, register_arity
== 4 (a0 through a3 each carry their own "(set! aN ...)" annotation). jak1's actual
landed signature for that same symbol (decompiler/config/jak1/all-types.gc:3154) is
"(function vector vector matrix vector none)": four argument tokens, not three. N=4
observed maps to 4 object-typed argument slots, matching this module's convention
and contradicting the brief's "N-1" framing, which would have proposed only 3.
Every candidate this module emits follows the checked convention, not the brief's
literal wording; see the lane report for this note carried up to the orchestrator.

BEHAVIOR SKIP: a symbol is behavior-held when any of its ledger occurrences carries
error_class "invalid-function-type" -- extern_corpus.py's own classifier for the
"Call to run-function-in-process or set-to-run ... invalid function type" detail
text, which is specifically the mechanism used to invoke a process's :behavior
closure. This is the only signal actually reachable from ledger data: the brief also
asks to check "the symbol's existing bare declaration" for a :behavior marker, but a
ledger entry's declared_bare_type is, by the ledger's own construction
(extern_ledger.scan_jakx_declarations restricts bare_active to the literal tokens
"function"/"object"), never anything but one of those two literal tokens -- a bare
declaration can structurally never carry a :behavior qualifier. That half of the
brief's check is unreachable given the current tooling, not silently skipped; it is
recorded here and in the lane report rather than papered over with a check that can
never fire.

TWIN SKIP: any ledger entry carrying a non-null "twin" field is held out entirely
(the separate transplant-verify tier owns it), before arity analysis runs at all.

RETURN-TYPE PROMOTION: an object-consuming call site is detected as a "(set! DEST
v0[-N])" annotation appearing within RETURN_CONSUME_WINDOW physical lines after the
jalr (bounded, not a dataflow trace, matching this codebase's other windowed
heuristics -- see extern_corpus.register_arity's own docstring for the same style of
bound). The jalr's own second annotation line ("-> [v0: TYPE]") is NOT used for this:
it is present after every call regardless of whether the type is real or "none" or
even an unresolved bare type (verified on actor-link-h_ir2.asm's method-set! call,
whose callee is already known to return none and whose own jalr annotation still
prints "-> [v0: none ]"), so it carries no information about whether the CALLER
actually reads the value afterward. Consumption is promoted to "object" the moment
ANY call site of the symbol demonstrates it (a permissive, safe-superset default,
matching this tier's own "wide signature now, narrow later" philosophy: missing a
real consumer by defaulting to none is a compile break waiting to happen, while
declaring "object" where the return is not actually read costs nothing).
"""
import argparse
import json
import os
import re
import sys

import extern_corpus as ec

LW_T9_RE = re.compile(r"\blw\s+t9,\s*(\S+)\(s7\)")  # mirrors extern_evidence.LW_T9_RE
CONSUME_V0_RE = re.compile(r"\(set!\s+\S+\s+v0(?:-\d+)?\)")
RETURN_CONSUME_WINDOW = 6
BEHAVIOR_ERROR_CLASS = "invalid-function-type"


class ProposeError(Exception):
    """Base for this module's own fail-fast errors."""


class VerifyResultsMismatchError(ProposeError):
    """extern_verify.py's own output does not cover the candidates this module wrote
    in phase 1, or covers a symbol this module never proposed. Either the wrong
    verify-results file was passed, or proposal_candidates.json was hand-edited
    between the two phases; either way this is not safe to paper over."""


def _return_consumed(lines, jalr_idx):
    """True if a "(set! DEST v0[-N])" annotation appears within
    RETURN_CONSUME_WINDOW lines after `jalr_idx`, stopping early at the next jalr
    (crossing into a different call's own setup, where a bare "v0" token no longer
    means this call's return value)."""
    hi = min(len(lines), jalr_idx + 1 + RETURN_CONSUME_WINDOW)
    for k in range(jalr_idx + 1, hi):
        if "jalr ra, t9" in lines[k]:
            break
        if CONSUME_V0_RE.search(lines[k]):
            return True
    return False


def collect_symbol_call_evidence(decode_dir, symbols):
    """Single pass over the whole corpus (one read per file) gathering, per symbol:
      arities: dict[arity] = occurrence count (register_arity's own value; never the
        jalr's own "(call! ...)" summary -- see extern_corpus.register_arity)
      consumed: bool, True the moment any call site demonstrates return consumption
      no_signal_count: call sites where register_arity returned None (no jalr found
        in window, or a jalr found but the window carried no op-tag annotations at
        all -- "no signal", never coerced to arity 0; see register_arity's docstring)

    One pass shared across every symbol rather than one search per symbol per file,
    same rationale as extern_evidence.collect_call_site_windows: O(symbols x files)
    is minutes at ledger scale, this is O(files) once.
    """
    target = set(symbols)
    arities = {s: {} for s in target}
    consumed = {s: False for s in target}
    no_signal_count = {s: 0 for s in target}

    for _obj, path in ec.iter_ir2_files(decode_dir):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
        for i, ln in enumerate(lines):
            m = LW_T9_RE.search(ln)
            if not m:
                continue
            sym = m.group(1)
            if sym not in target:
                continue
            jalr_idx = None
            hi = min(len(lines), i + 1 + ec.WALKBACK_WINDOW)
            for j in range(i + 1, hi):
                if "jalr ra, t9" in lines[j]:
                    jalr_idx = j
                    break
            if jalr_idx is None:
                no_signal_count[sym] += 1
                continue
            ra = ec.register_arity(lines, i, jalr_idx, ec.WALKBACK_WINDOW)
            if ra is None:
                no_signal_count[sym] += 1
                continue
            arities[sym][ra] = arities[sym].get(ra, 0) + 1
            if not consumed[sym] and _return_consumed(lines, jalr_idx):
                consumed[sym] = True

    return arities, consumed, no_signal_count


def build_signature(arity, return_type):
    """N object-typed argument tokens followed by one return-type token. See this
    module's docstring for the hand-checked derivation of why N, not N-1."""
    tokens = ["object"] * arity + [return_type]
    return "(function " + " ".join(tokens) + ")"


def classify_entries(entries, arities_by_symbol, consumed_by_symbol, no_signal_by_symbol):
    """-> (candidates, buckets) where buckets = {twin_held, behavior_held,
    arity_contradicted, no_signal}, each a list of {symbol, reason, evidence}."""
    candidates = []
    buckets = {"twin_held": [], "behavior_held": [], "arity_contradicted": [], "no_signal": []}

    for e in entries:
        symbol = e["symbol"]

        if e["twin"]:
            buckets["twin_held"].append({
                "symbol": symbol, "reason": "twin-bearing (transplant-verify tier owns this)",
                "twin": e["twin"],
            })
            continue

        behavior_occs = [o for o in e["occurrences"] if o["error_class"] == BEHAVIOR_ERROR_CLASS]
        if behavior_occs:
            buckets["behavior_held"].append({
                "symbol": symbol,
                "reason": "call-site context carries a :behavior marker "
                          "(invalid-function-type occurrence: run-function-in-process/set-to-run)",
                "occurrences": behavior_occs,
            })
            continue

        arities = arities_by_symbol[symbol]
        if not arities:
            buckets["no_signal"].append({
                "symbol": symbol, "reason": "zero annotated call sites",
                "no_signal_count": no_signal_by_symbol[symbol],
            })
            continue

        if len(arities) > 1:
            buckets["arity_contradicted"].append({
                "symbol": symbol, "reason": "annotated call sites disagree on arity",
                "observed_arities": {str(a): c for a, c in sorted(arities.items())},
                "source": "propose",
            })
            continue

        (arity,) = arities.keys()
        return_type = "object" if consumed_by_symbol[symbol] else "none"
        signature = build_signature(arity, return_type)
        candidates.append({
            "symbol": symbol,
            "signature": signature,
            "arity": arity,
            "return_type": return_type,
            "return_consumed": consumed_by_symbol[symbol],
            "annotated_call_site_count": sum(arities.values()),
            "no_signal_count": no_signal_by_symbol[symbol],
            "rank": e["rank"],
            "unblock_tier": e["unblock_tier"],
            "max_same_function_cascade": e["max_same_function_cascade"],
            "distinct_object_count": e["distinct_object_count"],
            "any_top_level_login": e["any_top_level_login"],
            "objects": e["objects"],
        })

    return candidates, buckets


def run_phase1(ledger, decode_dir, out_dir):
    entries = [e for e in ledger["entries"] if e["corpus_blocked"]]
    symbols = [e["symbol"] for e in entries]

    print(f"phase 1: {len(symbols)} corpus-blocked ledger symbols in scope")
    arities, consumed, no_signal = collect_symbol_call_evidence(decode_dir, symbols)
    candidates, buckets = classify_entries(entries, arities, consumed, no_signal)

    os.makedirs(out_dir, exist_ok=True)
    candidates_path = os.path.join(out_dir, "proposal_candidates.json")
    buckets_path = os.path.join(out_dir, "buckets.json")

    with open(candidates_path, "w", encoding="utf-8") as fh:
        json.dump(candidates, fh, indent=1)

    counts = {
        "corpus_blocked_in_scope": len(symbols),
        "candidates": len(candidates),
        "twin_held": len(buckets["twin_held"]),
        "behavior_held": len(buckets["behavior_held"]),
        "arity_contradicted": len(buckets["arity_contradicted"]),
        "no_signal": len(buckets["no_signal"]),
    }
    with open(buckets_path, "w", encoding="utf-8") as fh:
        json.dump({"counts": counts, **buckets}, fh, indent=1)

    print(json.dumps(counts, indent=1))
    print(f"-> {candidates_path}")
    print(f"-> {buckets_path}")
    print("phase 1 complete: run extern_verify.py on proposal_candidates.json, then "
          "rerun this module with --verify-results to produce the final proposals.json")
    return 0


def run_phase2(out_dir, verify_results_path):
    candidates_path = os.path.join(out_dir, "proposal_candidates.json")
    buckets_path = os.path.join(out_dir, "buckets.json")

    with open(candidates_path, "r", encoding="utf-8") as fh:
        candidates = json.load(fh)
    with open(buckets_path, "r", encoding="utf-8") as fh:
        buckets_doc = json.load(fh)
    with open(verify_results_path, "r", encoding="utf-8") as fh:
        verify_results = json.load(fh)

    by_symbol = {c["symbol"]: c for c in candidates}
    verify_by_symbol = {r["symbol"]: r for r in verify_results}

    missing_from_verify = sorted(set(by_symbol) - set(verify_by_symbol))
    unexpected_in_verify = sorted(set(verify_by_symbol) - set(by_symbol))
    if missing_from_verify or unexpected_in_verify:
        raise VerifyResultsMismatchError(
            f"verify results do not match proposal_candidates.json: "
            f"{len(missing_from_verify)} candidate(s) missing from verify output "
            f"({missing_from_verify[:5]}...), "
            f"{len(unexpected_in_verify)} symbol(s) in verify output not proposed "
            f"({unexpected_in_verify[:5]}...)")

    clean = []
    newly_contradicted = []
    other_verdicts = []
    for symbol, cand in by_symbol.items():
        verdict = verify_by_symbol[symbol]
        status = verdict["status"]
        if status == "clean":
            clean.append({**cand, "verify_observed_arities": verdict["observed_arities"]})
        elif status == "contradicted":
            newly_contradicted.append({
                "symbol": symbol, "reason": "extern_verify.py found a contradicting call site",
                "signature": cand["signature"], "proposed_arity": verdict["proposed_arity"],
                "observed_arities": verdict["observed_arities"],
                "contradictions": verdict["contradictions"],
                "source": "extern_verify",
            })
        else:
            # "no-call-sites" or "unparseable": neither should happen given how
            # proposal_candidates.json is built (every candidate has >=1 annotated
            # call site and a signature this module itself constructed as valid
            # syntax). Loud, not silently folded, if it ever does.
            other_verdicts.append({"symbol": symbol, "verify_status": status, "verdict": verdict})

    buckets_doc["arity_contradicted"] = buckets_doc.get("arity_contradicted", []) + newly_contradicted
    if other_verdicts:
        buckets_doc["unexpected_verify_verdicts"] = other_verdicts
    buckets_doc["counts"]["candidates"] = len(clean)
    buckets_doc["counts"]["arity_contradicted"] = len(buckets_doc["arity_contradicted"])
    buckets_doc["counts"]["verified_clean"] = len(clean)
    buckets_doc["counts"]["verify_contradicted"] = len(newly_contradicted)
    buckets_doc["counts"]["unexpected_verify_verdicts"] = len(other_verdicts)

    proposals_path = os.path.join(out_dir, "proposals.json")
    with open(proposals_path, "w", encoding="utf-8") as fh:
        json.dump(clean, fh, indent=1)
    with open(buckets_path, "w", encoding="utf-8") as fh:
        json.dump(buckets_doc, fh, indent=1)

    print(f"phase 2: {len(clean)} verified-clean, {len(newly_contradicted)} verify-contradicted, "
          f"{len(other_verdicts)} unexpected verdict(s)")
    print(f"-> {proposals_path}")
    print(f"-> {buckets_path}")
    return 0 if not other_verdicts else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ledger_json")
    ap.add_argument("decode_dir")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--verify-results", default=None,
                     help="phase 2: extern_verify.py's own --out file for proposal_candidates.json")
    args = ap.parse_args()

    with open(args.ledger_json, "r", encoding="utf-8") as fh:
        ledger = json.load(fh)

    if args.verify_results is None:
        return run_phase1(ledger, args.decode_dir, args.out_dir)
    return run_phase2(args.out_dir, args.verify_results)


if __name__ == "__main__":
    sys.exit(main())
