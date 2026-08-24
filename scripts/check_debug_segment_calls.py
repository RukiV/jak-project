#!/usr/bin/env python3
"""Fail an unguarded main-segment call into a symbol that stays 0 under -boot.

Why this exists (issue 745): DebugSegment is symbol 0 in every -boot boot
(retail-shaped, REPL-deaf), and *debug-segment* stays #f there. A whole-file
`(declare-file (debug))` object still links under -boot, but a symbol its file
defines with a PLAIN `defun`, `defbehavior`, `defmethod` or `define` has no
-boot-time fallback at all: the symbol stays at its zero default. A
main-segment caller that reaches one of these unconditionally dispatches to
goal address 0 or reads a zero the first time it runs there, exactly like the
extern-no-defun class check_extern_defun.py already gates, except the symbol
here does have a definition, just not one that ever links under -boot. This is
how jakx-cam-warp-tick (engine/camera/cam-start.gc) called debug-set-camera-
pos-rot!, a plain `defun` in cam-debug.gc's `(declare-file (debug))`, guarded
only on the caller's own combiner state, never on the callee or
*debug-segment*: fixed by fix/jakx-warp-pin-debug-segment.

The one built-in exception is goal-lib.gc's `defun-debug` macro (:280-297,
44 call sites in goal_src/jakx): it compiles to `(if *debug-segment* (define
NAME (lambda ...)) (define :no-typecheck #t NAME nothing))`, so NAME is
ALWAYS bound to something at link time, the real lambda under -debug or the
always-resident `nothing` function under -boot. Calling one under -boot is a
safe no-op, not a symbol-0 dispatch, and these are excluded from the hazard
set entirely. `defbehavior-debug`, `define-debug` and `defmethod-debug` are
held in the same exclusion by the same macro shape, defensively; none of the
three is actually defined anywhere in this tree today (verified: zero
occurrences of any of them in goal_src/jakx), so if one is ever introduced
its own expansion needs checking before trusting this exclusion for it.

What it checks, per game (jakx by default): every non-REF `.gc` file under
goal_src/<game> for top-level `defun`/`defbehavior`/`defmethod`/`define`
forms; a name defined this way inside a whole-file `(declare-file (debug))`
file is the hazard set, UNLESS the same name is also defined by one of those
forms in a non-debug file (a name-collision report; the main-segment
definition wins and the name is dropped from the hazard set). Every non-debug
file is then scanned for a hazard name used as a bare token anywhere inside a
`defun`/`defbehavior`/`defmethod`/`defstate`/etc. top-level form (never inside
a `deftype` or `declare-type`, which only declare a method table and never
call anything: including `defmethod` in the hazard-collecting heads makes a
method's own name collide with its declaration in the owning type's
`(:methods (NAME ...) ...)` list otherwise, camera-h.gc's tracking-spline
being the case that surfaced it). A `(define-extern NAME ...)` line is not a
call site.

A hit is GUARDED when its enclosing top-level form's text contains `(nonzero?
NAME)` or `(zero? NAME)` (both senses appear: blit-displays.gc:621 and
shadow-vu1.gc:174-176 guard *screen-shot-work* with `zero?`, inverted from the
more common `nonzero?`), the same check on a local NAME was just let-bound to
(tfrag-methods.gc:130 and tie-methods.gc:316 bind *dma-mem-usage* to a local
before testing it), or a bare `*debug-segment*` test anywhere in the form.
Everything else is UNGUARDED. An UNGUARDED hit whose own caller is itself a
`defun-debug`/`defbehavior-debug`/`define-debug` form is reported separately
as a note, not this bug class: that caller is bound to `nothing` under -boot
and never runs there, so a call it makes never actually happens.

Known sites (26, all pre-existing) are held in
scripts/check_debug_segment_calls_allow.json, keyed by (file, callee) rather
than by line, which drifts as surrounding code changes. joint.gc's `inspect`
calling mem-size and movie-path.gc's `active`/method-16 calling
*screen-shot-work* are issue 745's own sweep; mood.gc and time-of-day.gc
calling *override-table*, *override-mood-fog-table* and
*override-mood-color-table* are issue 749. An allowlisted (file, callee) pair
prints as "allowed (issue N)" and never fails the gate; a callee reached
unguarded from a file not in the allowlist is a new hit and fails it.

Exit 1 with one FAIL line per unguarded main-segment hit outside the
allowlist; exit 0 when every unguarded hit is allowlisted or none exist.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

# ---------------------------------------------------------------------------
# Defining-form vocabulary
# ---------------------------------------------------------------------------

# Plain defining forms: no `-debug` suffix, no built-in *debug-segment*
# fallback. One of these sitting inside a whole-file (declare-file (debug))
# file leaves the symbol it defines with no -boot-time value at all.
PLAIN_DEF_HEADS = {"defun", "defbehavior", "define", "defmethod"}

# goal-lib.gc's defun-debug (and the same shape under the other three names,
# see module docstring) always binds the symbol to something at link time,
# so a call to one of these under -boot is a safe no-op, not a symbol-0
# dispatch. Excluded from the hazard set entirely.
SUFFIXED_DEBUG_DEF_HEADS = {
    "defun-debug", "defbehavior-debug", "define-debug", "defmethod-debug",
}

# A GOAL token: any maximal run of characters that is not a paren, quote, or
# whitespace. GOAL symbols freely use -, !, ?, *, <, >, =, +, etc., so a
# word-boundary regex is wrong here; tokenizing on delimiters sidesteps that.
TOKEN_RE = re.compile(r"[^()'\"\s]+")

# Top-level forms that declare structure rather than execute code. A bare
# method name inside a deftype's own (:methods (NAME ...) ...) table token-
# matches like a call otherwise (see module docstring).
NON_CALLER_HEADS = {"deftype", "declare-type"}

DEBUG_CALLER_PREFIXES = ("defun-debug ", "defbehavior-debug ", "define-debug ")


# ---------------------------------------------------------------------------
# Line masking: strip comments (line and block) and blank string interiors,
# preserving line structure so line numbers reported later match the file.
# ---------------------------------------------------------------------------

def mask_line(line, in_string, in_block_comment):
    out = []
    i = 0
    n = len(line)
    while i < n:
        if in_block_comment:
            if line[i:i + 2] == "|#":
                in_block_comment = False
                out.append("  ")
                i += 2
                continue
            out.append(" ")
            i += 1
            continue

        c = line[i]

        if in_string:
            if c == "\\" and i + 1 < n:
                out.append("  ")
                i += 2
                continue
            if c == '"':
                in_string = False
                out.append('"')
                i += 1
                continue
            out.append(" ")
            i += 1
            continue

        if line[i:i + 2] == "#|":
            in_block_comment = True
            out.append("  ")
            i += 2
            continue
        if c == '"':
            in_string = True
            out.append('"')
            i += 1
            continue
        if c == ";":
            break
        out.append(c)
        i += 1

    return "".join(out), in_string, in_block_comment


def read_masked_lines(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        raw_lines = f.readlines()
    masked_lines = []
    in_string = False
    in_block_comment = False
    for line in raw_lines:
        m, in_string, in_block_comment = mask_line(line, in_string, in_block_comment)
        masked_lines.append(m)
    return masked_lines


# ---------------------------------------------------------------------------
# Top-level form segmentation (paren-balance over masked text)
# ---------------------------------------------------------------------------

class TopForm:
    def __init__(self, start_line, end_line, head, label, text_masked):
        self.start_line = start_line
        self.end_line = end_line
        self.head = head
        self.label = label
        self.text_masked = text_masked


def form_label(head, tokens):
    if head in ("defmethod", "defmethod-debug"):
        method = tokens[1] if len(tokens) > 1 else "?"
        typ = tokens[3] if len(tokens) > 3 else "?"
        return f"{head} {method} ({typ})"
    if head == "defstate":
        state = tokens[1] if len(tokens) > 1 else "?"
        typ = tokens[2] if len(tokens) > 2 else "?"
        return f"defstate {state} ({typ})"
    name = tokens[1] if len(tokens) > 1 else ""
    return f"{head} {name}".strip()


def segment_top_forms(masked_lines):
    forms = []
    depth = 0
    start = None
    buf_start_idx = None
    for idx, line in enumerate(masked_lines, start=1):
        if start is None and line.strip() == "":
            continue
        for ch in line:
            if ch == "(":
                if depth == 0 and start is None:
                    start = idx
                    buf_start_idx = idx - 1
                depth += 1
            elif ch == ")":
                if depth > 0:
                    depth -= 1
        if start is not None and depth == 0:
            text = "".join(masked_lines[buf_start_idx:idx])
            tokens = TOKEN_RE.findall(text)
            head = tokens[0] if tokens else ""
            label = form_label(head, tokens)
            name = tokens[1] if len(tokens) > 1 else ""
            forms.append((TopForm(start, idx, head, label, text), name))
            start = None
            buf_start_idx = None
    return forms


def is_debug_file(masked_lines):
    for line in masked_lines:
        stripped = line.strip()
        if stripped.startswith("(declare-file") and "(debug)" in stripped:
            return True
    return False


# ---------------------------------------------------------------------------
# Corpus: collect hazard symbols (Phase B), then scan call sites (Phase C/D)
# ---------------------------------------------------------------------------

DEFINE_EXTERN_RE = re.compile(r"^\s*\(\s*define-extern\b")
END_DELIM = r"(?=[()'\"\s]|$)"
IDENT = r"[^()'\"\s]+"


def zero_check_re(tok):
    return re.compile(r"(?:non)?zero\?\s+" + re.escape(tok) + END_DELIM)


def bind_re(name):
    return re.compile(r"\(\s*(" + IDENT + r")\s+" + re.escape(name) + r"\s*\)")


def guard_verdict(name, form_text_masked):
    m = zero_check_re(name).search(form_text_masked)
    if m:
        return f"{m.group(0).split()[0]} {name}"
    if "*debug-segment*" in form_text_masked:
        return "*debug-segment* test"
    for bm in bind_re(name).finditer(form_text_masked):
        var = bm.group(1)
        m2 = zero_check_re(var).search(form_text_masked)
        if m2:
            return f"{m2.group(0).split()[0]} {var} (bound from {name})"
    return "UNGUARDED"


def find_gc_files(root):
    out = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            if fn.endswith(".gc") and not fn.endswith("_REF.gc"):
                out.append(os.path.join(dirpath, fn))
    return sorted(out)


def build_corpus(game_root, repo_root):
    files = []
    for path in find_gc_files(game_root):
        masked_lines = read_masked_lines(path)
        dbg = is_debug_file(masked_lines)
        forms = segment_top_forms(masked_lines)
        rel = os.path.relpath(path, repo_root).replace("\\", "/")
        files.append((rel, dbg, forms))
    return files


def collect_hazard_symbols(files):
    """(hazard, collisions): hazard maps name -> (file, line, head) of its
    first debug-file definition; collisions is the count of names dropped
    because a non-debug file also defines them (main-segment wins)."""
    debug_defs = defaultdict(list)
    main_defs = defaultdict(list)
    for rel, is_debug, forms in files:
        for form, name in forms:
            if form.head not in PLAIN_DEF_HEADS or not name:
                continue
            entry = (rel, form.start_line, form.head)
            (debug_defs if is_debug else main_defs)[name].append(entry)

    collide_names = sorted(set(debug_defs) & set(main_defs))
    hazard = {name: defs[0] for name, defs in debug_defs.items() if name not in set(collide_names)}
    return hazard, len(collide_names)


def scan_call_sites(files, hazard):
    """Yield dicts describing every hazard-name token found inside a non-
    debug file's own callable top-level forms."""
    names = set(hazard.keys())
    if not names:
        return
    for rel, is_debug, forms in files:
        if is_debug:
            continue
        for form, _name in forms:
            if form.head in NON_CALLER_HEADS:
                continue
            # form.text_masked is the concatenation of the file's own masked
            # lines from start_line to end_line; index into it by splitting
            # on '\n' rather than re-reading the file.
            lines = form.text_masked.split("\n")
            for offset, masked in enumerate(lines):
                if not masked.strip():
                    continue
                if DEFINE_EXTERN_RE.match(masked):
                    continue
                for m in TOKEN_RE.finditer(masked):
                    tok = m.group(0)
                    if tok not in names:
                        continue
                    line_no = form.start_line + offset
                    yield {
                        "file": rel,
                        "line": line_no,
                        "caller": form.label,
                        "callee": tok,
                        "callee_def": hazard[tok],
                        "guard": guard_verdict(tok, form.text_masked),
                    }


