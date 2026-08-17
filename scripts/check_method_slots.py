#!/usr/bin/env python3
"""Flag a method/state dispatch on a slot no landed file fills.

Why this exists: GOAL method dispatch by name (`(name-of-method this arg)`,
`(method-of-type type name)`, `(method-of-object obj name)`) is resolved against
a type's method table. That table entry is 0 until some `defmethod` (or, for a
state, some `defstate`) for that name links somewhere in the tree, on the target
type or an ancestor of it. If nothing ever does, the call compiles clean, links
clean, and jumps to address 0 the first time it actually runs. No compile gate
sees this: the callee's absence is invisible to (mi) and to goalc-test, because
the caller only *names* the method, it never references a symbol that would fail
to link. This hit the boot twice on 2026-08-17: movie-path dispatching
path-control's methods 9 and 18 (get-num-segments's sibling, total-distance)
while path.gc was still a stub, and rigid-body-object's own method-81 dispatch
from its event handler.

What it checks, per game (jakx by default): the type hierarchy, own fields, and
`:methods` / `:state-methods` (with their slot ids) from
decompiler/config/<game>/all-types.gc; every
`(defmethod NAME ((this TYPE) ...) ...)` and `(defmethod NAME TYPE (...) ...)`
and every `(defstate NAME (TYPE) ...)` landed under goal_src/<game> (non-REF,
content below `;; DECOMP BEGINS` when that marker is present) as a filled slot
for TYPE; and, in that same landed source, every textually-resolvable dispatch:

  (a) `(method-of-type TYPE NAME)` (TYPE a literal type name), and
      `(method-of-object X NAME)` where X is `this`, `self`, or a `(-> this ...)`
      / `(-> self ...)` field-deref chain resolved through the field types in
      all-types.gc;
  (b) `(NAME this ...)` and `(NAME (-> this ...) ...)` inside a `defmethod` of a
      known type, where NAME is a method of that type (or the deref target's
      type) or an ancestor;
  (c) `(NAME self ...)` and `(NAME (-> self ...) ...)` inside a `defbehavior` or
      inside a `defstate`'s handlers, for a known `:behavior`/state type.

A dispatch is skipped, not reported, when NAME does not resolve to a declared
method or state-method of the target type or an ancestor: that is the
"be conservative" half of the brief, since an unresolved head is far more
likely to be an ordinary function call than a genuine method dispatch this tool
can prove anything about. This is deliberately not a general call-graph or
type-inference pass; it only recognizes the handful of textually literal forms
listed above, and a field deref chain resolves only through plain and
pointer-to-structure field types, one hop at a time.

A slot is filled for type T if T or any ancestor of T has a landed defmethod
(for :methods) or defstate (for :state-methods) of that name. Unlike
check_state_inherit this does not model link order: an ordinary method-table
slot is patched directly on its owning type whenever that defmethod links, not
copied through an inheritance snapshot at one fixed moment the way a virtual
defstate's `inherit-state` is (that link-order-sensitive case is exactly what
check_state_inherit already guards). So "filled" here means "filled anywhere,
by anything, eventually" -- if that is still false after scanning every landed
file, the slot is 0 for good.

Findings are grouped by the calling object (the .gc file's own name, matching
the DGO listings). An object that links into the game DGO is on the boot path:
every unfilled dispatch from it prints as a FAIL line. An object that only
links into level DGOs prints as a note instead, because level residency
depends on borrow lists and common-level sharing the .gd files alone do not
encode. This checker has a real backlog against the current tree (see the
--report inventory); forcing every FAIL red by default is not this script's
call, so the default exit code is 0 regardless of what it finds. Pass --strict
to make a FAIL line (never a note) return 1.

--report prints every recognized dispatch, filled or not, linked or not: an
inventory of what this tool does and does not see, for auditing coverage.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

DEFTYPE_HDR_RE = re.compile(r"^\(deftype\s+([^\s()]+)\s+\(", re.M)
DEFMETHOD_RE = re.compile(
    r"^\(defmethod\s+([^\s()]+)\s+(?:\(\(this\s+([^\s()]+)\)|([^\s()]+))", re.M
)
DEFBEHAVIOR_RE = re.compile(r"^\(defbehavior\s+([^\s()]+)\s+([^\s()]+)\s*\(", re.M)
DEFSTATE_RE = re.compile(r"^\(defstate\s+([^\s()]+)\s+\(([^\s()]+)\)", re.M)
DGO_OBJ_RE = re.compile(r'"([^"]+)\.o"')

METHOD_OF_TYPE_RE = re.compile(r"\(method-of-type\s+([^\s()]+)\s+([^\s()]+)\)")
METHOD_OF_OBJECT_BARE_RE = re.compile(r"\(method-of-object\s+(this|self)\s+([^\s()]+)\)")
METHOD_OF_OBJECT_DEREF_RE = re.compile(
    r"\(method-of-object\s+\(->\s+(?:this|self)((?:\s+[^\s()]+)*)\s*\)\s+([^\s()]+)\)"
)
BARE_CALL_RE = re.compile(r"\(([A-Za-z][^\s()]*)\s+(?:this|self)\b")
DEREF_CALL_RE = re.compile(r"\(([A-Za-z][^\s()]*)\s+\(->\s+(?:this|self)((?:\s+[^\s()]+)*)\s*\)")


# ---------------------------------------------------------------------------
# Minimal s-expression helpers: depth-matching that understands GOAL's three
# depth-invisible spans (line comments, block comments, string literals) and
# the `#\x` character-literal escape, so a stray `#\(` or a `;` inside a
# format string never desyncs paren counting. Everything above this section
# works on regions these helpers already bounded.
# ---------------------------------------------------------------------------


def matching_close(text, open_idx):
    """Index just past the ')' that matches the '(' at open_idx."""
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == ";":
            nl = text.find("\n", i)
            i = n if nl == -1 else nl + 1
            continue
        if c == "#" and i + 1 < n and text[i + 1] == "|":
            end = text.find("|#", i + 2)
            i = n if end == -1 else end + 2
            continue
        if c == "#" and i + 1 < n and text[i + 1] == "\\":
            i += 2
            j = i
            while j < n and (text[j].isalnum() or text[j] == "-"):
                j += 1
            i = j if j > i else i + 1
            continue
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if c == "(":
            depth += 1
            i += 1
            continue
        if c == ")":
            depth -= 1
            i += 1
            if depth == 0:
                return i
            continue
        i += 1
    return n


def next_open_paren(text, i, limit):
    """First '(' at or after i, skipping whitespace/comments/one docstring."""
    while i < limit:
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if c == ";":
            nl = text.find("\n", i)
            i = limit if nl == -1 or nl >= limit else nl + 1
            continue
        if c == "#" and i + 1 < limit and text[i + 1] == "|":
            end = text.find("|#", i + 2)
            i = limit if end == -1 else end + 2
            continue
        if c == '"':
            i += 1
            while i < limit and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if c == "(":
            return i
        return None
    return None


def block_entries(text, block_start, block_end):
    """Entries of a (:methods ...) / (:state-methods ...) body: bare atoms
    (state-method names are often unwrapped) or (name ...) forms (method
    entries always are). Returns (name, entry_start, entry_end) triples."""
    out = []
    i = block_start
    while i < block_end:
        c = text[i]
        if c.isspace():
            i += 1
            continue
        if c == ";":
            nl = text.find("\n", i)
            i = block_end if nl == -1 or nl >= block_end else nl + 1
            continue
        if c == "#" and i + 1 < block_end and text[i + 1] == "|":
            end = text.find("|#", i + 2)
            i = block_end if end == -1 else end + 2
            continue
        if c == "(":
            end = matching_close(text, i)
            nm = re.match(r"\(\s*([^\s()]+)", text[i:end])
            out.append((nm.group(1) if nm else None, i, end))
            i = end
            continue
        m = re.match(r"[^\s()]+", text[i:block_end])
        if not m:
            i += 1
            continue
        out.append((m.group(0), i, i + m.end()))
        i += m.end()
    return out


def entry_id(text, entry_end, next_boundary):
    """Trailing `;; N` slot id on the same line as an entry, if present."""
    nl = text.find("\n", entry_end)
    line_end = next_boundary if nl == -1 else min(nl, next_boundary)
    m = re.search(r";;\s*(\d+)\b", text[entry_end:line_end])
    return int(m.group(1)) if m else None


# ---------------------------------------------------------------------------
# all-types.gc: type hierarchy, own fields, own methods and state-methods.
# ---------------------------------------------------------------------------


def load_all_types(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    # Strip block comments before anything else: all-types.gc carries dead
    # `#| (deftype ... ...) |#` blocks left by earlier resurrections (the
    # convention the file itself calls "dead-type sweep"), and a naive
    # column-0 `^\(deftype` regex matches their commented-out headers just as
    # readily as a live one. Line comments are left alone here because entry
    # ids live in trailing `;; N` comments this function still needs to read.
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    parent_of = {}
    fields_of = defaultdict(dict)  # type -> {field_name: field_type or None}
    methods_of = defaultdict(dict)  # type -> {name: id or None}, own decls only
    states_of = defaultdict(dict)  # type -> {name: id or None}, own decls only

    for hm in DEFTYPE_HDR_RE.finditer(text):
        name = hm.group(1)
        paren_open = hm.end() - 1
        parent_end = matching_close(text, paren_open)
        parent_m = re.match(r"\s*([^\s()]+)", text[paren_open + 1 : parent_end - 1])
        parent_of[name] = parent_m.group(1) if parent_m else None

        form_end = matching_close(text, hm.start())

        field_open = next_open_paren(text, parent_end, form_end)
        if field_open is not None:
            field_close = matching_close(text, field_open)
            for fname, fstart, fend in block_entries(text, field_open + 1, field_close - 1):
                if fname is None:
                    continue
                rest = text[fstart:fend]
                tm = re.match(r"\(\s*[^\s()]+\s+([^\s()]+|\([^()]*\))", rest)
                ftype = None
                if tm:
                    tok = tm.group(1)
                    if tok.startswith("("):
                        pm = re.match(r"\(\s*pointer\s+([^\s()]+)", tok)
                        ftype = pm.group(1) if pm else None
                    else:
                        ftype = tok
                fields_of[name][fname] = ftype

        for kw, dest in ((":methods", methods_of), (":state-methods", states_of)):
            km = re.search(r"\(" + re.escape(kw) + r"\b", text[hm.start() : form_end])
            if not km:
                continue
            kw_open = hm.start() + km.start()
            blk_end = matching_close(text, kw_open)
            entries = block_entries(text, kw_open + 1, blk_end - 1)
            next_id = None
            for idx, (ename, estart, eend) in enumerate(entries):
                if ename is None:
                    continue
                boundary = entries[idx + 1][1] if idx + 1 < len(entries) else blk_end - 1
                eid = entry_id(text, eend, boundary)
                if eid is None:
                    eid = next_id
                if eid is not None:
                    next_id = eid + 1
                dest[name][ename] = eid

    return parent_of, fields_of, methods_of, states_of


def ancestors(parent_of, t):
    out = []
    seen = set()
    while t in parent_of and parent_of[t] is not None and t not in seen:
        seen.add(t)
        t = parent_of[t]
        out.append(t)
    return out


def chain_lookup(dict_of, parent_of, t, name):
    """(owner_type, id) for the nearest type in {t}+ancestors(t) declaring
    name, or (None, None)."""
    seen = set()
    cur = t
    while cur is not None and cur not in seen:
        seen.add(cur)
        if name in dict_of.get(cur, {}):
            return cur, dict_of[cur][name]
        cur = parent_of.get(cur)
    return None, None


def chain_any(set_of, parent_of, t, name):
    seen = set()
    cur = t
    while cur is not None and cur not in seen:
        seen.add(cur)
        if name in set_of.get(cur, ()):
            return True
        cur = parent_of.get(cur)
    return False


def resolve_field_chain(start_type, chain, fields_of, parent_of):
    t = start_type
    for f in chain:
        if t is None:
            return None
        _owner, ftype = chain_lookup(fields_of, parent_of, t, f)
        t = ftype
    return t


# ---------------------------------------------------------------------------
# goal_src: filled slots (defmethod/defstate) and dispatch call sites.
# ---------------------------------------------------------------------------


def strip_comments(text):
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    return "\n".join(line.split(";", 1)[0] for line in text.split("\n"))


def landed_text(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    if ";; DECOMP BEGINS" in text:
        text = text.split(";; DECOMP BEGINS", 1)[1]
    return text


def load_link_order(dgos_dir, game_dgo):
    rank = defaultdict(dict)
    files = [game_dgo] + sorted(
        f for f in os.listdir(dgos_dir) if f.endswith(".gd") and f != game_dgo
    )
    for di, fn in enumerate(files):
        path = os.path.join(dgos_dir, fn)
        for pos, m in enumerate(DGO_OBJ_RE.finditer(open(path, encoding="utf-8").read())):
            rank[m.group(1)].setdefault(di, pos)
    return rank


def collect_spans(text):
    """List of (self_type, body_start, body_end) for every top-level
    defmethod/defbehavior/defstate in text. `this` inside a defmethod and
    `self` inside a defbehavior or a defstate's handlers both resolve to
    self_type here; the two keywords are never mixed up because a caller
    matches only the keyword its own regex looked for."""
    spans = []
    for m in DEFMETHOD_RE.finditer(text):
        ty = m.group(2) or m.group(3)
        if ty is None:
            continue
        end = matching_close(text, m.start())
        spans.append((ty, m.end(), end))
    for m in DEFBEHAVIOR_RE.finditer(text):
        end = matching_close(text, m.start())
        spans.append((m.group(2), m.end(), end))
    for m in DEFSTATE_RE.finditer(text):
        end = matching_close(text, m.start())
        spans.append((m.group(2), m.end(), end))
    return spans


def scan_file(text, fields_of, methods_of, states_of, parent_of):
    """Return (filled_defs, candidates). filled_defs: list of
    ('method'|'state', type, name). candidates: list of (target_type, name,
    expr_text) recognized dispatches, before the filled/declared check."""
    filled_defs = []
    for m in DEFMETHOD_RE.finditer(text):
        ty = m.group(2) or m.group(3)
        if ty:
            filled_defs.append(("method", ty, m.group(1)))
    for m in DEFSTATE_RE.finditer(text):
        filled_defs.append(("state", m.group(2), m.group(1)))

    stripped = strip_comments(text)
    candidates = []

    for m in METHOD_OF_TYPE_RE.finditer(stripped):
        ty, name = m.group(1), m.group(2)
        if ty in parent_of or ty in methods_of or ty in states_of:
            candidates.append((ty, name, "(method-of-type %s %s)" % (ty, name)))

    for ty, body_start, body_end in collect_spans(stripped):
        body = stripped[body_start:body_end]

        for m in METHOD_OF_OBJECT_BARE_RE.finditer(body):
            name = m.group(2)
            candidates.append((ty, name, "(method-of-object %s %s)" % (m.group(1), name)))

        for m in METHOD_OF_OBJECT_DEREF_RE.finditer(body):
            chain = m.group(1).split()
            name = m.group(2)
            target = resolve_field_chain(ty, chain, fields_of, parent_of)
            expr = "(method-of-object (-> this%s) %s)" % (
                (" " + " ".join(chain)) if chain else "",
                name,
            )
            if target:
                candidates.append((target, name, expr))

        for m in BARE_CALL_RE.finditer(body):
            name = m.group(1)
            candidates.append((ty, name, "(%s this ...)" % name))

        for m in DEREF_CALL_RE.finditer(body):
            name = m.group(1)
            chain = m.group(2).split()
            target = resolve_field_chain(ty, chain, fields_of, parent_of)
            expr = "(%s (-> this%s) ...)" % (name, (" " + " ".join(chain)) if chain else "")
            if target:
                candidates.append((target, name, expr))

    return filled_defs, candidates


def load_goal_src(goal_src):
    """Return [(obj, landed_text), ...] for every non-REF .gc file under
    goal_src/<game>, obj being the filename without its .gc extension (the
    same key the DGO listings use)."""
    all_candidates = []
    for dp, _dirs, files in os.walk(goal_src):
        for fn in files:
            if not fn.endswith(".gc") or fn.endswith("_REF.gc"):
                continue
            path = os.path.join(dp, fn)
            obj = fn[:-3]
            text = landed_text(path)
            all_candidates.append((obj, text))
    return all_candidates


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument(
        "--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    )
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--game-dgo", default="game.gd")
    ap.add_argument("--strict", action="store_true", help="exit 1 if any FAIL (game-DGO) finding exists")
    ap.add_argument("--report", action="store_true", help="inventory every recognized dispatch, filled or not")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    alltypes = os.path.join(root, "decompiler", "config", args.game, "all-types.gc")
    goal_src = os.path.join(root, "goal_src", args.game)
    parent_of, fields_of, methods_of, states_of = load_all_types(alltypes)
    rank = load_link_order(os.path.join(goal_src, "dgos"), args.game_dgo)

    file_texts = load_goal_src(goal_src)

    filled_methods = defaultdict(set)
    filled_states = defaultdict(set)
    per_file_candidates = {}
    for obj, text in file_texts:
        filled_defs, candidates = scan_file(text, fields_of, methods_of, states_of, parent_of)
        for kind, ty, name in filled_defs:
            (filled_methods if kind == "method" else filled_states)[ty].add(name)
        per_file_candidates.setdefault(obj, []).extend(candidates)

    report_rows = []
    violations, notes = [], []
    for obj, candidates in per_file_candidates.items():
        for target_type, name, expr in candidates:
            owner, mid = chain_lookup(methods_of, parent_of, target_type, name)
            kind = "method"
            filled_set = filled_methods
            if owner is None:
                owner, mid = chain_lookup(states_of, parent_of, target_type, name)
                kind = "state"
                filled_set = filled_states
            if owner is None:
                continue  # not a recognized method/state name: not a dispatch we can prove anything about
            filled = chain_any(filled_set, parent_of, target_type, name)
            if args.report:
                report_rows.append((obj, kind, expr, name, target_type, owner, mid, filled))
                continue
            if filled:
                continue
            anc = ancestors(parent_of, target_type)
            chain_desc = ", ".join([target_type] + anc) if anc else target_type
            row = (obj, kind, expr, name, target_type, owner, mid, chain_desc)
            if obj not in rank:
                continue  # never linked into any DGO: this call never runs
            if 0 in rank[obj]:
                violations.append(row)
            else:
                notes.append(row)

    if args.report:
        report_rows.sort()
        for obj, kind, expr, name, target_type, owner, mid, filled in report_rows:
            status = "filled" if filled else "UNFILLED"
            idtxt = mid if mid is not None else "?"
            print(
                "%s: %s targets %s's %s %s (id %s, declared on %s) [%s]"
                % (obj, expr, target_type, kind, name, idtxt, owner, status)
            )
        print("check_method_slots (%s): %d recognized dispatch(es) inventoried" % (args.game, len(report_rows)))
        return 0

    def_word = {"method": "defmethod", "state": "defstate"}
    for label, rows in (("note", sorted(notes)), ("FAIL", sorted(violations))):
        for obj, kind, expr, name, target_type, owner, mid, chain_desc in rows:
            idtxt = mid if mid is not None else "?"
            print(
                "%s: %s: %s targets %s's %s %s (id %s, declared on %s); no landed %s "
                "fills it on %s or any ancestor (%s)"
                % (
                    label, obj, expr, target_type, kind, name, idtxt, owner,
                    def_word[kind], target_type, chain_desc,
                )
            )

    print(
        "check_method_slots (%s): %d game-DGO FAIL(s), %d level-DGO note(s)"
        % (args.game, len(violations), len(notes))
    )
    if args.strict and violations:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
