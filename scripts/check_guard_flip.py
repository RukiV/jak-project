#!/usr/bin/env python3
"""Flag a (nonzero? FN) landing guard whose own body still dispatches or
reads something unresolved once FN links.

Why this exists: `(if (nonzero? FN) (FN ...))` and `(when (nonzero? FN) ...)`
only prove FN's own symbol slot is nonzero, which happens the moment some
landed file supplies a defun for it. That tells goalc nothing about what FN's
body does once it actually runs. This is how game-info.gc's own
`(nonzero? kill-current-talker)` guard took a boot down on 2026-08-18: the
guard armed the instant ambient.gc landed a real kill-current-talker, and
kill-current-talker's body unconditionally dispatched
`(gui-control-method-16 *gui-control* ...)` against *gui-control*, a global
whose type is declared by three separate define-extern forms across the tree
but that no landed file ever gives a value. The first call faulted on the
never-instantiated symbol. PR 427's fix was not to touch game-info.gc's outer
guard at all; it wrapped kill-current-talker's own risky calls in a second,
inner `(when (nonzero? *gui-control*) ...)` guard, so landing
kill-current-talker no longer flips anything live until *gui-control* itself
is real. This checker recognizes that idiom on purpose: a body-internal
`(nonzero? G)` guard around a risky reference to G resolves the finding,
because that is the actual fix shape, not a coincidence to route around.

What it checks, per game (jakx by default): every landed `(if (nonzero? FN)
...)` / `(when (nonzero? FN) ...)` form under goal_src/<game> whose protected
region actually calls `(FN ...)` (a guard that tests FN but never calls it is
not this defect's shape). FN is ARMED when some landed defun, defbehavior,
defun-debug or def-mips2c binds it anywhere in the tree, DORMANT otherwise
(a DORMANT guard's consequent never runs today, so its body is not a live
risk and is not inspected). For each ARMED FN bound by a defun, defbehavior
or defmethod (a def-mips2c body is raw assembly, not GOAL text this checker
can read, so those are reported separately as not inspectable), FN's own body
is scanned for:

  (a) a method dispatch this checker cannot prove is filled: the same
      auto-named `(TYPE-method-N RECEIVER ...)` shape, `(method-of-object
      RECEIVER NAME)` shape, and, when FN itself has a `this`/`self` receiver
      (a defbehavior or defmethod), the bare `(NAME this ...)` / `(NAME self
      ...)` shape check_method_slots.py already recognizes and resolves
      against the type hierarchy from decompiler/config/<game>/all-types.gc;
  (b) a bare read of a plain data global whose only declaration anywhere in
      goal_src is define-extern, with no landed define or define-perm ever
      giving it a value. Function-typed and state-typed externs are excluded
      from this class on purpose: an unbound function call is check_extern_
      defun.py's finding to make (and that checker already scans every
      landed call site, guard body included, so repeating it here would
      double-report the same defect under a different name), and a state
      symbol is armed by defstate, not by define/define-perm, which is a
      question check_state_inherit.py already owns. Kernel builtins are
      excluded the same mechanical way check_extern_defun.py excludes them
      for its own extern class: every symbol declared inside a file
      compiler-setup.gc loads through asm-file is backed by the C++ runtime,
      never by a goal_src define, so it is never a real "left unbound"
      finding no matter how many places its type gets declared.

A finding in either class is dropped, not reported, when the risky reference
sits inside a body-internal `(if (nonzero? G) ...)` / `(when (nonzero? G)
...)` whose tested symbol G is the dispatch's own receiver token (class a) or
the global itself (class b): that is PR 427's fix idiom, described above, and
recognizing it is a deliberate design choice, not an oversight. Deref-chain
receivers (`(-> this foo)`) are out of scope for both the dispatch scan and
the guard-resolution match in a guarded function's own body, matching
check_method_slots.py's own stated conservatism; a receiver this checker
cannot resolve is skipped, not reported, since an unresolved head is more
likely an ordinary call this tool cannot prove anything about than a genuine
unfilled dispatch.

Reuse: the type hierarchy loader, the landed-defmethod/defstate fill scan,
and the auto-name/method-of-object/bare-call regexes are imported directly
from check_method_slots.py (load_all_types, scan_file, ancestors,
chain_lookup, chain_any, matching_close, strip_comments, landed_text,
load_link_order, and the METHOD_OF_OBJECT_*/BARE_CALL_RE/DEFBEHAVIOR_RE/
DEFMETHOD_RE patterns), since that machinery is exactly "is this dispatch
filled" and re-deriving it would drift from the one place that logic already
lives. What is NOT imported is scan_file's whole-file candidate list itself:
it does not track match offsets, so it cannot answer "which dispatches sit
inside THIS ONE guarded function's body" (as opposed to anywhere in the
file), and its DEFBEHAVIOR_RE/DEFMETHOD_RE matches stop partway through the
form's own argument list, which check_method_slots.py never needed to skip
because it only asks each span for its receiver type. The candidate-finding
loop below, and the body_span helper that locates where a body actually
starts past that argument list, are therefore a scoped reimplementation,
built from the same imported regexes and resolved through the same imported
lookup helpers.

Findings are grouped by the calling object, the same convention check_
spawn_init.py, check_method_slots.py and check_extern_defun.py use: the
object containing the outer guard is what determines FAIL (game DGO) versus
note (level DGO only), since that is the form the boot actually reaches
first when FN lands.

Modes: report (default) prints every finding and exits 0; --report prints
every guard found, ARMED or DORMANT, resolved or not, for auditing coverage;
--strict exits 1 when any game-DGO FAIL exists.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_method_slots as cms

OUTER_GUARD_RE = re.compile(r"\((if|when)\s+\(nonzero\?\s+([^\s()]+)\)")
INNER_GUARD_RE = re.compile(r"\((?:if|when)\s+\(nonzero\?\s+([^\s()]+)\)")
DEFUN_RE = re.compile(r"^\(defun\s+([^\s()]+)\s+", re.M)
DEFUN_DEBUG_RE = re.compile(r"^\(defun-debug\s+([^\s()]+)\s+", re.M)
MIPS2C_RE = re.compile(r"^\(def-mips2c\s+([^\s()]+)", re.M)
DEFINE_EXTERN_RE = re.compile(r"\(define-extern\s+([^\s()]+)\s+")
# Function-typed and state-typed externs are carved out of the class-b global
# scan (see the docstring); a docstring string is allowed between the name
# and the type, matching kernel-defs.gc's own style, the same as check_
# extern_defun.py's DEFINE_EXTERN_FN_RE.
DEFINE_EXTERN_FN_RE = re.compile(
    r'\(define-extern\s+([^\s()]+)\s+(?:"(?:[^"\\]|\\.)*"\s+)?\(function\b'
)
DEFINE_EXTERN_STATE_RE = re.compile(
    r'\(define-extern\s+([^\s()]+)\s+(?:"(?:[^"\\]|\\.)*"\s+)?\(state\b'
)
DEFINE_VALUE_RE = re.compile(r"\((?:define|define-perm)\s+([^\s()]+)\s+")
AUTO_NAME_RECV_RE = re.compile(r"\((([A-Za-z][A-Za-z0-9-]*?)-method-(\d+))\s+([^\s()]+)")
NONZERO_TEST_RE = re.compile(r"\(nonzero\?\s+([^\s()]+)\)")
GLOBAL_TOKEN_RE = re.compile(
    r"(?<![A-Za-z0-9!?*<>=+/._%&$#-])([A-Za-z0-9!?*<>=+/._%&$#-]+)(?![A-Za-z0-9!?*<>=+/._%&$#-])"
)
SYM_RE = re.compile(r"^[A-Za-z0-9!?*<>=+/._%&$#-]+$")


def landed_offset_and_text(text):
    """(line_of_marker, landed_substring), same convention as check_extern_
    defun.py's helper of the same name: line_of_marker lets a match offset
    found in the returned substring be translated back to a real file:line."""
    idx = text.find(";; DECOMP BEGINS")
    if idx == -1:
        return 1, text
    after = text.find("\n", idx)
    after = idx if after == -1 else after + 1
    return text.count("\n", 0, after) + 1, text[after:]


def load_files(goal_src):
    files = []
    for dp, _d, fns in os.walk(goal_src):
        for fn in fns:
            if fn.endswith(".gc") and not fn.endswith("_REF.gc"):
                files.append((fn[:-3], os.path.join(dp, fn)))
    return files


ASM_FILE_RE = re.compile(r'\(asm-file\s+"([^"]+)"')


def collect_kernel_builtins(root, game):
    """Every symbol declared via define-extern inside a file compiler-setup.gc
    loads through asm-file: the same mechanical signal check_extern_defun.py
    uses. compiler-setup.gc's own comment on the asm-file line says these
    files' contents are provided by the runtime, so a symbol declared only
    there is never expected to have a goal_src binding."""
    setup_path = os.path.join(root, "goal_src", game, "compiler-setup.gc")
    text = open(setup_path, encoding="utf-8", errors="replace").read()
    builtins = set()
    for m in ASM_FILE_RE.finditer(text):
        asm_path = os.path.join(root, m.group(1).replace("/", os.sep))
        asm_text = cms.strip_comments(open(asm_path, encoding="utf-8", errors="replace").read())
        for dm in DEFINE_EXTERN_RE.finditer(asm_text):
            builtins.add(dm.group(1))
    return builtins


def body_span(text, form_start, form_end, extra_head_tokens):
    """Index where a defun/defbehavior/defmethod's own executable body
    begins. The regexes used to find these forms (borrowed from
    check_method_slots.py for defbehavior/defmethod, written locally for
    defun/defun-debug) stop at varying points inside the form's own head,
    never past the argument list: check_method_slots.py never needed the
    body's start offset, only the type each span belongs to, so its own
    m.end() is not reusable here. This walks the form itself: skip the
    keyword, walk past `extra_head_tokens` more top-level tokens (the name,
    plus for defbehavior/defmethod however many type tokens precede the
    argument list), then skip exactly one more top-level form, the argument
    list, and return what follows it."""
    i = form_start + 1
    while i < form_end and not text[i].isspace():
        i += 1
    to_skip = extra_head_tokens + 1
    skipped = 0
    while skipped < to_skip and i < form_end:
        while i < form_end and text[i].isspace():
            i += 1
        if i >= form_end:
            break
        if text[i] == "(":
            i = cms.matching_close(text, i)
        else:
            while i < form_end and not text[i].isspace() and text[i] not in "()":
                i += 1
        skipped += 1
    return i


def internal_guards(body):
    """[(guard_symbol, protected_start, protected_end), ...] for every
    body-internal (if (nonzero? G) ...) / (when (nonzero? G) ...)."""
    out = []
    for m in INNER_GUARD_RE.finditer(body):
        end = cms.matching_close(body, m.start())
        out.append((m.group(1), m.end(), end))
    return out


def is_protected(idx, guards, symbol):
    return any(g == symbol and start <= idx < end for g, start, end in guards)


def find_risky(body, self_type, fields_of, methods_of, states_of, parent_of,
                filled_methods, filled_states, descendants_of, extern_only_globals):
    """[(class, detail), ...] unresolved findings inside one binding's body."""
    guards = internal_guards(body)
    findings = []

    # class (d) from check_method_slots.py: the decompiler's auto-generated
    # method names spell their own type, so this needs no receiver-type
    # context. A function body never nests a deftype, unlike a whole file, so
    # the deftype-blanking step scan_file does before this scan is not needed
    # here.
    for m in AUTO_NAME_RECV_RE.finditer(body):
        name, ty, recv = m.group(1), m.group(2), m.group(4)
        pre = body[max(0, m.start() - 40):m.start()].rstrip()
        if pre.endswith("method-of-type " + ty) or pre.endswith("method-of-object"):
            continue
        if ty not in parent_of and ty not in methods_of:
            continue
        owner, mid = cms.chain_lookup(methods_of, parent_of, ty, name)
        if owner is None:
            continue
        filled = cms.chain_any(filled_methods, parent_of, ty, name)
        if not filled:
            filled = any(name in filled_methods.get(d, ()) for d in descendants_of.get(ty, ()))
        if filled:
            continue
        if SYM_RE.match(recv) and is_protected(m.start(), guards, recv):
            continue
        findings.append((
            "dispatch",
            "(%s %s ...) targets %s's method %s (id %s, declared on %s); "
            "no landed defmethod fills it" % (name, recv, ty, name, mid if mid is not None else "?", owner),
        ))

    # this/self relative dispatches only make sense when the binding itself
    # has a this/self receiver (defbehavior or defmethod).
    if self_type:
        for m in cms.METHOD_OF_OBJECT_BARE_RE.finditer(body):
            recv_kw, name = m.group(1), m.group(2)
            owner, mid = cms.chain_lookup(methods_of, parent_of, self_type, name)
            if owner is None:
                continue
            if cms.chain_any(filled_methods, parent_of, self_type, name):
                continue
            if is_protected(m.start(), guards, recv_kw):
                continue
            findings.append((
                "dispatch",
                "(method-of-object %s %s) targets %s's method %s (id %s, declared on %s); "
                "no landed defmethod fills it" % (recv_kw, name, self_type, name, mid if mid is not None else "?", owner),
            ))
        for m in cms.BARE_CALL_RE.finditer(body):
            name = m.group(1)
            recv_kw = "this" if m.group(0).rstrip().endswith("this") else "self"
            owner, mid = cms.chain_lookup(methods_of, parent_of, self_type, name)
            kind, filled_set = "method", filled_methods
            if owner is None:
                owner, mid = cms.chain_lookup(states_of, parent_of, self_type, name)
                kind, filled_set = "state", filled_states
            if owner is None:
                continue
            if cms.chain_any(filled_set, parent_of, self_type, name):
                continue
            if is_protected(m.start(), guards, recv_kw):
                continue
            def_word = "defmethod" if kind == "method" else "defstate"
            findings.append((
                "dispatch",
                "(%s %s ...) targets %s's %s %s (id %s, declared on %s); "
                "no landed %s fills it" % (name, recv_kw, self_type, kind, name,
                                            mid if mid is not None else "?", owner, def_word),
            ))

    # Every real (non-test) occurrence of each candidate global, so a finding
    # is only raised when at least one occurrence is actually unprotected.
    # Reporting on the first occurrence found, as an earlier version of this
    # script did, false-positived on kill-current-talker: the first textual
    # occurrence of *gui-control* in its body is the token G inside the very
    # `(nonzero? G)` test that starts the guard protecting every real use
    # after it, and a test clause is never inside its own protected region.
    test_spans = [(m.start(), m.end()) for m in NONZERO_TEST_RE.finditer(body)]
    occurrences = defaultdict(list)
    for m in GLOBAL_TOKEN_RE.finditer(body):
        name = m.group(1)
        if name not in extern_only_globals:
            continue
        if any(s <= m.start() < e for s, e in test_spans):
            continue  # the zero-test itself does not dereference the global
        # A token right after a quote is data (a symbol literal, e.g. a
        # 'tag argument), not a variable read, even when it happens to share
        # a name with a declared extern.
        if m.start() > 0 and body[m.start() - 1] == "'":
            continue
        occurrences[name].append(m.start())

    for name, idxs in occurrences.items():
        unprotected = [i for i in idxs if not is_protected(i, guards, name)]
        if not unprotected:
            continue
        findings.append((
            "global",
            "reads %s (%d of %d occurrence(s) unguarded), which is declared "
            "via define-extern but no landed define/define-perm anywhere "
            "gives it a value" % (name, len(unprotected), len(idxs)),
        ))

    return findings


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--game-dgo", default="game.gd")
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--strict", action="store_true", help="exit 1 when any game-DGO FAIL exists")
    ap.add_argument("--report", action="store_true", help="print every guard, ARMED or DORMANT, resolved or not")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    goal_src = os.path.join(root, "goal_src", args.game)
    alltypes = os.path.join(root, "decompiler", "config", args.game, "all-types.gc")
    parent_of, fields_of, methods_of, states_of = cms.load_all_types(alltypes)
    rank = cms.load_link_order(os.path.join(goal_src, "dgos"), args.game_dgo)

    files = load_files(goal_src)
    landed_raw = {}     # path -> (obj, raw landed text, marker_line)
    landed_stripped = {}  # path -> comment-stripped landed text
    for obj, path in files:
        raw = cms.landed_text(path)
        marker_line, _ = landed_offset_and_text(open(path, encoding="utf-8", errors="replace").read())
        landed_raw[path] = (obj, raw, marker_line)
        landed_stripped[path] = cms.strip_comments(raw)

    # Filled method/state slots anywhere in landed goal_src: check_method_
    # slots.py's own logic, reused by calling its scan_file per file exactly
    # as its main() does.
    filled_methods = defaultdict(set)
    filled_states = defaultdict(set)
    for stripped in landed_stripped.values():
        filled_defs, _candidates = cms.scan_file(stripped, fields_of, methods_of, states_of, parent_of)
        for kind, ty, name in filled_defs:
            (filled_methods if kind == "method" else filled_states)[ty].add(name)

    descendants_of = defaultdict(set)
    for t in parent_of:
        for a in cms.ancestors(parent_of, t):
            descendants_of[a].add(t)

    # Function-shaped bindings anywhere in landed goal_src, and where their
    # own body text lives (defun/defbehavior/defmethod only; def-mips2c has
    # no GOAL body to scan).
    fn_defined = set()
    binding_sites = defaultdict(list)  # name -> [(path, self_type, body_start, body_end)]
    mips2c_only = set()
    for path, stripped in landed_stripped.items():
        for m in DEFUN_RE.finditer(stripped):
            name = m.group(1)
            fn_defined.add(name)
            end = cms.matching_close(stripped, m.start())
            binding_sites[name].append((path, None, body_span(stripped, m.start(), end, 1), end))
        for m in DEFUN_DEBUG_RE.finditer(stripped):
            name = m.group(1)
            fn_defined.add(name)
            end = cms.matching_close(stripped, m.start())
            binding_sites[name].append((path, None, body_span(stripped, m.start(), end, 1), end))
        for m in cms.DEFBEHAVIOR_RE.finditer(stripped):
            name = m.group(1)
            fn_defined.add(name)
            end = cms.matching_close(stripped, m.start())
            binding_sites[name].append((path, m.group(2), body_span(stripped, m.start(), end, 2), end))
        for m in cms.DEFMETHOD_RE.finditer(stripped):
            name = m.group(1)
            ty = m.group(2) or m.group(3)
            # group(2) set means the "((this TYPE) ...)" branch matched, where
            # the argument list itself is the very next top-level form after
            # NAME (no separate type token); group(3) set means the bare
            # "NAME TYPE (...)" branch, where TYPE is its own top-level token
            # ahead of the argument list.
            extra = 1 if m.group(2) else 2
            fn_defined.add(name)
            end = cms.matching_close(stripped, m.start())
            binding_sites[name].append((path, ty, body_span(stripped, m.start(), end, extra), end))
        for m in MIPS2C_RE.finditer(stripped):
            fn_defined.add(m.group(1))
            mips2c_only.add(m.group(1))

    # define-extern-only globals: declared somewhere, never given a value,
    # not function-typed or state-typed (see the docstring), not a kernel
    # builtin (see collect_kernel_builtins).
    kernel_builtins = collect_kernel_builtins(root, args.game)
    extern_all, function_typed, state_typed, given_value = set(), set(), set(), set()
    for stripped in landed_stripped.values():
        for m in DEFINE_EXTERN_RE.finditer(stripped):
            if SYM_RE.match(m.group(1)):
                extern_all.add(m.group(1))
        for m in DEFINE_EXTERN_FN_RE.finditer(stripped):
            function_typed.add(m.group(1))
        for m in DEFINE_EXTERN_STATE_RE.finditer(stripped):
            state_typed.add(m.group(1))
        for m in DEFINE_VALUE_RE.finditer(stripped):
            if SYM_RE.match(m.group(1)):
                given_value.add(m.group(1))
    extern_only_globals = extern_all - given_value - function_typed - state_typed - kernel_builtins

    fails, notes, report_rows = [], [], []
    dormant = uninspectable = 0
    for path, (obj, raw, marker_line) in landed_raw.items():
        stripped = landed_stripped[path]
        for m in OUTER_GUARD_RE.finditer(stripped):
            fn = m.group(2)
            if not SYM_RE.match(fn):
                continue
            form_end = cms.matching_close(stripped, m.start())
            protected = stripped[m.end():form_end]
            if not re.search(r"\(" + re.escape(fn) + r"\b", protected):
                continue
            line = marker_line + stripped.count("\n", 0, m.start())
            armed = fn in fn_defined
            if not armed:
                dormant += 1
                if args.report:
                    report_rows.append((obj, line, fn, "DORMANT", None))
                continue
            sites = binding_sites.get(fn, [])
            if not sites:
                uninspectable += 1
                if args.report:
                    where = "def-mips2c" if fn in mips2c_only else "unresolved binding"
                    report_rows.append((obj, line, fn, "ARMED (%s, not GOAL-inspectable)" % where, None))
                continue
            any_finding = False
            for bpath, self_type, bstart, bend in sites:
                body = landed_stripped[bpath][bstart:bend]
                risky = find_risky(body, self_type, fields_of, methods_of, states_of, parent_of,
                                    filled_methods, filled_states, descendants_of, extern_only_globals)
                for _kind, detail in risky:
                    any_finding = True
                    bobj = os.path.splitext(os.path.basename(bpath))[0]
                    msg = "%s:%d: guard on %s arms it live; %s's own body (%s) %s" % (
                        os.path.relpath(path, root).replace("\\", "/"), line, fn, fn, bobj, detail,
                    )
                    (fails if 0 in rank.get(obj, {}) else notes).append((obj, line, msg))
                    if args.report:
                        report_rows.append((obj, line, fn, "ARMED, UNRESOLVED", detail))
            if args.report and not any_finding:
                report_rows.append((obj, line, fn, "ARMED, resolved (guarded internally or nothing risky found)", None))

    if args.report:
        report_rows.sort()
        for obj, line, fn, status, detail in report_rows:
            extra = (": %s" % detail) if detail else ""
            print("%s:%d: (nonzero? %s) [%s]%s" % (obj, line, fn, status, extra))
        print(
            "check_guard_flip (%s): %d guard(s) inventoried, %d DORMANT, %d ARMED-not-inspectable"
            % (args.game, len(report_rows), dormant, uninspectable)
        )
        return 0

    fails.sort(key=lambda r: (r[0], r[1]))
    notes.sort(key=lambda r: (r[0], r[1]))
    for _obj, _line, msg in fails:
        print("FAIL:", msg)
    for _obj, _line, msg in notes:
        print("note:", msg)
    print(
        "check_guard_flip (%s): %d game-DGO FAIL(s), %d level-DGO note(s), "
        "%d DORMANT guard(s), %d ARMED-not-GOAL-inspectable guard(s)"
        % (args.game, len(fails), len(notes), dormant, uninspectable)
    )
    if args.strict and fails:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
