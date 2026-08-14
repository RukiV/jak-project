#!/usr/bin/env python3
"""Generate jakx's own tpages.gc and textures.gc from the decompiler's texture dump.

Forge investigation, 2026-08-13: jakx's decompiler-macros.gc borrowed jak3's
goal_src/jak3/engine/data/{tpages,textures}.gc wholesale, plus eight hand-added
entries for names jak3 lacked entirely. That import is wrong two ways at once:

  - jak3's tables are missing 9,868 of jakx's 9,944 (name, tpage, idx) texture
    triples outright, because jakx has its own art with its own tpage layout.
  - for the names the two games DO share (the same texture landed in both
    games' builds), the tpage id is frequently different. Ten cases in the
    templea/b/c/d/x -vis-tfrag/-shrub/-water family carry a jak3 id that does
    not match jakx's own build at all, silently pointing (:texture (name tpage))
    references at the wrong tpage.

The fix is to stop borrowing and generate jakx's own tables from jakx's own
data. decompiler/config/jakx/ntsc_v1/tex-info.min.json is exactly the payload
the decompiler itself would have written to dump/tpages.gc and dump/textures.gc
had jakx's own extract run with dump_tex_info on (see
decompiler/ObjectFile/ObjectFileDB.cpp: process_tpages, print_tpage_for_dump,
print_tex_for_dump); this script performs that same reduction offline, without
booting the decompiler, from the already-committed dump.

tex-info.min.json is a flat list of [full_id, {idx, name, tpage_name}] entries,
one per texture. full_id packs the owning tpage's id into the high bits and the
texture's slot index into the low 16 bits, so:

    tpage_id = (full_id - idx) / 65536

tpages.gc gets one `(defconstant <tpage_name> <tpage_id>)` per unique tpage,
ascending by id, matching goal_src/jak3/engine/data/tpages.gc's own convention
(that table is also strictly ascending, though the decompiler builds it from an
unordered_map so nothing except convention pins the order). textures.gc gets
one `(def-tex <name> <tpage_name> <idx>)` per entry, ordered by full_id
ascending, which reproduces the decompiler's own std::map<u32, TextureData>
iteration order exactly (TextureDB::textures in decompiler/data/TextureDB.h)
and lands each tpage's textures as a contiguous ascending-idx block, same as
jak3's file.

Regenerate with:
    python3 scripts/jakx/gen_texture_tables.py
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TEX_INFO_DEFAULT = REPO_ROOT / "decompiler" / "config" / "jakx" / "ntsc_v1" / "tex-info.min.json"
TPAGES_OUT_DEFAULT = REPO_ROOT / "goal_src" / "jakx" / "engine" / "data" / "tpages.gc"
TEXTURES_OUT_DEFAULT = REPO_ROOT / "goal_src" / "jakx" / "engine" / "data" / "textures.gc"

# full_id = tpage_id * TPAGE_SLOT_SPAN + idx, per print_tex_for_dump's own
# `tex.first & 0x0000ffff` extraction of idx from the full id.
TPAGE_SLOT_SPAN = 65536


def load_tex_info(path: Path) -> list[tuple[int, dict]]:
    with path.open("r", encoding="utf-8") as f:
        entries = json.load(f)
    if not isinstance(entries, list):
        raise ValueError(f"{path}: expected a top-level JSON array, got {type(entries).__name__}")
    return entries


def build_tables(
    entries: list[tuple[int, dict]],
) -> tuple[dict[int, str], list[tuple[int, str, str, int]]]:
    """Derive the tpage id/name table and the sorted texture entry list.

    Raises ValueError if the source data is inconsistent with the formula (a
    non-multiple-of-TPAGE_SLOT_SPAN remainder) or ambiguous (one tpage name
    claimed by two ids, or one id claimed by two names) rather than silently
    picking one and hiding the conflict.
    """
    tpage_id_to_name: dict[int, str] = {}
    tpage_name_to_id: dict[str, int] = {}
    tex_entries: list[tuple[int, str, str, int]] = []

    for full_id, info in entries:
        idx = info["idx"]
        name = info["name"]
        tpage_name = info["tpage_name"]

        remainder = (full_id - idx) % TPAGE_SLOT_SPAN
        if remainder != 0:
            raise ValueError(
                f"{name!r}: full_id {full_id} - idx {idx} is not a multiple of "
                f"{TPAGE_SLOT_SPAN} (remainder {remainder})"
            )
        tpage_id = (full_id - idx) // TPAGE_SLOT_SPAN

        existing_name = tpage_id_to_name.get(tpage_id)
        if existing_name is not None and existing_name != tpage_name:
            raise ValueError(
                f"tpage id {tpage_id} claimed by both {existing_name!r} and {tpage_name!r}"
            )
        tpage_id_to_name[tpage_id] = tpage_name

        existing_id = tpage_name_to_id.get(tpage_name)
        if existing_id is not None and existing_id != tpage_id:
            raise ValueError(
                f"tpage name {tpage_name!r} claimed by both id {existing_id} and id {tpage_id}"
            )
        tpage_name_to_id[tpage_name] = tpage_id

        tex_entries.append((full_id, name, tpage_name, idx))

    # ascending by full_id: tpage-major, idx-minor, same order the decompiler's
    # own std::map<u32, TextureData> would iterate.
    tex_entries.sort(key=lambda e: e[0])

    return tpage_id_to_name, tex_entries


def render_tpages(tpage_id_to_name: dict[int, str]) -> str:
    lines = [
        f"(defconstant {tpage_id_to_name[tpage_id]} {tpage_id})\n"
        for tpage_id in sorted(tpage_id_to_name)
    ]
    return "".join(lines)


def render_textures(tex_entries: list[tuple[int, str, str, int]]) -> str:
    lines = [
        f"(def-tex {name} {tpage_name} {idx})\n" for _full_id, name, tpage_name, idx in tex_entries
    ]
    return "".join(lines)


def write_lf(path: Path, text: str) -> None:
    """Write with bare LF regardless of platform; the repo normalizes to LF in
    the blob anyway (.gitattributes: `* text=auto`), and jak3's committed
    tables are also LF at the blob, so this keeps a clean diff on checkout."""
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tex-info", type=Path, default=TEX_INFO_DEFAULT, help="input tex-info.min.json")
    parser.add_argument("--tpages-out", type=Path, default=TPAGES_OUT_DEFAULT, help="output tpages.gc")
    parser.add_argument("--textures-out", type=Path, default=TEXTURES_OUT_DEFAULT, help="output textures.gc")
    args = parser.parse_args(argv)

    entries = load_tex_info(args.tex_info)
    tpage_id_to_name, tex_entries = build_tables(entries)

    write_lf(args.tpages_out, render_tpages(tpage_id_to_name))
    write_lf(args.textures_out, render_textures(tex_entries))

    print(f"read {len(entries)} texture entries from {args.tex_info}")
    print(f"wrote {len(tpage_id_to_name)} tpages to {args.tpages_out}")
    print(f"wrote {len(tex_entries)} textures to {args.textures_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
