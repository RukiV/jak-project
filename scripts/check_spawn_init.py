#!/usr/bin/env python3
"""Report landed process-spawn sites whose init function is not landed anywhere.

Why this exists: `(process-spawn TYPE ...)` (goal_src/<game>/kernel/gstate.gc)
expands to `(run-now-in-process <new> TYPE-init-by-other ...)` unless an explicit
`:init FN` is given, and `process-spawn-function` runs the function it is handed.
The init symbol is a plain global reference: goalc compiles it fine whether or not
anything defines it, the linker does not care, and only at spawn time does the
kernel find a #f where a function should be and die with "attempting to run
nullptr function!". That is how the vehicle band's first boot smoke went down on
2026-08-17: vehicle-manager-start was landed (jakx-init calls it behind a runtime
`nonzero?` guard, so landing the starter alone flipped the guard on) while
vehicle-manager-init-by-other was omitted from the same file. Every compile gate
was green.

What it checks, per game (jakx by default): every non-REF .gc under goal_src/<game>
(the text below `;; DECOMP BEGINS`, comments stripped) for `(process-spawn TYPE
[:init FN] ...)` and `(process-spawn-function TYPE FN ...)`; the init symbol must be
defined by defbehavior, defun, defun-debug or define somewhere in a landed file
that any DGO listing links. A spawn whose type or init is an expression rather
than a symbol is skipped (the type is dynamic). Sites in objects the game DGO
links are FAILs, sites in level-only objects are notes, sites in objects no DGO
lists are ignored (they never link).

Modes: report (default) prints every finding and exits 0; --strict exits 1 when
any game-DGO FAIL exists, for a CI gate once the standing count is zero.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

DGO_OBJ_RE = re.compile(r'"([^"]+)\.o"')
DEF_RE = re.compile(r"\((?:defbehavior|defun|defun-debug|define|define-perm)\s+([^\s()]+)")
SPAWN_RE = re.compile(r"\((process-spawn|process-spawn-function)\s+")
SYM_RE = re.compile(r"^[A-Za-z0-9!?*<>=+/._%&$#-]+$")


def strip_comments(text):
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    return "\n".join(line.split(";", 1)[0] for line in text.split("\n"))


def landed_text(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    if ";; DECOMP BEGINS" in text:
        text = text.split(";; DECOMP BEGINS", 1)[1]
    return strip_comments(text)


def load_link_order(dgos_dir, game_dgo):
    rank = defaultdict(dict)
    files = [game_dgo] + sorted(f for f in os.listdir(dgos_dir) if f.endswith(".gd") and f != game_dgo)
    for di, fn in enumerate(files):
        for pos, m in enumerate(DGO_OBJ_RE.finditer(open(os.path.join(dgos_dir, fn), encoding="utf-8").read())):
            rank[m.group(1)].setdefault(di, pos)
    return rank


def matching_close(text, open_idx):
    depth = 0
    in_str = False
    i = open_idx
    while i < len(text):
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
    return len(text)


def tokens_of(form):
    """Split a form body into top-level tokens (symbols, strings, or whole sub-forms)."""
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


def spawn_sites(text):
    """Yield (kind, type_token, init_token_or_None) for every spawn form."""
    for m in SPAWN_RE.finditer(text):
        close = matching_close(text, m.start())
        body = text[m.end():close]
        toks = tokens_of(body)
        if not toks:
            continue
        kind = m.group(1)
        ty = toks[0]
        init = None
        if kind == "process-spawn-function":
            init = toks[1] if len(toks) > 1 else None
        else:
            for k, t in enumerate(toks):
                if t == ":init" and k + 1 < len(toks):
                    init = toks[k + 1]
            if init is None and SYM_RE.match(ty):
                init = ty + "-init-by-other"
        yield kind, ty, init


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--strict", action="store_true", help="exit 1 when any game-DGO FAIL exists")
    args = ap.parse_args()

    goal_src = os.path.join(args.root, "goal_src", args.game)
    dgos_dir = os.path.join(goal_src, "dgos")
    rank = load_link_order(dgos_dir, "game.gd")

    files = []
    for dp, _d, fns in os.walk(goal_src):
        for fn in fns:
            if fn.endswith(".gc") and not fn.endswith("_REF.gc"):
                files.append((fn[:-3], os.path.join(dp, fn)))
    texts = {obj: landed_text(p) for obj, p in files}

    defined = set()
    for obj, text in texts.items():
        if obj in rank:
            defined.update(DEF_RE.findall(text))

    fails, notes, skipped = [], [], 0
    for obj, text in sorted(texts.items()):
        if obj not in rank:
            continue
        for kind, ty, init in spawn_sites(text):
            if init is None or not SYM_RE.match(init) or not SYM_RE.match(ty):
                skipped += 1
                continue
            if init in defined:
                continue
            msg = f"{obj}: ({kind} {ty} ...) runs {init}, which no landed object in any DGO defines"
            (fails if 0 in rank[obj] else notes).append(msg)

    for m in fails:
        print("FAIL:", m)
    for m in notes:
        print("note:", m)
    print(f"check_spawn_init ({args.game}): {len(fails)} game-DGO FAIL(s), {len(notes)} level-DGO note(s), "
          f"{skipped} dynamic site(s) skipped")
    if args.strict and fails:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
