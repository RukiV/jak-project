"""Arity pre-gate for a batch of proposed extern signatures, before any all-types.gc
landing (the landing gates themselves -- (mi), TypeConsistency, mover diff -- belong to
the landing lane, not this tooling; this only checks a proposal against the corpus).

Usage:
  python extern_verify.py <proposals.json> <decode_dir> [--out OUT.json]

<proposals.json> is a JSON array: [{"symbol": "foo", "signature": "(function int int
none)"}, ...]. Any extra keys per entry (e.g. "rank", "notes") are ignored, so a batch
sliced straight out of an evidence card's own "symbol" plus a factory-round answer
drops in without reshaping.

<decode_dir> is the same read-only full-corpus decode dir the ledger and evidence tools
use. This tool never writes into it.

METHOD: proposed arity is the typespec's own top-level argument count minus the return
type (zero-arg is "(function RETURN)", never "(function () RETURN)", matching the
factory's own convention; a malformed typespec is a hard error for that proposal, not a
silent skip). Observed arity, per call site, is extern_corpus.register_arity: the
argument registers (a0-a3, t0-t3) actually set between the "lw t9, SYMBOL(s7)" load and
the "jalr ra, t9" that calls it, read off each argument instruction's OWN "(set! aN
...)" IR annotation. This tool's first version instead trusted the jalr line's own
"(call! a0-N a1-N ...)" summary annotation, which is the decompiler's aggregate record
of the same thing; that turned out to be the wrong source of truth (see
extern_corpus.register_arity's docstring): a function whose own type propagation has
already failed can leave the jalr's call! summary empty ("(call!)", zero args shown)
while every individual argument-setup instruction two lines above it still carries a
correct "(set! aN ...)" annotation. register_arity reads the reliable source directly;
the call! summary is still carried per site as annotation_arity, for citation only.

EVERY call site in the corpus is checked (unlike extern_evidence.py's capped examples,
which exist to build bounded LLM context: this is a mechanical consistency gate, and a
sampled gate is not a gate). A call site contributes NO signal only when no jalr was
found within extern_corpus.WALKBACK_WINDOW lines of its lw at all; once a jalr is
found, register_arity always returns a real count (0 included), so it is never coerced
or defaulted the way the call! summary alone would have to be.

CONTRADICTION: any call site whose observed arity differs from the proposed signature's
arity. Every distinct observed arity across all of a symbol's call sites is reported
(not just whether any one contradicts), since a function actually called with two
different arities in the real corpus is itself worth flagging regardless of which
proposal is on the table.

Exit code is 0 if every proposal is arity-clean, 1 if any proposal has at least one
contradicting call site (the caller decides what "clean" means for a landing gate; this
tool's only job is to surface the disagreement, loudly, with citations).
"""
import argparse
import json
import sys

import extern_corpus as ec


class ProposalParseError(Exception):
    """A proposed typespec did not parse as (function ARG... RET)."""


def proposed_arity(signature):
    """(function ARG1 ... ARGN RET) -> N. (function RET) -> 0."""
    sig = signature.strip()
    if not (sig.startswith("(") and sig.endswith(")")):
        raise ProposalParseError(f"not a parenthesized form: {signature!r}")
    tokens = ec.split_top_level(sig[1:-1])
    if not tokens or tokens[0] != "function":
        raise ProposalParseError(f"does not start with 'function': {signature!r}")
    rest = tokens[1:]
    if not rest:
        raise ProposalParseError(f"(function) with no return type at all: {signature!r}")
    return len(rest) - 1  # last token is the return type


