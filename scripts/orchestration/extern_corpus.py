"""Shared parsing library for the untyped-extern factory tooling (extern_ledger.py,
extern_evidence.py, extern_verify.py). Nothing in here writes anywhere; it only reads a
full-corpus decode dir of "<obj>_ir2.asm" files and a worktree's all-types.gc files.

Two independent things live here, both read-only:

1. ir2.asm corpus parsing: split each decode file into function blocks (delimited by
   "; .function NAME" header lines through the next header or EOF), classify every
   ";; ERROR:" warning line into one of the seven attribution-eligible classes, and for
   the six INDIRECT classes walk backward through the function's own asm body for the
   nearest "lw REG, SYMBOL(s7)" load to name the blocking symbol (the DIRECT class names
   its symbol already, in the error text itself).

   The seven classes (matched on the literal warning text, verified against the corpus
   2026-08-19):
     DIRECT:
       "Function X has unknown type"                -> symbol = X, no walkback. X in
         "(method N type)" form is a vtable slot, not a define-extern symbol, and is
         excluded from attribution (tracked separately as excluded_method_form).
     INDIRECT (all begin "failed type prop at N: ..."; walk back from op N):
       "... Called a function, but we do not know its type"
       "... add failed: object <...>"
       "... add failed: <uninitialized> <...>"
       "... add failed: basic <...>"
       "... Call to run-function-in-process or set-to-run ... invalid function type: ..."
       "... Called something that was not a function: object"

   The walkback is a bounded, undirected textual scan (nearest preceding "lw REG,
   SYMBOL(s7)" within WALKBACK_WINDOW physical lines, not a register dataflow trace): this
   matches the brief's stated method and is intentionally simple. It legitimately misses
   cases where the failing register traces to a method-vtable load, a stack value, or a
   parameter rather than a global symbol (verified by hand: actor-link-h_ir2.asm's method
   18 "not a function: object" at op 24 traces back to a1-0, a parameter register, with no
   lw ...(s7) in the window at all -- correctly unattributed) -- an unresolved walkback is
   evidence the error is not extern-caused, not a parser bug.

2. all-types.gc extern declaration parsing: every "(define-extern NAME TYPE)" form,
   active or commented out, single-line or wrapped across lines with a docstring (GOAL
   convention: TYPE is always the LAST top-level element of the form, whether or not a
   docstring string literal sits between NAME and TYPE). "Last active declaration wins"
   is the real goalc semantic (a later define-extern silently overrides an earlier one at
   actual compile time; see the provisional-externs bucket's own comments in
   decompiler/config/jakx/all-types.gc around line 86023), so this module tracks, per
   symbol, only the LAST active (non-commented) declaration's type; commented-out
   remnants are reported separately as informational only.
"""
import bisect
import os
import re


WALKBACK_WINDOW = 12

FUNCTION_HEADER_RE = re.compile(r"^; \.function (.+)$")
OP_TAG_RE = re.compile(r";;\s*\[\s*(\d+)\]")
LW_SYM_RE = re.compile(r"\blwu?\s+\S+,\s*([^\s(),]+)\(s7\)")
SYMBOL_TOKEN_RE = re.compile(r"^[A-Za-z*+\-/<>=!?._'][\w*+\-/<>=!?._'#]*$")
CALL_BANG_RE = re.compile(r"\(call!([^)]*)\)")
CALL_ARG_REG_RE = re.compile(r"\b(?:a[0-3]|t[0-3])-\d+\b")

DIRECT_UNKNOWN_TYPE_RE = re.compile(r"^;; ERROR: Function (.+) has unknown type\s*$")
METHOD_FORM_RE = re.compile(r"^\(method \d+ \S+\)$")
FAILED_TYPE_PROP_RE = re.compile(r"^;; ERROR: failed type prop at (\d+): (.+)$")
ANY_ERROR_RE = re.compile(r"^;; ERROR:")

