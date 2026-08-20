"""Build the untyped-extern ledger for typing-factory round 5 (wave 10 tooling lane).

Usage:
  python extern_ledger.py <decode_dir> --root <worktree_root> --out-dir <dir>
  python extern_ledger.py <decode_dir> --jakx-all-types P --jak2-all-types P --jak3-all-types P --out-dir <dir>

<decode_dir> is a full-corpus jakx decompiler output directory (every "<obj>_ir2.asm"
under it, e.g. D:\\jakx-nightly-watch\\decode\\jakx). Read-only: this tool never
regenerates or writes into that directory. --root derives the three all-types.gc paths
as <root>/decompiler/config/{jakx,jak2,jak3}/all-types.gc; pass the three --*-all-types
flags directly to override any of them (jak2/jak3 are optional; a missing one just means
no twin lookups against that game, printed as a warning, not a failure).

WHAT GOES ON THE LEDGER (see extern_corpus.py's module docstring for the exact parsing
and attribution mechanism). Two independent symbol populations, unioned:

  declared-bare: a symbol whose LAST ACTIVE (define-extern ...) in the jakx all-types.gc
    is the literal bare type `function` or `object`. This is the "which placeholders
    exist right now" population (628 measured 2026-08-19; the "681" figure quoted
    elsewhere in project memory is an earlier snapshot, before this worktree's ongoing
    provisional-extern-hygiene sweeps retyped some of them -- both numbers describe the
    same active-only definition, at different points in time). Commented-out remnant
    declarations ("... (define-extern X function) ..." with a ";; " prefix) do NOT
    count toward this population on their own (a commented form has zero effect on
    goalc); they are carried per-entry as informational context only
    (commented_bare_remnant_lines), since a symbol whose ONLY trace is a commented-out
    line is functionally the same as having no declaration at all, which is itself
    worth knowing when deciding whether a fix is a retype or a brand new declaration.

  corpus-blocked: a symbol named directly by a "Function X has unknown type" warning,
    or found by walkback from one of the six indirect classes, anywhere in the decode
    corpus (1088 distinct symbols measured 2026-08-19; project memory's "1,016" context
    figure is the same measurement from an earlier corpus snapshot -- see this tool's
    own printed reconciliation for the exact class-by-class arithmetic).

  A corpus-blocked symbol whose CURRENT jakx all-types.gc active declaration is already
  a real (non-bare) type is excluded from the ledger outright (excluded_stale_resolved):
  the frozen decode dir predates today's all-types.gc, so this is not a bug, it is
  proof the decode dir is stale for that symbol specifically (measured 84 such symbols;
  matches the project's standing "decompiler_out/ is stale until proven otherwise" rule).
  These are still reported, separately, for transparency.

UNBLOCK-VALUE SCORING (the ranking that matters -- see brief). Three tiers, applied in
this strict priority order as a sort key, each computed as the BEST case across every
occurrence of the symbol in the corpus (a symbol that blocks a top-level-login anywhere
gets top priority even if its other occurrences are unremarkable):
  (a) any_top_level_login: does any attributed occurrence of this symbol sit inside a
      "(top-level-login <object>)" function? This is the whole-object-killer class:
      per project memory (jakx-provisional-extern-hygiene), one bad extern used in a
      top-level form can kill that entire object's decode, and every deftype in it.
      Fixing one of these symbols can unblock far more than its own marker count shows.
  (b) max_same_function_cascade: the largest "how many OTHER ;; ERROR: lines (any
      class, not just the seven attribution-eligible ones) share this symbol's
      occurrence's containing function" value across all its occurrences. A high
      cascade count means the function is heavily poisoned, and typing the blocking
      symbol is likely to clear multiple markers in one shot, not just its own.
  (c) distinct_object_count: how many different objects (files) this symbol's
      occurrences span. A symbol blocking many files is higher-leverage to fix than one
      confined to a single file, all else equal.
  Ties broken by symbol name for determinism. Declared-bare-only symbols with zero
  corpus occurrences sort after every corpus-blocked symbol (their (a)/(b)/(c) are all
  the lowest possible value), which is correct: this ledger cannot measure their
  unblock value from the corpus, and ranking them by it would be inventing a number.

TWIN COLUMN: jak2/jak3 all-types.gc, same lookup mechanism (last-active-declaration-
wins), restricted to declarations whose type starts with "(function" (a real spec, not
another bare placeholder or a non-function type). jak3 is preferred as primary when
both games have a twin; if jak2 disagrees with jak3's text, it is kept as `also` on the
entry rather than dropped, since a factory reviewer benefits from seeing the mismatch
up front rather than rediscovering it. A name match is not an identity match: a twin
signature is a hypothesis that extern_verify.py checks against jakx call sites (#515).

Outputs (both deterministically ordered by rank, so a rerun against unchanged inputs
byte-diffs clean):
  <out-dir>/extern_ledger.json   -- full machine-readable ledger (see this module's
                                     build_entry() for the exact per-symbol schema)
  <out-dir>/extern_ledger.md     -- compact human table, top MD_TABLE_LIMIT rows (the
                                     full population is in the json; a 1500+ row
                                     markdown table is not a "compact" table for anyone)
"""
import argparse
import json
import os
import sys

