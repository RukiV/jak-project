#!/usr/bin/env python3
"""Fail a type whose method landings link before its vtable gets rewiped.

Why this exists (issue 534): new_type (game/kernel/jakx/kscheme.cpp:1049)
unconditionally rewrites every method slot on EVERY execution of `deftype`
for the same interned type object: parent methods for inherited slots,
literal 0 for every custom slot, no matter what a defmethod already put
there. A type deftyped identically in more than one object is therefore not
a harmless convention, it is a live vtable wipe waiting on link order: if any
object holding a `defmethod` for that type links before the LAST object that
redefines the type, the later `deftype` execution zeros the method's slot
again the moment it links, and every landed method silently becomes
undispatchable (jumps to goal address 0) the first time something calls it.
No compile gate sees this: (mi) and offline-test both check each object in
isolation, never the cross-object link order the kernel actually replays at
boot. This is exactly how gui-control's methods 9, 11, 12, 14, 15, 16, 17,
19 and 25 (landed in loader.o, link position 248) went dark at runtime:
cam-states.o, an identical, later-linking copy of the same deftype (link
position 467), blanked every one of them again on its own link.

What it checks, scoped to goal_src/jakx only: the object link order from
goal_src/jakx/dgos/game.gd (the shared ENGINE/GAME pool every one of these
files links into; level DGOs are out of scope, matching the brief this
checker was written against), and every column-0 `(deftype NAME ...)` and
`(defmethod ... NAME ...)` (either `(defmethod METHOD ((this TYPE) ...) ...)`
or the constructor shape `(defmethod METHOD TYPE (...) ...)`) landed under
goal_src/jakx (non-REF sources only; a `_REF.gc` dump is decompiler output,
never links, and duplicating its own deftype is not a real link-order
defect). Source files map to DGO objects by filename, matching every DGO
listing's own `"name.o"` convention (gui-h.gc is gui-h.o, and so on).

A type name is DUPLICATED when two or more of its own scanned files execute
`(deftype NAME ...)` and are both listed in game.gd. For a duplicated type,
the LAST such deftype in link order is the one whose execution wins at boot
(every earlier deftype execution for the same name is provably overwritten
by it, by the same live-rewrite mechanism this check exists to catch). Any
object holding a `defmethod` for that type which links BEFORE that last
duplicate deftype is a violation: whatever it filled gets wiped out again
once the later object links. An object holding both the last duplicate
deftype and a defmethod for the same type is not a violation against
itself (same link event, no intervening rewrite).

Objects never listed in game.gd are out of scope (they cannot be ordered
against anything here) and are skipped, not reported.

Also prints, as a separate informational line, how many distinct type names
in the scanned tree carry two or more column-0 deftype occurrences at all,
duplicated or not, single-linked or not: a raw count of the tree-wide
duplicate-deftype class issue 534 named at 38, for reconciling against that
figure (see the checker's own --root invocation notes in its landing PR;
this sweep and issue 534's own sweep both use a column-0 regex, so a
difference between the two counts is worth explaining, not assuming away).

Exit 1 with one FAIL line per violation; exit 0 when clean.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

DEFTYPE_RE = re.compile(r"^\(deftype\s+([^\s()]+)", re.M)
DEFMETHOD_RE = re.compile(
    r"^\(defmethod\s+([^\s()]+)\s+(?:\(\(this\s+([^\s()]+)\)|([^\s()]+))", re.M
)
DGO_OBJ_RE = re.compile(r'"([^"]+)\.o"')

# Types held out of the single-home sweep (issue 540). Each entry is a
# specific, checked reason a merge is unsafe or out of scope right now, not
# a blanket permission to skip: this allowlist only suppresses the
# informational "duplicated name" tally below, never violation detection
# (a held name that ever gains a defmethod host creating a real link-order
# violation still fails this check, same as any other duplicated type).
# Remove an entry only when its cited reason no longer holds.
ALLOWLIST = {
    "net-audio-data-characteristics": (
        "scert-2-h.gc duplicates stream-media-h.gc's own copy for a verified "
        "compile-order need: scert-9-h.gc (all_objs.json index 272) needs "
        "net-stream-media-params :inline (via medius-connect-in-params) "
        "before stream-media-h.gc compiles (index 503), and scert-2-h.gc "
        "compiles earlier still (index 265). Neither copy hosts a defmethod "
        "for this type in goal_src/jakx (zero violations), so this is a "
        "build-order duplicate, not a vtable-wipe risk (issue 534). Retiring "
        "stream-media-h.gc's own copy would leave that file with no content "
        "of its own; the file's own note explicitly frames this as a "
        "duplication, not a relocation, and holding here avoids restructuring "
        "an unrelated file for this sweep."
    ),
    "net-stream-media-params": (
        "Same cluster and same reasoning as net-audio-data-characteristics "
        "above: scert-2-h.gc (all_objs.json index 265) duplicates "
        "stream-media-h.gc's copy (index 503) because scert-9-h.gc (index "
        "272) needs this type :inline before stream-media-h.gc compiles. "
        "Zero defmethod hosts for this type in goal_src/jakx."
    ),
    "net-video-data-characteristics": (
        "Same cluster and same reasoning as net-audio-data-characteristics "
        "above: scert-2-h.gc (all_objs.json index 265) duplicates "
        "stream-media-h.gc's copy (index 503) because scert-9-h.gc (index "
        "272) needs net-stream-media-params :inline (which embeds this type "
        "inline in turn) before stream-media-h.gc compiles. Zero defmethod "
        "hosts for this type in goal_src/jakx."
    ),
}


def strip_comments(text):
    text = re.sub(r"#\|.*?\|#", "", text, flags=re.S)
    return "\n".join(line.split(";", 1)[0] for line in text.split("\n"))


def load_link_order(game_gd_path):
    """Return {object: position}, position being this object's index into
    game.gd's own quoted ".o" listing, in file order."""
    rank = {}
    text = open(game_gd_path, encoding="utf-8", errors="replace").read()
    for pos, m in enumerate(DGO_OBJ_RE.finditer(text)):
        rank.setdefault(m.group(1), pos)
    return rank