# ---------------------------------------------------------------------------
# Allowlist
# ---------------------------------------------------------------------------

def load_allowlist(path):
    if not os.path.isfile(path):
        return {}, []
    with open(path, encoding="utf-8") as f:
        entries = json.load(f)
    table = {}
    for e in entries:
        table[(e["file"], e["callee"])] = e["issue"]
    return table, entries


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0],
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--allow", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "check_debug_segment_calls_allow.json"))
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    game_root = os.path.join(root, "goal_src", args.game)
    if not os.path.isdir(game_root):
        print(f"error: {game_root} is not a directory", file=sys.stderr)
        return 2

    allow_table, allow_entries = load_allowlist(args.allow)

    files = build_corpus(game_root, root)
    hazard, collisions = collect_hazard_symbols(files)
    hits = sorted(scan_call_sites(files, hazard), key=lambda h: (h["file"], h["line"]))

    unguarded = [h for h in hits if h["guard"] == "UNGUARDED"]
    unguarded_debug_caller = [h for h in unguarded if h["caller"].startswith(DEBUG_CALLER_PREFIXES)]
    unguarded_main = [h for h in unguarded if not h["caller"].startswith(DEBUG_CALLER_PREFIXES)]

    fails, allowed = [], []
    for h in unguarded_main:
        key = (h["file"], h["callee"])
        if key in allow_table:
            allowed.append((h, allow_table[key]))
        else:
            fails.append(h)

    for h in unguarded_debug_caller:
        cfile, cline, chead = h["callee_def"]
        print(f"note: {h['file']}:{h['line']}: {h['caller']} calls {h['callee']} "
              f"(defined {cfile}:{cline} {chead}) unguarded, but the caller itself is "
              f"a *-debug form bound to `nothing` under -boot, so this is not the "
              f"issue 745 class")

    for h, issue in sorted(allowed, key=lambda p: (p[0]["file"], p[0]["line"])):
        print(f"allowed (issue {issue}): {h['file']}:{h['line']}: {h['caller']} -> {h['callee']}")

    for h in fails:
        cfile, cline, chead = h["callee_def"]
        print(f"FAIL: {h['file']}:{h['line']}: {h['caller']} calls {h['callee']} "
              f"(defined {cfile}:{cline} ({chead}) inside a whole-file (declare-file "
              f"(debug)) object) with no *debug-segment*/nonzero?/zero? guard in "
              f"scope, so this dispatches to goal 0 under -boot (issue 745 class)")

    matched_keys = {(h["file"], h["callee"]) for h in unguarded_main}
    stale = [e for e in allow_entries if (e["file"], e["callee"]) not in matched_keys]
    if stale:
        print(f"note: {len(stale)} allowlist entry(s) matched no current unguarded hit "
              f"(possibly stale, non-fatal):")
        for e in stale:
            print(f"  {e['file']} -> {e['callee']} (issue {e['issue']})")

    print(f"check_debug_segment_calls ({args.game}): {len(fails)} FAIL(s), {len(allowed)} allowed "
          f"(issue-tracked), {len(unguarded_debug_caller)} debug-caller note(s), "
          f"{len(hazard)} hazard symbol(s) ({collisions} name collision(s) excluded, main-segment "
          f"wins), {len(hits)} call site(s) scanned")

    if fails:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