import extern_corpus as ec

MD_TABLE_LIMIT = 200


def scan_corpus(decode_dir):
    """-> (per_symbol dict, class_breakdown dict, totals dict).

    per_symbol[symbol] = {
      "occurrences": [ {object, function, is_top_level_login, error_class, error_line,
                         op_index, attribution_line, same_function_cascade} ... ],
    }
    same_function_cascade is computed per occurrence as (that function's total
    ;; ERROR: line count) - 1 (the brief's "OTHER error lines in the same function").
    """
    per_symbol = {}
    class_breakdown = {}
    totals = {"in_scope_error_lines": 0, "attributed_occurrences": 0,
              "excluded_method_form_lines": 0, "unresolved_in_scope_lines": 0}

    for obj, path in ec.iter_ir2_files(decode_dir):
        blocks = ec.parse_ir2_file(path, obj)
        for b in blocks:
            cascade = max(0, b["error_line_count"] - 1)
            for occ in b["occurrences"]:
                entry = per_symbol.setdefault(occ["symbol"], {"occurrences": []})
                entry["occurrences"].append({
                    "object": obj,
                    "function": b["function"],
                    "is_top_level_login": b["is_top_level_login"],
                    "error_class": occ["error_class"],
                    "error_line": occ["error_line"],
                    "op_index": occ["op_index"],
                    "attribution_line": occ["attribution_line"],
                    "same_function_cascade": cascade,
                })
                totals["attributed_occurrences"] += 1
                class_breakdown[occ["error_class"]] = class_breakdown.get(occ["error_class"], 0) + 1
            totals["excluded_method_form_lines"] += len(b["excluded_method_form"])
            totals["unresolved_in_scope_lines"] += len(b["unresolved"])

    totals["in_scope_error_lines"] = (totals["attributed_occurrences"]
                                       + totals["excluded_method_form_lines"]
                                       + totals["unresolved_in_scope_lines"])
    return per_symbol, class_breakdown, totals


def scan_jakx_declarations(all_types_path):
    """-> (bare_active: {name: (type, line)}, commented: {name: [line, ...]},
           typed_active: {name: (type, line)})"""
    active = ec.parse_active_define_externs(all_types_path)
    commented = ec.parse_commented_bare_externs(all_types_path)
    bare_active = {n: v for n, v in active.items() if v[0] in ("function", "object")}
    typed_active = {n: v for n, v in active.items() if v[0] not in ("function", "object")}
    return bare_active, commented, typed_active


