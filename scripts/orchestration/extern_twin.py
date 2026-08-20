"""Twin-transplant tier for the untyped-extern ledger (wave 11, issue #515's own
"transplant-verify tier" that extern_propose.py's TWIN SKIP section names but does not
build). Turns a ledger twin-held bucket (see extern_ledger.py's twin_for()) into
extern_verify.py-shaped candidates, holding out any twin whose signature references a
type jakx's own all-types.gc does not know.

Usage:
  python extern_twin.py <buckets.json> <jakx_all_types.gc> --out-dir <dir>

<buckets.json> is extern_propose.py's phase-1 output (this tool reads only the
"twin_held" list: {symbol, twin: {game, type, line, also}}). <jakx_all_types.gc> is
this worktree's own decompiler/config/jakx/all-types.gc (read-only; this tool never
writes to it).

WHY A SEPARATE TYPE-GAP CHECK, BEFORE extern_verify.py EVEN RUNS: a twin signature is a
hypothesis copied from jak2/jak3, and those games' type universes are supersets of
jakx's (more content shipped, more types deftyped). Landing a twin declaration that
names a type jakx's own all-types.gc never deftypes would not fail quietly: the
decompiler's type loader parses the WHOLE file as one pass, and an unresolvable type
reference in one define-extern is exactly the same class of fatal parse failure project
memory already records for a same-name redefinition to a different type (jakx-provisional-
extern-hygiene: "an object-typed extern in top-level arithmetic kills the whole top-level
and every deftype in it"; jakx-phase1-landing-pipeline's "Type redefinition when parsing
decompiler type file" crash). A mangled twin is worse than no twin at all, so this check
runs before any candidate reaches extern_verify.py or all-types.gc, not after a crash
proves it.

KNOWN_TYPES = every "(deftype NAME ..." in jakx's own all-types.gc, UNION the fixed
compiler-bootstrap set that is never deftype'd anywhere (hand-checked against
common/type_system/TypeSystem.cpp's TypeSystem::TypeSystem() and
TypeSystem::add_builtin_types(), lines 36-37 and 1085-1276 of that file, the complete
set those two spans register: "none"/"_type_"/"_varargs_" always-included, then object,
structure, basic, symbol, type, string, function, vu-function, link-block, kheap, array,
pair, connectable, file-stream, pointer, inline-array, number, float, integer,
binteger, sinteger, int8/16/32/64/128, uinteger, uint8/16/32/64/128, meters, degrees,
seconds, int, uint, memory-usage-block). Confirmed empirically too: grepping jakx's
all-types.gc for "^(deftype float " / "^(deftype int " / "^(deftype object " etc. all
return zero -- these never get their own deftype line, they exist purely from the C++
bootstrap, so a deftype-only scan would have wrongly flagged every ordinary function
signature that merely mentions "int" or "float" as a type gap.

A signature's every argument/return type token is walked recursively (a compound type
like "(pointer actor-group)" or "(inline-array prototype-bucket-shrub)" contributes both
its container name, e.g. "pointer", and its parameter, e.g. "actor-group"; both must
resolve), after the same ":TAG VALUE" stripping extern_verify.py's proposed_arity() uses
(see that module for the hand-checked citation), since a tag's value is a type name too
(e.g. ":behavior vehicle" needs "vehicle" to exist, which every deftype'd process/state
owner does by construction) but the ":behavior" keyword token itself is not a type name
to look up.

Outputs (<out-dir>):
  twin_candidates.json  -- extern_verify.py's own input shape, [{symbol, signature}, ...],
                            every twin whose types all resolve; primary game's signature
                            (jak3 preferred, per extern_ledger.twin_for) is what is
                            proposed. Feed this into extern_verify.py next.
  type_gap_held.json    -- {symbol, signature, twin_game, missing_types: [...]} for every
                            twin held out here, with which type(s) failed to resolve.
"""
import argparse
import json
import os
import re
import sys

import extern_corpus as ec
from extern_verify import _strip_type_tags

DEFTYPE_RE = re.compile(r"^\((?:deftype|defenum)\s+(\S+)", re.M)

# Compiler-bootstrap types, never deftype'd anywhere in any game's all-types.gc. See this
# module's own docstring for the exact TypeSystem.cpp citation this was hand-checked
# against, and the empirical zero-deftype-hits confirmation.
KERNEL_BUILTIN_TYPES = {
    "none", "_type_", "_varargs_",
    "object", "structure", "basic", "symbol", "type", "string", "function",
    "vu-function", "link-block", "kheap", "array", "pair", "connectable",
    "file-stream", "pointer", "inline-array", "number", "float", "integer",
    "binteger", "sinteger", "int8", "int16", "int32", "int64", "int128",
    "uinteger", "uint8", "uint16", "uint32", "uint64", "uint128",
    "meters", "degrees", "seconds", "int", "uint", "memory-usage-block",
}