# order matters: check the more specific "add failed: X" variants before any looser match.
INDIRECT_CLASSIFIERS = (
    ("call-unknown-type", lambda d: "we do not know its type" in d),
    ("add-failed-object", lambda d: d.startswith("add failed: object")),
    ("add-failed-uninitialized", lambda d: d.startswith("add failed: <uninitialized>")),
    ("add-failed-basic", lambda d: d.startswith("add failed: basic")),
    ("invalid-function-type", lambda d: "invalid function type" in d),
    ("not-a-function-object", lambda d: "not a function: object" in d),
)

ALL_ERROR_CLASSES = ("unknown-type-direct",) + tuple(c for c, _ in INDIRECT_CLASSIFIERS)


class ExternCorpusError(Exception):
    """Base for this module's own fail-fast parse errors."""


class AllTypesParseError(ExternCorpusError):
    """A (define-extern ...) form in an all-types.gc did not parse: either a genuinely
    new convention this scanner does not yet handle, or a real syntax problem. Either
    way it is not safe to silently skip, since a skip here silently drops a symbol from
    the ledger's provenance."""


class Ir2ParseError(ExternCorpusError):
    """An ir2.asm file's structure did not match what this scanner expects."""


def _line_index(newline_offsets, pos):
    """1-based line number for a character offset, from a precomputed sorted list of
    every '\\n' offset in the text (bisect gives O(log n) instead of a fresh O(n)
    text.count('\\n', 0, pos) per call, which matters at ~6000 calls per 87k-line file)."""
    return bisect.bisect_right(newline_offsets, pos) + 1


def classify_error_line(text):
    """text is the full ';; ERROR: ...' line (no trailing newline). Returns one of:
      ("direct", symbol, None) for a directly-named function, symbol NOT a method form
      ("direct-method-form", raw_name, None) for a "(method N type)" direct name
      ("indirect", class_name, op_index) for a walkback-eligible class
      None if the line is out of scope for this ledger (not one of the seven classes)
    """
    m = DIRECT_UNKNOWN_TYPE_RE.match(text)
    if m:
        name = m.group(1).strip()
        if METHOD_FORM_RE.match(name):
            return ("direct-method-form", name, None)
        return ("direct", name, None)
    m = FAILED_TYPE_PROP_RE.match(text)
    if m:
        op_index = int(m.group(1))
        detail = m.group(2)
        for cls, pred in INDIRECT_CLASSIFIERS:
            if pred(detail):
                return ("indirect", cls, op_index)
    return None


def _iter_function_blocks(lines):
    """Yield (name, is_top_level_login, start_idx, end_idx) 0-based, end_idx exclusive,
    for every "; .function NAME" header found in `lines`."""
    headers = []
    for i, ln in enumerate(lines):
        m = FUNCTION_HEADER_RE.match(ln)
        if m:
            headers.append((i, m.group(1).strip()))
    for idx, (start, name) in enumerate(headers):
        end = headers[idx + 1][0] if idx + 1 < len(headers) else len(lines)
        is_tll = name.startswith("(top-level-login ")
        yield name, is_tll, start, end