def twin_for(symbol, jak2_active, jak3_active):
    j3 = jak3_active.get(symbol) if jak3_active else None
    j2 = jak2_active.get(symbol) if jak2_active else None
    j3f = bool(j3 and j3[0].startswith("(function"))
    j2f = bool(j2 and j2[0].startswith("(function"))
    if not j3f and not j2f:
        return None
    if j3f:
        primary_game, primary = "jak3", j3
        alt_game, alt, alt_f = "jak2", j2, j2f
    else:
        primary_game, primary = "jak2", j2
        alt_game, alt, alt_f = None, None, False
    twin = {"game": primary_game, "type": primary[0], "line": primary[1]}
    if alt_f:
        twin["also"] = {"game": alt_game, "type": alt[0], "line": alt[1], "matches_primary": alt[0] == primary[0]}
    return twin


def build_entries(per_symbol, bare_active, commented, typed_active, jak2_active, jak3_active):
    """-> (entries: list[dict], stale_resolved: list[dict])"""
    all_symbols = set(per_symbol) | set(bare_active)
    entries = []
    stale_resolved = []

    for symbol in sorted(all_symbols):
        is_corpus_blocked = symbol in per_symbol
        is_declared_bare = symbol in bare_active

        if is_corpus_blocked and not is_declared_bare and symbol in typed_active:
            # corpus said this symbol was blocking, but the CURRENT all-types.gc
            # already gives it a real type: the decode dir predates that fix.
            stale_resolved.append({
                "symbol": symbol,
                "current_type": typed_active[symbol][0],
                "current_type_line": typed_active[symbol][1],
                "occurrence_count": len(per_symbol[symbol]["occurrences"]),
            })
            continue

        occs = per_symbol.get(symbol, {}).get("occurrences", [])
        objects = sorted({o["object"] for o in occs})
        any_tll = any(o["is_top_level_login"] for o in occs)
        max_cascade = max((o["same_function_cascade"] for o in occs), default=0)

        provenance = []
        if is_declared_bare:
            provenance.append("declared-bare")
        if is_corpus_blocked:
            provenance.append("corpus-blocked")

        tier = "top-level-login" if any_tll else ("cascade" if max_cascade > 0 else "isolated")

        entry = {
            "symbol": symbol,
            "provenance": provenance,
            "declared_bare": is_declared_bare,
            "declared_bare_type": bare_active[symbol][0] if is_declared_bare else None,
            "declared_bare_line": bare_active[symbol][1] if is_declared_bare else None,
            "commented_bare_remnant_lines": commented.get(symbol, []),
            "corpus_blocked": is_corpus_blocked,
            "occurrence_count": len(occs),
            "distinct_object_count": len(objects),
            "objects": objects,
            "any_top_level_login": any_tll,
            "max_same_function_cascade": max_cascade,
            "unblock_tier": tier,
            "occurrences": sorted(occs, key=lambda o: (o["object"], o["error_line"])),
            "twin": twin_for(symbol, jak2_active, jak3_active),
        }
        entries.append(entry)

    entries.sort(key=lambda e: (
        0 if e["any_top_level_login"] else 1,
        -e["max_same_function_cascade"],
        -e["distinct_object_count"],
        e["symbol"],
    ))
    for i, e in enumerate(entries, start=1):
        e["rank"] = i

    return entries, stale_resolved