def observed_arities(decode_dir, symbol):
    """-> (arities: dict[int, list[{object, lw_line, jalr_line, call_bang, annotation_arity}]],
           no_signal: list[{object, lw_line, reason}])

    arities maps an observed arg count to every call site that showed it (citations).
    The count used is extern_corpus.register_arity (read off each argument-setup
    instruction's own "(set! aN ...)" annotation), not the jalr's own "(call! ...)"
    summary: see register_arity's docstring for the real corpus case
    (closest-pt-in-triangle at collide-cache_ir2.asm:724) where the two disagree, the
    summary alone under-reporting a genuine 4-argument call as 0. The summary's own
    reading is still carried per site (as annotation_arity/call_bang) for citation, and
    a site where the two signals disagree is worth a human's attention even when
    register_arity is what this tool trusts for the contradiction verdict.

    A call site contributes NO signal in two cases: no jalr was found within the window
    at all (site["jalr_line_idx"] is None), or a jalr was found but the scanned window
    carries no op-tag annotations whatsoever (site["register_arity"] is None: bare,
    un-decompiled disassembly, real corpus case at collide-cache_ir2.asm:1265 and
    collide-mesh_ir2.asm:3175 -- see register_arity's own docstring). Once register_
    arity returns a real number, 0 included, that is a trustworthy observation: it means
    the decompiler was actively annotating this region and found no argument-register
    assignments, not that nothing was looked at."""
    arities = {}
    no_signal = []
    for obj, path in ec.iter_ir2_files(decode_dir):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
        for site in ec.find_call_sites(lines, symbol):
            if site["jalr_line_idx"] is None:
                no_signal.append({"object": obj, "lw_line": site["lw_line_idx"] + 1,
                                   "reason": "no jalr found within window"})
                continue
            if site["register_arity"] is None:
                no_signal.append({"object": obj, "lw_line": site["lw_line_idx"] + 1,
                                   "jalr_line": site["jalr_line_idx"] + 1,
                                   "reason": "jalr found but no op-tag annotations in window (bare disassembly)"})
                continue
            arities.setdefault(site["register_arity"], []).append({
                "object": obj,
                "lw_line": site["lw_line_idx"] + 1,
                "jalr_line": site["jalr_line_idx"] + 1,
                "call_bang": site["call_bang_text"],
                "annotation_arity": site["arity"],
            })
    return arities, no_signal


def verify_one(symbol, signature, decode_dir):
    try:
        want = proposed_arity(signature)
    except ProposalParseError as e:
        return {"symbol": symbol, "signature": signature, "status": "unparseable",
                "error": str(e)}

    arities, no_signal = observed_arities(decode_dir, symbol)
    if not arities:
        return {"symbol": symbol, "signature": signature, "proposed_arity": want,
                "status": "no-call-sites", "observed_arities": {},
                "no_signal_count": len(no_signal)}

    contradictions = {a: sites for a, sites in arities.items() if a != want}
    status = "contradicted" if contradictions else "clean"
    return {
        "symbol": symbol,
        "signature": signature,
        "proposed_arity": want,
        "status": status,
        "observed_arities": {str(a): len(sites) for a, sites in sorted(arities.items())},
        "contradictions": {
            str(a): sites for a, sites in sorted(contradictions.items())
        } if contradictions else {},
        "no_signal_count": len(no_signal),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("proposals_json")
    ap.add_argument("decode_dir")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    with open(args.proposals_json, "r", encoding="utf-8") as fh:
        proposals = json.load(fh)
    if not isinstance(proposals, list):
        print("proposals.json must be a JSON array of {symbol, signature}", file=sys.stderr)
        return 2

    results = []
    for p in proposals:
        symbol = p["symbol"]
        signature = p["signature"]
        print(f"verifying {symbol}: {signature}")
        results.append(verify_one(symbol, signature, args.decode_dir))

    clean = sum(1 for r in results if r["status"] == "clean")
    contradicted = [r for r in results if r["status"] == "contradicted"]
    no_sites = sum(1 for r in results if r["status"] == "no-call-sites")
    unparseable = [r for r in results if r["status"] == "unparseable"]

    print(f"\n{len(results)} proposal(s): {clean} clean, {len(contradicted)} contradicted, "
          f"{no_sites} with no call sites, {len(unparseable)} unparseable")
    for r in contradicted:
        print(f"  CONTRADICTED {r['symbol']}: proposed arity {r['proposed_arity']}, "
              f"observed {r['observed_arities']}")
        for arity, sites in r["contradictions"].items():
            for s in sites[:3]:
                print(f"    arity {arity} at {s['object']}:{s['jalr_line']} {s['call_bang']}")
    for r in unparseable:
        print(f"  UNPARSEABLE {r['symbol']}: {r['error']}")

    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=1)
        print(f"\n-> {args.out}")

    return 1 if (contradicted or unparseable) else 0


if __name__ == "__main__":
    sys.exit(main())