def parse_ir2_file(path, object_name):
    """Parse one <object>_ir2.asm file into a list of block dicts:
      {object, function, is_top_level_login, start_line, end_line (1-based, inclusive),
       error_line_count, occurrences, excluded_method_form, unresolved}

    occurrences: [{error_class, error_line, op_index, symbol, attribution_line, detail}]
      attribution_line is the 1-based line the symbol was read off: the error line
      itself for the direct class, or the "lw ...(s7)" line found by walkback for an
      indirect class.
    excluded_method_form: [{error_line, name}] -- direct "has unknown type" on a
      "(method N type)" name; not a define-extern symbol, cannot attribute.
    unresolved: [{error_line, error_class, op_index, reason}] -- in-scope but the
      walkback found no symbol (or the referenced op index does not exist in this
      function), or is direct-with-no-name-mismatch (should not happen; loud if it does).
    """
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    lines = text.split("\n")

    blocks = []
    for name, is_tll, start, end in _iter_function_blocks(lines):
        block_lines = lines[start:end]
        op_map = {}
        for i, ln in enumerate(block_lines):
            m = OP_TAG_RE.search(ln)
            if m:
                op_index = int(m.group(1))
                if op_index not in op_map:
                    op_map[op_index] = i
        error_line_count = sum(1 for ln in block_lines if ANY_ERROR_RE.match(ln))

        occurrences = []
        excluded_method_form = []
        unresolved = []
        for i, ln in enumerate(block_lines):
            if not ANY_ERROR_RE.match(ln):
                continue
            classified = classify_error_line(ln.rstrip())
            if classified is None:
                continue
            kind = classified[0]
            error_line_1based = start + i + 1
            if kind == "direct-method-form":
                excluded_method_form.append({"error_line": error_line_1based, "name": classified[1]})
                continue
            if kind == "direct":
                occurrences.append({
                    "error_class": "unknown-type-direct",
                    "error_line": error_line_1based,
                    "op_index": None,
                    "symbol": classified[1],
                    "attribution_line": error_line_1based,
                    "detail": ln.strip(),
                })
                continue
            # indirect
            _, cls, op_index = classified
            op_line_idx = op_map.get(op_index)
            if op_line_idx is None:
                unresolved.append({"error_line": error_line_1based, "error_class": cls,
                                    "op_index": op_index, "reason": "op index not found in function body"})
                continue
            lo = max(0, op_line_idx - WALKBACK_WINDOW)
            found_symbol = None
            found_line_idx = None
            for j in range(op_line_idx - 1, lo - 1, -1):
                m = LW_SYM_RE.search(block_lines[j])
                if m and SYMBOL_TOKEN_RE.match(m.group(1)):
                    found_symbol = m.group(1)
                    found_line_idx = j
                    break
            if found_symbol is None:
                unresolved.append({"error_line": error_line_1based, "error_class": cls,
                                    "op_index": op_index, "reason": "no lw SYM(s7) within walkback window"})
                continue
            occurrences.append({
                "error_class": cls,
                "error_line": error_line_1based,
                "op_index": op_index,
                "symbol": found_symbol,
                "attribution_line": start + found_line_idx + 1,
                "detail": ln.strip(),
            })

        blocks.append({
            "object": object_name,
            "function": name,
            "is_top_level_login": is_tll,
            "start_line": start + 1,
            "end_line": end,
            "error_line_count": error_line_count,
            "occurrences": occurrences,
            "excluded_method_form": excluded_method_form,
            "unresolved": unresolved,
        })
    return blocks


def iter_ir2_files(decode_dir):
    """Sorted list of (object_name, path) for every "<object>_ir2.asm" directly under
    decode_dir. Deterministic order (sorted by object name) so every downstream JSON/MD
    render is stable across reruns."""
    out = []
    for fn in sorted(os.listdir(decode_dir)):
        if fn.endswith("_ir2.asm"):
            out.append((fn[: -len("_ir2.asm")], os.path.join(decode_dir, fn)))
    return out


def call_site_arity(call_bang_text):
    """Number of distinct argument-register tokens (a0-3/t0-3, version-suffixed like
    'a0-24') inside a "(call! ...)" annotation's argument list text. "(call!)" -> 0."""
    return len(CALL_ARG_REG_RE.findall(call_bang_text))


SET_REG_RE = re.compile(r";;\s*\[\s*\d+\]\s*\(set!\s+(a[0-3]|t[0-3])(?:-\d+)?\b")
ARG_REG_ORDER = ("a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3")


