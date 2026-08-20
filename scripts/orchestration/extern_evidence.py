"""Build per-symbol evidence cards for the typing-factory LLM round, from a ledger
already produced by extern_ledger.py.

Usage:
  python extern_evidence.py <ledger.json> <decode_dir> --out-dir <dir>
      [--limit N] [--batch-size 25] [--max-examples 5] [--before 6] [--after 4]

<ledger.json> is extern_ledger.py's own output (its "entries" list is the symbol
population and already carries provenance, unblock score, declaration, and twin data;
this tool does not re-derive any of that, only adds call-site evidence to it).
<decode_dir> is the same read-only full-corpus decode dir the ledger was built from.

CALL-SITE WINDOW: for every symbol, every "lw t9, SYMBOL(s7)" load found anywhere in
the corpus (not just where it caused an error: a factory reviewer needs to see how the
function is actually USED to propose a signature, and most call sites never error at
all), paired with the nearest following "jalr ra, t9" within WALKBACK_WINDOW lines. The
window is [lw_line - --before, jalr_line + --after] (default 6 before, 4 after): before
because some argument registers are set up ahead of the t9 load (verified,
actor-link-h_ir2.asm op 12 sets a0 two ops before its op-13 "lw t9, entity-actor-lookup"
in the very same call), after to capture the return-value use following the delay-slot
"sll v0, ra, 0". Every line in a window carries its own 1-based line number, and every
call site carries its source object name, so every window line is independently
citable by file:line -- the factory's verbatim-citation rule.

Per symbol this is capped at --max-examples call sites (default 5): unlike
extern_verify.py, which must see EVERY call site to make an arity claim, this tool is
building LLM prompt context, where a handful of representative examples plus the
already-summarized error_occurrences from the ledger is enough, and embedding
lobby-menu-manager-default-handler's full 22 error occurrences times however many total
call sites it actually has into one card would bloat every batch file for no benefit
(the ranking and cascade counts already tell the reviewer this symbol is heavily used).

BATCHING: cards are written --batch-size (default 25) to a file, in ledger rank order,
under <out-dir>/cards/cards_XXXX.json, plus <out-dir>/cards_manifest.json mapping every
symbol to its batch file and index within it. Rationale: one file per symbol would mean
1500+ small files cluttering a directory for no reviewer benefit (the ledger's own JSON
is already the single-file index); one giant file makes a single LLM round's input
unnecessarily large and unresumable if a round is interrupted partway. A batch size
matching a realistic per-round LLM batch (per project memory, jakx-typing-review-
factory) keeps each file a natural work unit.

CARD SCHEMA (one object per symbol; see build_card() for the exact fields):
  symbol, rank, provenance, unblock_score {tier, any_top_level_login,
  max_same_function_cascade, distinct_object_count}, declaration {active_bare, type,
  file, line, commented_remnant_lines}, twin (ledger's own twin object, unchanged),
  error_occurrences (the ledger's own occurrences list, unchanged), call_sites: [
    {object, file, lw_line, jalr_line, call_bang, window: [{line_no, text}, ...]}
  ]
"""
import argparse
import json
import os
import re
import sys

import extern_corpus as ec

LW_T9_RE = re.compile(r"\blw\s+t9,\s*(\S+)\(s7\)")


def collect_call_site_windows(decode_dir, symbols, before, after, max_examples):
    """-> dict[symbol] = [ {object, lw_line, jalr_line, call_bang, window}, ... ]

    Single pass over the whole corpus (one read per file), bucketing matches into every
    requested symbol's list as they are found, capped at max_examples per symbol. This
    is deliberately one pass shared across every symbol rather than one search per
    symbol per file: doing it per symbol is O(symbols x files) and takes minutes at
    ledger scale, where this is O(files) once, seconds."""
    target = set(symbols)
    result = {s: [] for s in target}
    for obj, path in ec.iter_ir2_files(decode_dir):
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
        for i, ln in enumerate(lines):
            m = LW_T9_RE.search(ln)
            if not m:
                continue
            sym = m.group(1)
            bucket = result.get(sym)
            if bucket is None or len(bucket) >= max_examples:
                continue
            jalr_idx = None
            call_bang = None
            hi = min(len(lines), i + 1 + ec.WALKBACK_WINDOW)
            for j in range(i + 1, hi):
                if "jalr ra, t9" in lines[j]:
                    jalr_idx = j
                    cm = ec.CALL_BANG_RE.search(lines[j])
                    call_bang = cm.group(0) if cm else None
                    break
            lo = max(0, i - before)
            anchor = jalr_idx if jalr_idx is not None else i
            hi2 = min(len(lines), anchor + 1 + after)
            window = [{"line_no": k + 1, "text": lines[k]} for k in range(lo, hi2)]
            bucket.append({
                "object": obj,
                "lw_line": i + 1,
                "jalr_line": (jalr_idx + 1) if jalr_idx is not None else None,
                "call_bang": call_bang,
                "window": window,
            })
    return result