def render_markdown(entries, counts, limit=MD_TABLE_LIMIT):
    lines = []
    lines.append("# Untyped-extern ledger\n")
    lines.append("## Headline counts\n")
    for k, v in counts.items():
        if isinstance(v, dict):
            continue
        lines.append(f"- {k}: {v}")
    lines.append("")
    lines.append("## Error class breakdown\n")
    for k, v in sorted(counts.get("error_class_breakdown", {}).items()):
        lines.append(f"- {k}: {v}")
    lines.append("")
    lines.append(f"## Top {min(limit, len(entries))} by unblock value (of {len(entries)} total)\n")
    lines.append("| rank | symbol | tier | cascade | objects | provenance | twin |")
    lines.append("|---|---|---|---|---|---|---|")
    for e in entries[:limit]:
        twin = e["twin"]
        twin_s = f"{twin['game']}: `{twin['type']}`" if twin else ""
        lines.append(
            f"| {e['rank']} | `{e['symbol']}` | {e['unblock_tier']} | "
            f"{e['max_same_function_cascade']} | {e['distinct_object_count']} | "
            f"{'+'.join(e['provenance'])} | {twin_s} |"
        )
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("decode_dir")
    ap.add_argument("--root", default=None, help="worktree root; derives the three all-types.gc paths")
    ap.add_argument("--jakx-all-types", default=None)
    ap.add_argument("--jak2-all-types", default=None)
    ap.add_argument("--jak3-all-types", default=None)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    if not os.path.isdir(args.decode_dir):
        print(f"decode dir does not exist: {args.decode_dir}", file=sys.stderr)
        return 2

    def resolve(explicit, game):
        if explicit:
            return explicit
        if args.root:
            return os.path.join(args.root, "decompiler", "config", game, "all-types.gc")
        return None

    jakx_path = resolve(args.jakx_all_types, "jakx")
    jak2_path = resolve(args.jak2_all_types, "jak2")
    jak3_path = resolve(args.jak3_all_types, "jak3")
    if not jakx_path or not os.path.isfile(jakx_path):
        print(f"jakx all-types.gc not found (pass --root or --jakx-all-types): {jakx_path}", file=sys.stderr)
        return 2

    print(f"scanning corpus: {args.decode_dir}")
    per_symbol, class_breakdown, totals = scan_corpus(args.decode_dir)

    print(f"scanning jakx declarations: {jakx_path}")
    bare_active, commented, typed_active = scan_jakx_declarations(jakx_path)

    jak2_active = None
    jak3_active = None
    if jak2_path and os.path.isfile(jak2_path):
        jak2_active = ec.parse_active_define_externs(jak2_path)
    else:
        print(f"WARNING: no jak2 all-types.gc ({jak2_path}); jak2 twins skipped", file=sys.stderr)
    if jak3_path and os.path.isfile(jak3_path):
        jak3_active = ec.parse_active_define_externs(jak3_path)
    else:
        print(f"WARNING: no jak3 all-types.gc ({jak3_path}); jak3 twins skipped", file=sys.stderr)

    entries, stale_resolved = build_entries(per_symbol, bare_active, commented, typed_active,
                                             jak2_active, jak3_active)

    twin_count = sum(1 for e in entries if e["twin"])
    tier_a_count = sum(1 for e in entries if e["any_top_level_login"])
    overlap_both = sum(1 for e in entries if len(e["provenance"]) == 2)

    counts = {
        "corpus_blocked_symbols": len(per_symbol),
        "declared_bare_symbols": len(bare_active),
        "overlap_both_provenance": overlap_both,
        "stale_resolved_excluded": len(stale_resolved),
        "total_ledger_symbols": len(entries),
        "twin_count": twin_count,
        "tier_top_level_login_count": tier_a_count,
        **totals,
        "error_class_breakdown": class_breakdown,
    }

    os.makedirs(args.out_dir, exist_ok=True)
    json_path = os.path.join(args.out_dir, "extern_ledger.json")
    md_path = os.path.join(args.out_dir, "extern_ledger.md")

    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump({
            "generated_from": {
                "decode_dir": os.path.abspath(args.decode_dir),
                "jakx_all_types": os.path.abspath(jakx_path),
                "jak2_all_types": os.path.abspath(jak2_path) if jak2_path else None,
                "jak3_all_types": os.path.abspath(jak3_path) if jak3_path else None,
            },
            "counts": counts,
            "entries": entries,
            "stale_resolved_excluded_symbols": sorted(stale_resolved, key=lambda s: s["symbol"]),
        }, fh, indent=1)

    with open(md_path, "w", encoding="utf-8") as fh:
        fh.write(render_markdown(entries, counts))

    print(json.dumps(counts, indent=1))
    print(f"\n-> {json_path}")
    print(f"-> {md_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