def register_arity(lines, lw_idx, jalr_idx, window=WALKBACK_WINDOW):
    """Observed argument count from the individual "(set! aN ...)"/"(set! tN ...)" IR
    annotations on the lines between the nearest preceding jalr (or lw_idx - window,
    whichever bound is closer -- never crossing into an earlier, unrelated call's own
    argument setup) and jalr_idx, exclusive. Returned as the size of the largest
    CONTIGUOUS canonical-order prefix of {a0,a1,a2,a3,t0,t1,t2,t3} actually seen (GOAL's
    calling convention fills positional args in order, so a real arity leaves no gap).
    None if the scanned window carries NO op-tag annotations at all (see below): the
    difference between "the decompiler counted zero argument registers" (a real 0) and
    "this whole region is bare, un-annotated disassembly" (no signal) matters, and only
    the op-tag check tells them apart.

    This reads each argument-setup instruction's OWN annotation, not the jalr's "(call!
    ...)" summary (see call_site_arity): a function whose own type propagation has
    already failed can still annotate individual "(set! aN ...)" assignments correctly
    while leaving the jalr's own call! summary empty. Verified on real corpus data,
    collide-cache_ir2.asm's closest-pt-in-triangle call site at line 724: the jalr's own
    comment is a bare "(call!)" (call_site_arity would read 0) despite a0 through a3
    each carrying an individual "(set! aN ...)" annotation on lines 720-723
    (register_arity correctly reads 4). extern_verify.py's own red-green check caught
    call_site_arity alone turning this into a false contradiction against a correct
    4-argument proposal; register_arity is what it actually verifies against.

    A second, related false-zero was caught the same way: closest-pt-in-triangle's call
    sites at collide-cache_ir2.asm:1265 and collide-mesh_ir2.asm:3175 sit in fully bare
    disassembly (no "[N]" op-tag anywhere in range at all, not even on unrelated
    instructions) -- an early version of this function returned a bare 0 for these too,
    which is not "zero arguments observed", it is "nothing was observed". The op-tag
    check below makes that case None instead."""
    lo = max(0, lw_idx - window)
    for k in range(jalr_idx - 1, lo - 1, -1):
        if "jalr ra, t9" in lines[k]:
            lo = k + 1
            break
    any_annotation = False
    seen = set()
    for k in range(lo, jalr_idx):
        if OP_TAG_RE.search(lines[k]):
            any_annotation = True
        m = SET_REG_RE.search(lines[k])
        if m:
            seen.add(m.group(1))
    if not any_annotation:
        return None
    n = 0
    for reg in ARG_REG_ORDER:
        if reg not in seen:
            break
        n += 1
    return n


def find_call_sites(lines, symbol, forward_window=WALKBACK_WINDOW):
    """Every call site of `symbol` in a flat list of file lines (0-based): each is a
    "lw t9, SYMBOL(s7)" load paired with the nearest FOLLOWING "jalr ra, t9" within
    forward_window lines (bounded window, matching the walkback window's size since both
    describe "how far apart do coupled instructions realistically fall").

    Returns a list of dicts: {lw_line_idx, jalr_line_idx, arity, call_bang_text,
    register_arity}. arity/call_bang_text are None if no jalr was found in range (a load
    that was never actually called in this window; still reported, with
    jalr_line_idx=None, rather than silently dropped). register_arity is always
    computed once a jalr is found, independent of whether the jalr's own annotation
    parsed (see register_arity's docstring for why the two can disagree).
    """
    sym_re = re.compile(r"\blw\s+t9,\s*" + re.escape(symbol) + r"\(s7\)")
    sites = []
    for i, ln in enumerate(lines):
        if not sym_re.search(ln):
            continue
        jalr_idx = None
        arity = None
        call_text = None
        reg_arity = None
        hi = min(len(lines), i + 1 + forward_window)
        for j in range(i + 1, hi):
            if "jalr ra, t9" in lines[j]:
                jalr_idx = j
                m = CALL_BANG_RE.search(lines[j])
                if m:
                    call_text = m.group(0)
                    arity = call_site_arity(m.group(1))
                reg_arity = register_arity(lines, i, jalr_idx, forward_window)
                break
        sites.append({"lw_line_idx": i, "jalr_line_idx": jalr_idx, "arity": arity,
                       "call_bang_text": call_text, "register_arity": reg_arity})
    return sites


# ---------------------------------------------------------------------------
# all-types.gc extern declaration scanning
# ---------------------------------------------------------------------------

ACTIVE_DEFINE_EXTERN_START_RE = re.compile(r"^\(define-extern\b", re.MULTILINE)
COMMENTED_BARE_RE = re.compile(
    r"^;;\s*\(define-extern\s+(\S+)\s+(function|object)\)\s*(?:;;.*)?$", re.MULTILINE)

MAX_FORM_SCAN_CHARS = 20000  # a real form is a few hundred chars at most; this is a trip wire


