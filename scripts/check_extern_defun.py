#!/usr/bin/env python3
"""Flag a define-extern function symbol that is called from landed code but
bound by nothing goalc will ever link.

Why this exists: `(define-extern NAME (function ...))` only tells the compiler
NAME's type, so a call site `(NAME ...)` type-checks and compiles clean no
matter whether anything ever supplies NAME's value. If no `defun`,
`defun-debug`, `defbehavior`, `def-mips2c` or lambda-valued `define` /
`define-perm` for NAME links anywhere in the tree, the linker leaves NAME's
symbol slot at its zero default, and the call becomes a jump to goal address 0
the first time it actually runs. No compile gate sees this: (mi) and
goalc-test both only check that the call's *type* resolves, never that its
*value* does. This is exactly how find-nearest-camera-distance took a boot
down on 2026-08-18: declared via define-extern in three files (vehicle-debris,
wvehicle-wheel, race-mesh) and called from landed race-mesh method 10, with no
defun anywhere until drawable.gc's own decode landed one and retired the case.
`defmethod` does not count as a binding here: a method lives in its type's own
method table, not the global symbol table, so it can never satisfy a plain
`(NAME ...)` call regardless of how many types define a method of that name.

C++ kernel builtins are the mechanical false-positive class. compiler-setup.gc
loads exactly one file through `(asm-file ...)`, goal_src/<game>/kernel-defs.gc,
whose own header says what it is: "everything defined in the C Kernel /
runtime... these types/functions are provided by <game>'s runtime." Every
define-extern function symbol declared inside an asm-file'd file is backed by
the C++ runtime rather than by goal_src, so this checker reads the asm-file
list out of compiler-setup.gc itself (rather than hardcoding the filename) and
treats every function extern declared in those files as defined by
construction.

What it checks, per game (jakx by default): every `(define-extern NAME
(function ...))` under goal_src/<game> (any file, declaration position; a
docstring between NAME and the type is allowed, matching kernel-defs.gc's own
style); whether NAME is bound anywhere in goal_src by `defun`, `defun-debug`,
`defbehavior`, `def-mips2c`, or a `define` / `define-perm` whose value is a
`(lambda ...)` form (directly or under one `(the-as TYPE ...)` / `(the TYPE
...)` wrapper); and, for every NAME left unbound and not a kernel builtin,
every real call site `(NAME ...)` in the landed text (below `;; DECOMP BEGINS`
when that marker is present, comments stripped) of a `.gc` file that some DGO
actually links. A call is `(NAME ...)` at expression position: an open paren
immediately followed by NAME. Declarations never match this shape (NAME
follows `define-extern`, not an open paren), and block comments, line
comments and therefore og:preserve-this comment blocks are stripped before the
call scan runs, so prose mentions of NAME inside commentary are invisible to
it.

Findings are grouped by the calling object (the .gc file's own name, matching
the DGO listings), the same split check_spawn_init.py and check_method_slots.py
use: an object linked into the game DGO is on the boot path, so an unresolved
call from it is a FAIL; an object linked only into level DGOs is a note,
because level residency depends on borrow lists the .gd files alone do not
encode; an object no DGO lists never links, so calls from it are ignored.

Modes: report (default) prints every finding and exits 0; --strict exits 1
when any game-DGO FAIL exists, for a CI gate once the standing count is zero.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

DGO_OBJ_RE = re.compile(r'"([^"]+)\.o"')
ASM_FILE_RE = re.compile(r'\(asm-file\s+"([^"]+)"')
DEFINE_EXTERN_FN_RE = re.compile(
    r'\(define-extern\s+([^\s()]+)\s+(?:"(?:[^"\\]|\\.)*"\s+)?\(function\b'
)
# Unconditional definers: each of these forms always binds NAME to a real
# function value, whatever its body looks like.
DEF_RE = re.compile(r"\((?:defun|defun-debug|defbehavior|def-mips2c)\s+([^\s()]+)")
# Conditional definers: `define` and `define-perm` bind NAME to whatever value
# expression follows, so NAME only counts as a function binding when that
# value is itself a lambda. `define-perm` carries an extra TYPE token before
# the value (`(define-perm NAME TYPE VALUE)`); `define` does not.
DEFINE_HEAD_RE = re.compile(r"\((define|define-perm)\s+([^\s()]+)\s+")
SYM_RE = re.compile(r"^[A-Za-z0-9!?*<>=+/._%&$#-]+$")


def strip_comments(text):
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    return "\n".join(line.split(";", 1)[0] for line in text.split("\n"))


def matching_close(text, open_idx):
    depth = 0
    in_str = False
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n


def tokens_of(form):
    """Split a form body into top-level tokens (symbols, strings, whole
    sub-forms). Copied from check_spawn_init.py's helper of the same name."""
    out = []
    i = 0
    while i < len(form):
        c = form[i]
        if c.isspace():
            i += 1
        elif c == "(":
            j = matching_close(form, i)
            out.append(form[i:j + 1])
            i = j + 1
        elif c == '"':
            j = i + 1
            while j < len(form) and form[j] != '"':
                j += 2 if form[j] == "\\" else 1
            out.append(form[i:j + 1])
            i = j + 1
        else:
            j = i
            while j < len(form) and not form[j].isspace() and form[j] not in "()":
                j += 1
            out.append(form[i:j])
            i = j
    return out