def build_card(entry, call_sites, jakx_all_types_path):
    return {
        "symbol": entry["symbol"],
        "rank": entry["rank"],
        "provenance": entry["provenance"],
        "unblock_score": {
            "tier": entry["unblock_tier"],
            "any_top_level_login": entry["any_top_level_login"],
            "max_same_function_cascade": entry["max_same_function_cascade"],
            "distinct_object_count": entry["distinct_object_count"],
        },
        "declaration": {
            "active_bare": entry["declared_bare"],
            "type": entry["declared_bare_type"],
            "file": jakx_all_types_path,
            "line": entry["declared_bare_line"],
            "commented_remnant_lines": entry["commented_bare_remnant_lines"],
        },
        "twin": entry["twin"],
        "error_occurrences": entry["occurrences"],
        "call_sites": call_sites,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ledger_json")
    ap.add_argument("decode_dir")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--limit", type=int, default=None, help="only the top N ledger entries by rank")
    ap.add_argument("--batch-size", type=int, default=25)
    ap.add_argument("--max-examples", type=int, default=5, help="call sites per symbol")
    ap.add_argument("--before", type=int, default=6, help="lines of window before the lw t9 load")
    ap.add_argument("--after", type=int, default=4, help="lines of window after the jalr")
    args = ap.parse_args()

    with open(args.ledger_json, "r", encoding="utf-8") as fh:
        ledger = json.load(fh)

    entries = ledger["entries"]
    if args.limit is not None:
        entries = entries[: args.limit]

    jakx_all_types_path = ledger["generated_from"]["jakx_all_types"]
    symbols = [e["symbol"] for e in entries]

    print(f"collecting call-site windows for {len(symbols)} symbols against {args.decode_dir}")
    windows = collect_call_site_windows(args.decode_dir, symbols, args.before, args.after, args.max_examples)

    cards = [build_card(e, windows[e["symbol"]], jakx_all_types_path) for e in entries]

    cards_dir = os.path.join(args.out_dir, "cards")
    os.makedirs(cards_dir, exist_ok=True)
    manifest = {}
    batch_size = args.batch_size
    n_batches = (len(cards) + batch_size - 1) // batch_size if cards else 0
    for b in range(n_batches):
        chunk = cards[b * batch_size: (b + 1) * batch_size]
        fname = f"cards_{b + 1:04d}.json"
        fpath = os.path.join(cards_dir, fname)
        with open(fpath, "w", encoding="utf-8") as fh:
            json.dump(chunk, fh, indent=1)
        for idx, c in enumerate(chunk):
            manifest[c["symbol"]] = {"file": os.path.join("cards", fname), "index": idx, "rank": c["rank"]}

    manifest_path = os.path.join(args.out_dir, "cards_manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as fh:
        json.dump({
            "ledger_json": os.path.abspath(args.ledger_json),
            "decode_dir": os.path.abspath(args.decode_dir),
            "total_cards": len(cards),
            "batch_size": batch_size,
            "batches": n_batches,
            "symbols": manifest,
        }, fh, indent=1)

    with_call_sites = sum(1 for c in cards if c["call_sites"])
    without = len(cards) - with_call_sites
    print(f"wrote {n_batches} batch file(s) under {cards_dir}")
    print(f"{len(cards)} cards total: {with_call_sites} with >=1 call site, {without} with zero "
          f"(pure-data symbols never called via jalr, or genuinely uncalled in this corpus)")
    print(f"-> {manifest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