def split_top_level(s):
    """Top-level whitespace-split of an s-expression body, respecting nested parens and
    double-quoted strings (with backslash escaping) so "(function object object)" and
    "\"a docstring with (parens) and spaces\"" each come back as one token.

    A "(" is a token boundary on its own even with no preceding whitespace (the real
    GOAL/Lisp reader rule): jak2's all-types.gc:30500 has
    "cam-launcher-long-joystick(function vector :behavior camera-slave))" with zero
    space between the name and the type, and a naive whitespace-only split would glue
    them into one bogus element."""
    tokens = []
    cur = []
    depth = 0
    in_string = False
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if in_string:
            cur.append(c)
            if c == "\\" and i + 1 < n:
                cur.append(s[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if c == '"':
            in_string = True
            cur.append(c)
        elif c == "(":
            if depth == 0 and cur:
                tokens.append("".join(cur))
                cur = []
            depth += 1
            cur.append(c)
        elif c == ")":
            depth -= 1
            cur.append(c)
            if depth == 0:
                tokens.append("".join(cur))
                cur = []
        elif c.isspace() and depth == 0:
            if cur:
                tokens.append("".join(cur))
                cur = []
        else:
            cur.append(c)
        i += 1
    if cur:
        tokens.append("".join(cur))
    return tokens


def _split_define_extern_form(form_text, line_no):
    """form_text is the full "(define-extern ... )" form, balanced. -> (name, type_str).
    GOAL convention: the form is (define-extern NAME [DOCSTRING] TYPE); TYPE is always
    the last top-level element regardless of whether a docstring is present."""
    if not (form_text.startswith("(") and form_text.endswith(")")):
        raise AllTypesParseError(f"line {line_no}: form does not start/end with parens: {form_text[:80]!r}")
    inner = form_text[1:-1]
    if not inner.startswith("define-extern"):
        raise AllTypesParseError(f"line {line_no}: form does not start with define-extern: {form_text[:80]!r}")
    rest = inner[len("define-extern"):]
    elements = split_top_level(rest)
    if len(elements) not in (2, 3):
        raise AllTypesParseError(
            f"line {line_no}: expected (define-extern NAME [DOCSTRING] TYPE), got "
            f"{len(elements)} top-level element(s): {elements!r}")
    return elements[0], elements[-1]


def parse_active_define_externs(path):
    """-> dict[name] = (type_str, line_no) using LAST-active-declaration-wins (the real
    goalc semantic; see module docstring). Every active (define-extern ...) form is
    balanced with a string-aware paren scan, so docstring-wrapped multi-line forms
    parse the same as single-line ones."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    newline_offsets = [i for i, c in enumerate(text) if c == "\n"]
    last = {}
    for m in ACTIVE_DEFINE_EXTERN_START_RE.finditer(text):
        start = m.start()
        line_no = _line_index(newline_offsets, start)
        depth = 0
        in_string = False
        k = start
        n = min(len(text), start + MAX_FORM_SCAN_CHARS)
        end = None
        while k < n:
            c = text[k]
            if in_string:
                if c == "\\":
                    k += 2
                    continue
                if c == '"':
                    in_string = False
                k += 1
                continue
            if c == '"':
                in_string = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    end = k + 1
                    break
            k += 1
        if end is None:
            raise AllTypesParseError(
                f"{path}:{line_no}: (define-extern form did not balance within "
                f"{MAX_FORM_SCAN_CHARS} chars; a new all-types.gc convention this "
                f"scanner does not handle, not something to skip silently")
        form_text = text[start:end]
        name, type_str = _split_define_extern_form(form_text, line_no)
        last[name] = (type_str, line_no)  # later match in file order overwrites: last wins
    return last


def parse_commented_bare_externs(path):
    """-> dict[name] = [line_no, ...] for every fully-commented ";; (define-extern NAME
    function)" / "... object)" remnant line. Informational only (goalc never sees a
    commented-out form); used to flag symbols whose only trace of a declaration at all
    is a historical, inactive one."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    newline_offsets = [i for i, c in enumerate(text) if c == "\n"]
    out = {}
    for m in COMMENTED_BARE_RE.finditer(text):
        name = m.group(1)
        line_no = _line_index(newline_offsets, m.start())
        out.setdefault(name, []).append(line_no)
    return out
