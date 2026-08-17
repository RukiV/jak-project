#!/usr/bin/env python3
"""Fail a virtual defstate whose parent state is not landed before it links.

Why this exists: a `(defstate NAME (TYPE) :virtual #t ...)` whose parent type
chain declares NAME as a state method does not stand alone. goalc
(goalc/compiler/compilation/State.cpp, define-virtual-state-hook) compiles it
into `(inherit-state <new> (method-of-type PARENT NAME))` at the file's top
level, so the parent's method slot is read while the object links. If no
ancestor's `(defstate NAME (ANCESTOR) ...)` has executed by then, the slot is 0
and inherit-state dereferences null: the boot dies at kernel link with a crash
report pointing into gstate and reading goal address 12 or 16, long after the
offending file printed its `[link and exec]` line. No compile gate sees this,
because the child compiles fine against a parent whose states are declared but
whose file is still a stub; only a boot does. That is exactly how
construction-obs-h took develop down on 2026-08-17 while every gate was green.

What it checks, per game (jakx by default): the type hierarchy and
`:state-methods` from decompiler/config/<game>/all-types.gc, every non-REF
defstate under goal_src/<game>, and the link order from goal_src/<game>/dgos.
For each virtual (non-override) defstate whose parent chain declares the state,
some ancestor's defstate must appear earlier in link order: an earlier object in
the same DGO listing, an object in the game DGO for a level object, or an
earlier form in the same file. Objects that are not in any DGO listing are
skipped (they never link).

Exit 1 with one line per violation; exit 0 when clean.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

DEFTYPE_RE = re.compile(r"^\(deftype\s+([^\s()]+)\s+\(([^\s()]+)\)", re.M)
STATE_METHODS_RE = re.compile(r"\(:state-methods(.*?)\n\s*\)", re.S)
METHOD_LINE_RE = re.compile(r"^\s+\(([^\s()]+)\s+\(", re.M)
DEFSTATE_RE = re.compile(r"^\(defstate\s+([^\s()]+)\s+\(([^\s()]+)\)", re.M)
VIRTUAL_RE = re.compile(r":virtual\s+([^\s()]+)")
DGO_OBJ_RE = re.compile(r'"([^"]+)\.o"')


def strip_comments(text):
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    return "\n".join(line.split(";", 1)[0] for line in text.split("\n"))


def load_types(alltypes_path):
    txt = strip_comments(open(alltypes_path, encoding="utf-8", errors="replace").read())
    parent_of = {}
    declared = defaultdict(set)  # type -> method and state-method names
    starts = [(m.start(), m.group(1), m.group(2)) for m in DEFTYPE_RE.finditer(txt)]
    starts.append((len(txt), None, None))
    for i in range(len(starts) - 1):
        start, name, parent = starts[i]
        body = txt[start:starts[i + 1][0]]
        parent_of[name] = parent
        sm = STATE_METHODS_RE.search(body)
        if sm:
            for ln in sm.group(1).split("\n"):
                tok = ln.strip()
                if not tok:
                    continue
                if tok.startswith("("):
                    tok = tok.strip("()").split()[0]
                declared[name].add(tok.split()[0])
        for mm in METHOD_LINE_RE.finditer(body):
            declared[name].add(mm.group(1))
    return parent_of, declared


def ancestors(parent_of, t):
    out = []
    seen = set()
    while t in parent_of and parent_of[t] is not None and t not in seen:
        seen.add(t)
        t = parent_of[t]
        out.append(t)
    return out


def load_link_order(dgos_dir, game_dgo):
    """Return {object: {dgo_index: position}}; the game DGO is index 0.

    An object can be listed in many DGOs (level DGOs repeat shared objects), and
    it links at a different position in each, so membership is kept per DGO.
    """
    rank = defaultdict(dict)
    files = [game_dgo] + sorted(f for f in os.listdir(dgos_dir)
                                if f.endswith(".gd") and f != game_dgo)
    for di, fn in enumerate(files):
        path = os.path.join(dgos_dir, fn)
        for pos, m in enumerate(DGO_OBJ_RE.finditer(open(path, encoding="utf-8").read())):
            rank[m.group(1)].setdefault(di, pos)
    return rank


def load_defstates(goal_src):
    """Return list of (state, type, virtual, object, position_in_file)."""
    out = []
    for dp, _, files in os.walk(goal_src):
        for fn in files:
            if not fn.endswith(".gc") or fn.endswith("_REF.gc"):
                continue
            text = open(os.path.join(dp, fn), encoding="utf-8", errors="replace").read()
            if ";; DECOMP BEGINS" in text:
                text = text.split(";; DECOMP BEGINS", 1)[1]
            for m in DEFSTATE_RE.finditer(text):
                head = text[m.end():m.end() + 400]
                vm = VIRTUAL_RE.search(head)
                out.append((m.group(1), m.group(2), vm.group(1) if vm else None,
                            fn[:-3], m.start()))
    return out


def links_before(rank, defs_of, obj, pos, state, anc_types):
    """True if, in every DGO listing `obj`, some ancestor defstate for `state`
    executes before (obj, pos): earlier in the same file, earlier in that DGO,
    or anywhere in the game DGO when `obj` is a level object."""
    for my_dgo, my_pos in rank[obj].items():
        ok = False
        for a in anc_types:
            for o, p in defs_of.get((state, a), []):
                if o == obj:
                    if p < pos:
                        ok = True
                    continue
                if o not in rank:
                    continue
                if my_dgo in rank[o] and rank[o][my_dgo] < my_pos:
                    ok = True
                if my_dgo != 0 and 0 in rank[o]:
                    ok = True
        if not ok:
            return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--game-dgo", default="game.gd")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    alltypes = os.path.join(root, "decompiler", "config", args.game, "all-types.gc")
    goal_src = os.path.join(root, "goal_src", args.game)
    parent_of, declared = load_types(alltypes)
    rank = load_link_order(os.path.join(goal_src, "dgos"), args.game_dgo)
    defs = load_defstates(goal_src)

    defs_of = defaultdict(list)
    for st, ty, _virt, obj, pos in defs:
        defs_of[(st, ty)].append((obj, pos))

    # Game-DGO objects link in exactly the listed order, so any miss there is a
    # certain crash. Level objects also depend on which other levels are resident
    # (borrow lists, common levels), which the .gd files alone do not encode, so
    # a level object only fails when the ancestor state is landed nowhere at all;
    # the rest are printed as notes for a human to weigh.
    violations, notes = [], []
    for st, ty, virt, obj, pos in defs:
        if virt != "#t" or obj not in rank:
            continue
        anc = ancestors(parent_of, ty)
        if not any(st in declared.get(a, ()) for a in anc):
            continue
        if links_before(rank, defs_of, obj, pos, st, anc):
            continue
        landed = [o for a in anc for o, _p in defs_of.get((st, a), [])]
        found = ", ".join(landed) or "nowhere in goal_src"
        entry = (min(rank[obj].items()), obj, st, ty, anc[0], found)
        if 0 in rank[obj] or not landed:
            violations.append(entry)
        else:
            notes.append(entry)

    for label, rows in (("note", sorted(notes)), ("FAIL", sorted(violations))):
        for (_dgo, _pos), obj, st, ty, parent, found in rows:
            print(f"{label}: {obj}: (defstate {st} ({ty}) :virtual #t) inherits {parent}'s {st} "
                  f"at link time; an ancestor defstate is landed {found}")
    if violations:
        print(f"virtual-state inheritance check ({args.game}): {len(violations)} violation(s), "
              f"{len(notes)} level-order note(s)")
        return 1
    print(f"virtual-state inheritance check ({args.game}): OK, {len(defs)} defstates against "
          f"{len(rank)} linked objects, {len(notes)} level-order note(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