def scan_known_types(all_types_path):
    """-> set of every type name jakx's own all-types.gc deftypes or defenums, unioned
    with KERNEL_BUILTIN_TYPES. defenum matters here as much as deftype: bucket-id,
    collide-status, gui-status, speech-type and font-color are all real jakx types (an
    enum IS a type, usable in an argument/return position) that a deftype-only scan
    wrongly flagged as gaps on the first pass of this check -- caught by hand-checking
    against jakx's own already-landed evidence rather than trusted on the first
    pass: all-types.gc:63922-63930 carries a prior lane's own og:preserve-this note
    for the SAME type this module flags for vehicle-init-by-other
    (traffic-object-spawn-params, not an enum, a genuine gap) stating in so many words
    that "traffic-object-spawn-params... does not exist in jakx", independent
    confirmation this check's remaining two real gaps (traffic-object-spawn-params,
    game-save-elt) are correct and the defenum fix removed exactly the false
    positives."""
    with open(all_types_path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    deftyped = set(DEFTYPE_RE.findall(text))
    return deftyped | KERNEL_BUILTIN_TYPES


def extract_type_names(token):
    """A signature token is either a bare symbol ("int", "actor-group") or a
    parenthesized compound form ("(pointer actor-group)", "(inline-array
    prototype-bucket-shrub)"). Returns every leaf type-name symbol referenced,
    recursively, with ":TAG VALUE" pairs stripped at every nesting level (matching
    extern_verify.proposed_arity's own top-level stripping, generalized: a tag can only
    legally appear where the real deftype.cpp parser allows it, but stripping
    generically on the ':' marker at every level is a safe superset, never a false
    negative)."""
    token = token.strip()
    if token.startswith("(") and token.endswith(")"):
        inner = ec.split_top_level(token[1:-1])
        inner = _strip_type_tags(inner)
        names = set()
        for t in inner:
            names |= extract_type_names(t)
        return names
    return {token}


def type_gaps(signature, known_types):
    """-> sorted list of type names in `signature` that are not in `known_types`.
    Empty list means every referenced type resolves."""
    sig = signature.strip()
    assert sig.startswith("(") and sig.endswith(")"), f"not parenthesized: {signature!r}"
    tokens = ec.split_top_level(sig[1:-1])
    assert tokens and tokens[0] == "function", f"not a function typespec: {signature!r}"
    rest = _strip_type_tags(tokens[1:])
    missing = set()
    for t in rest:
        for name in extract_type_names(t):
            if name not in known_types:
                missing.add(name)
    return sorted(missing)


def build_candidates(twin_held, known_types):
    """-> (candidates: [{symbol, signature}], type_gap_held: [{symbol, signature,
    twin_game, missing_types}])"""
    candidates = []
    gap_held = []
    for entry in twin_held:
        symbol = entry["symbol"]
        twin = entry["twin"]
        signature = twin["type"]
        missing = type_gaps(signature, known_types)
        if missing:
            gap_held.append({
                "symbol": symbol, "signature": signature, "twin_game": twin["game"],
                "twin_line": twin["line"], "missing_types": missing,
            })
        else:
            candidates.append({"symbol": symbol, "signature": signature,
                                "twin_game": twin["game"], "twin_line": twin["line"]})
    return candidates, gap_held


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("buckets_json")
    ap.add_argument("jakx_all_types")
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    with open(args.buckets_json, "r", encoding="utf-8") as fh:
        buckets = json.load(fh)
    twin_held = buckets["twin_held"]

    known_types = scan_known_types(args.jakx_all_types)
    print(f"{len(known_types)} known types (deftype scan + kernel-builtin set)")

    candidates, gap_held = build_candidates(twin_held, known_types)

    os.makedirs(args.out_dir, exist_ok=True)
    cand_path = os.path.join(args.out_dir, "twin_candidates.json")
    gap_path = os.path.join(args.out_dir, "type_gap_held.json")

    with open(cand_path, "w", encoding="utf-8") as fh:
        json.dump(candidates, fh, indent=1)
    with open(gap_path, "w", encoding="utf-8") as fh:
        json.dump(gap_held, fh, indent=1)

    print(f"{len(twin_held)} twin-held symbols: {len(candidates)} type-clean candidates, "
          f"{len(gap_held)} type-gap-held")
    for g in gap_held:
        print(f"  TYPE-GAP {g['symbol']}: missing {g['missing_types']} in {g['signature']!r}")
    print(f"-> {cand_path}")
    print(f"-> {gap_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