def scan_goal_src(goal_src):
    """Return (deftypes_of, defmethods_of): each {type_name: [obj, ...]} in
    the file-walk order encountered (order is re-sorted by link rank by the
    caller; walk order itself carries no meaning)."""
    deftypes_of = defaultdict(list)
    defmethods_of = defaultdict(list)
    for dp, _dirs, files in os.walk(goal_src):
        for fn in sorted(files):
            if not fn.endswith(".gc") or fn.endswith("_REF.gc"):
                continue
            path = os.path.join(dp, fn)
            obj = fn[:-3]
            text = strip_comments(open(path, encoding="utf-8", errors="replace").read())
            for m in DEFTYPE_RE.finditer(text):
                deftypes_of[m.group(1)].append(obj)
            for m in DEFMETHOD_RE.finditer(text):
                ty = m.group(2) or m.group(3)
                if ty:
                    defmethods_of[ty].append(obj)
    return deftypes_of, defmethods_of


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument(
        "--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    )
    ap.add_argument("--game", default="jakx")
    ap.add_argument("--game-dgo", default="game.gd")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    goal_src = os.path.join(root, "goal_src", args.game)
    game_gd = os.path.join(goal_src, "dgos", args.game_dgo)

    rank = load_link_order(game_gd)
    deftypes_of, defmethods_of = scan_goal_src(goal_src)

    dup_names = sorted(name for name, objs in deftypes_of.items() if len(objs) >= 2)

    violations = []
    for name in dup_names:
        linked_dups = sorted(
            {(obj, rank[obj]) for obj in deftypes_of[name] if obj in rank},
            key=lambda t: t[1],
        )
        if len(linked_dups) < 2:
            continue  # duplicated in source, but fewer than two copies actually link
        last_obj, last_rank = linked_dups[-1]
        for obj in defmethods_of.get(name, ()):
            if obj not in rank:
                continue
            obj_rank = rank[obj]
            if obj_rank < last_rank:
                violations.append((last_rank, obj_rank, obj, name, last_obj))

    violations.sort()
    for _last_rank, obj_rank, obj, name, last_obj in violations:
        print(
            f"FAIL: {obj}.o (link position {obj_rank}) hosts a (defmethod ... {name} ...) "
            f"that links before {name}'s last duplicate (deftype {name} ...) in "
            f"{last_obj}.o (link position {_last_rank}); that later deftype execution "
            f"zeros this method's custom slot on link (issue 534)"
        )

    held = sorted(name for name in dup_names if name in ALLOWLIST)
    reported_dup_names = [name for name in dup_names if name not in ALLOWLIST]

    held_suffix = (
        f"; {len(held)} held via ALLOWLIST ({', '.join(held)})" if held else ""
    )
    print(
        f"check_type_singlehome ({args.game}): {len(reported_dup_names)} type name(s) "
        f"with a duplicated column-0 deftype in goal_src/{args.game} (linked or not); "
        f"{len(violations)} link-order violation(s){held_suffix}"
    )
    if violations:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