def is_lambda_value(tok):
    """True if a define/define-perm value token is a lambda, directly or
    under one (the-as TYPE ...) / (the TYPE ...) cast wrapper."""
    if tok.startswith("(lambda"):
        return True
    m = re.match(r"\(the-as\s+[^\s()]+\s+(.*)\)$", tok, re.S)
    if not m:
        m = re.match(r"\(the\s+[^\s()]+\s+(.*)\)$", tok, re.S)
    return bool(m) and m.group(1).lstrip().startswith("(lambda")


def line_of(text, idx):
    return text.count("\n", 0, idx) + 1


def landed_offset_and_text(text):
    """(line_of_marker, landed_substring). line_of_marker is the 1-based line
    number the returned substring's own line 1 corresponds to in the original
    file, so a match offset found in the substring can be translated back to
    a real file:line. No marker means the whole file is landed."""
    idx = text.find(";; DECOMP BEGINS")
    if idx == -1:
        return 1, text
    after = text.find("\n", idx)
    after = idx if after == -1 else after + 1
    return text.count("\n", 0, after) + 1, text[after:]


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


def collect_asm_files(compiler_setup_path, root):
    text = open(compiler_setup_path, encoding="utf-8", errors="replace").read()
    return [os.path.join(root, m.group(1).replace("/", os.sep)) for m in ASM_FILE_RE.finditer(text)]


def collect_definitions(stripped_text):
    """(extern_fn_names, defined_names) found anywhere in one file's
    comment-stripped text."""
    externs = set(DEFINE_EXTERN_FN_RE.findall(stripped_text))
    defined = set(DEF_RE.findall(stripped_text))
    for m in DEFINE_HEAD_RE.finditer(stripped_text):
        kind, name = m.group(1), m.group(2)
        form_end = matching_close(stripped_text, m.start())
        body = stripped_text[m.end():form_end]
        toks = tokens_of(body)
        val_idx = 1 if kind == "define-perm" else 0  # skip the extra TYPE token
        if len(toks) > val_idx and is_lambda_value(toks[val_idx]):
            defined.add(name)
    return externs, defined


def find_call_sites(landed_stripped_text, names):
    """{name: [line, ...]} for every `(NAME ...)` at expression position."""
    hits = defaultdict(list)
    for m in re.finditer(r"\(([A-Za-z0-9!?*<>=+/._%&$#-]+)(?=[\s)])", landed_stripped_text):
        name = m.group(1)
        if name in names:
            hits[name].append(line_of(landed_stripped_text, m.start()))
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--strict", action="store_true", help="exit 1 when any game-DGO FAIL exists")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    goal_src = os.path.join(root, "goal_src", args.game)
    compiler_setup = os.path.join(goal_src, "compiler-setup.gc")
    rank = load_link_order(os.path.join(goal_src, "dgos"), "game.gd")

    builtin_files = set(os.path.normcase(os.path.normpath(p)) for p in collect_asm_files(compiler_setup, root))

    files = []
    for dp, _d, fns in os.walk(goal_src):
        for fn in fns:
            if fn.endswith(".gc") and not fn.endswith("_REF.gc"):
                files.append((fn[:-3], os.path.join(dp, fn)))

    raw_texts = {}
    for obj, path in files:
        raw_texts[path] = (obj, open(path, encoding="utf-8", errors="replace").read())

    all_externs = set()
    defined = set()
    builtin_externs = set()
    decl_files = defaultdict(list)  # extern name -> [obj, ...] that declare it

    for path, (obj, raw) in raw_texts.items():
        stripped = strip_comments(raw)
        externs, file_defined = collect_definitions(stripped)
        for name in externs:
            if not SYM_RE.match(name):
                continue
            all_externs.add(name)
            decl_files[name].append(obj)
            if os.path.normcase(os.path.normpath(path)) in builtin_files:
                builtin_externs.add(name)
        defined.update(n for n in file_defined if SYM_RE.match(n))

    unresolved = all_externs - defined - builtin_externs

    fails, notes = [], []
    skipped_no_dgo = 0
    for path, (obj, raw) in raw_texts.items():
        if obj not in rank:
            skipped_no_dgo += 1
            continue
        marker_line, landed = landed_offset_and_text(raw)
        stripped_landed = strip_comments(landed)
        hits = find_call_sites(stripped_landed, unresolved)
        for name, lines in hits.items():
            decl_note = "declared in %s" % ", ".join(sorted(set(decl_files[name])))
            for rel_line in lines:
                abs_line = marker_line + rel_line - 1
                msg = "%s:%d: (%s ...) calls %s, %s, but no defun/defbehavior/def-mips2c/lambda-define binds it anywhere in goal_src" % (
                    os.path.relpath(path, root).replace("\\", "/"), abs_line, name, name, decl_note,
                )
                (fails if 0 in rank[obj] else notes).append((obj, abs_line, msg))

    fails.sort(key=lambda r: (r[0], r[1]))
    notes.sort(key=lambda r: (r[0], r[1]))
    for _obj, _line, msg in fails:
        print("FAIL:", msg)
    for _obj, _line, msg in notes:
        print("note:", msg)
    print(
        "check_extern_defun (%s): %d game-DGO FAIL(s), %d level-DGO note(s), "
        "%d function extern(s) total, %d unresolved, %d kernel builtin(s) from %d asm-file(s)"
        % (args.game, len(fails), len(notes), len(all_externs), len(unresolved), len(builtin_externs), len(builtin_files))
    )
    if args.strict and fails:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
