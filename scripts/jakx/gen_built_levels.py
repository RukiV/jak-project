import argparse
import hashlib
import re
import sys
from pathlib import Path

# Generate the buildable-level allowlist the jakx cross-family want-driver gates on
# (forge issue 550, rung R1).
#
# jakx-want-driver-tick's candidate filter used to pin the boot's home family for the
# whole boot ((= (-> cp level) home), level.gc). That pin cannot simply be dropped: only
# a fraction of the game's ~370 DGOs are extraction-armed in
# decompiler/config/jakx/ntsc_v1/inputs.jsonc's "levels_to_extract" array, and families
# sit close together in world space (garage-fma-1 is under a kilometre from
# kcross-start, and garage's own want set has 8 of 9 levels unbuilt). An unfiltered
# driver near a family boundary would want a level with no built DGO behind it.
#
# This script derives *jakx-built-levels*: every level-load-info :name a currently
# built DGO can serve, so the driver (and jakx-cam-fly-tick) can gate a candidate
# continue point's whole want set against real buildability instead of pinning one
# family. It writes the array straight into goal_src/jakx/engine/level/level.gc between
# a marked generated block, the same BEGIN/END GENERATED convention
# extract_race_centerline.py established for the racer centerline table, and refuses to
# run if that block's markers are not already present (seed them once by hand; every
# run after that is a clean in-place replace).
#
# THE MAPPING RULE (verified against level-info.gc, not assumed)
# ----------------------------------------------------------------
# levels_to_extract names DGO files ("ICA.DGO", "JUNGLEW.DGO", ...), but continue
# points and want arrays reference level-load-info's own :name field ('icea, 'junglew,
# ...), which is frequently NOT the same string as the DGO's basename. The bridge is
# level-load-info's :nickname field, which IS always the owning DGO's basename
# lowercased with the extension dropped. Verified directly against level-info.gc:
#
#   ICA.DGO  -> nickname 'ica  -> icea  :name 'icea   (level-load-info line ~9450)
#   ICB.DGO  -> nickname 'icb  -> iceb  :name 'iceb   (line ~9474)
#   ICD.DGO  -> nickname 'icd  -> iced  :name 'iced   (line ~9522)
#   ICEW.DGO -> nickname 'icew -> icew  :name 'icew   (line ~9546)
#   ICES.DGO -> nickname 'ices -> ices  :name 'ices   (line ~9721)
#   JGA.DGO  -> nickname 'jga  -> junglea :name 'junglea (line ~287)
#   JUNGLEW.DGO -> nickname 'junglew -> junglew :name 'junglew (line ~529)
#
# One wrinkle, also verified rather than assumed: a nickname is not always unique to a
# single :name. KRASW.DGO, KRA.DGO, KRB.DGO and KRC.DGO each back TWO level-load-info
# entries that share one nickname: krasw/kraswfma, krasa/krasafma, krasb/krasbfma,
# krasc/krascfma (the "fma" entries are the same physical DGO under a second logical
# level, the free-roam counterpart to the timed-race entry, mirroring how garage's own
# continues are all named "garage-fma-*"). Building the DGO makes BOTH :name values
# available, so the mapping below is nickname -> the SET of :name values sharing it,
# unioned into the allowlist rather than collapsed to one.
#
# ALWAYS-BUILT ENGINE DGOS
# ------------------------
# CGO/KERNEL.CGO and CGO/GAME.CGO are the two engine containers inputs.jsonc's
# "dgo_names" leaves uncommented unconditionally; they build regardless of
# levels_to_extract. They are folded into the same nickname lookup as every
# levels_to_extract entry, on principle, but verified to contribute nothing today:
# neither "kernel" nor "game" matches any level-load-info :nickname in level-info.gc
# (checked directly, zero matches for both). The four small always-resident
# pseudo-levels (default-level, intro, title, halfpipe) that ship with no DGO of their
# own were investigated as a candidate addition here and rejected: the same "no DGO
# anywhere in dgo_names" signal that finds them also finds roughly seventy borrow-load
# TSK sub-levels and leftover dev/test levels that issue 550 itself documents as still
# unbuilt (TSK80B and TSK32C by name, rung R4), so "absent from dgo_names" is not a
# reliable "always built" signal and this script does not use it. See the R1 report for
# the full trail; this is a documented could-not-verify, not a silent gap.

SCRIPT_NAME = "gen_built_levels.py"
TABLE_SYMBOL = "*jakx-built-levels*"

REPO_ROOT = Path(__file__).resolve().parents[2]
INPUTS_PATH = REPO_ROOT / "decompiler" / "config" / "jakx" / "ntsc_v1" / "inputs.jsonc"
LEVEL_INFO_PATH = REPO_ROOT / "goal_src" / "jakx" / "engine" / "level" / "level-info.gc"
LEVEL_GC_PATH = REPO_ROOT / "goal_src" / "jakx" / "engine" / "level" / "level.gc"

# Always-built regardless of levels_to_extract (see header comment above). Folded into
# the same nickname lookup as every levels_to_extract entry; verified to contribute no
# extra names today.
ENGINE_DGOS = ["CGO/KERNEL.CGO", "CGO/GAME.CGO"]

# Sanity floor on how many level-load-info blocks level-info.gc should parse into,
# guarding against a silent parse regression (the real count is 436 at the time this
# script was written; a future landing only ever grows it).
MIN_LEVEL_LOAD_INFO_ENTRIES = 200

BEGIN_PREFIX = ";; BEGIN GENERATED: {}".format(SCRIPT_NAME)
END_PREFIX = ";; END GENERATED: {}".format(SCRIPT_NAME)


def md5_of(path):
    return hashlib.md5(path.read_bytes()).hexdigest()


def parse_string_array(text, key):
    """Pull every quoted string out of the named top-level JSON(C) array. Comments
    inside the array (// ...) are not stripped explicitly; they never sit on a line
    with a quoted string in this file, so the plain quote regex already ignores
    them."""
    m = re.search(r'"' + re.escape(key) + r'"\s*:\s*\[(.*?)\]', text, re.S)
    if not m:
        sys.exit("error: could not find a \"{}\" array in {}".format(key, INPUTS_PATH))
    return re.findall(r'"([^"]+)"', m.group(1))


def dgo_basename_lower(dgo_entry):
    """'DGO/ICA.DGO' or 'CGO/KERNEL.CGO' -> 'ica' / 'kernel': the nickname-matching
    key, per the mapping rule verified above."""
    base = dgo_entry.split("/")[-1]
    base = re.sub(r"\.(DGO|CGO)$", "", base, flags=re.I)
    return base.lower()


def parse_level_load_info(text):
    """Split level-info.gc on its own ';; definition for symbol X, type
    level-load-info' comments (one precedes every block, verified: 436 blocks, 436
    comments, in the same order) and pull :name and :nickname out of each. Returns
    nickname (lowercased) -> sorted list of :name values sharing it."""
    parts = re.split(r";; definition for symbol (\S+), type level-load-info\n", text)
    if len(parts) < 3:
        sys.exit(
            "error: found no level-load-info definitions in {}; the comment-header "
            "parse regex may be stale".format(LEVEL_INFO_PATH)
        )
    nick_to_names = {}
    entry_count = 0
    for i in range(1, len(parts), 2):
        block = parts[i + 1]
        mn = re.search(r":name '([A-Za-z0-9_-]+)", block)
        mnick = re.search(r":nickname '([A-Za-z0-9_-]+)", block)
        if not (mn and mnick):
            continue
        entry_count += 1
        nick_to_names.setdefault(mnick.group(1).lower(), set()).add(mn.group(1))
    if entry_count < MIN_LEVEL_LOAD_INFO_ENTRIES:
        sys.exit(
            "error: only parsed {} level-load-info entries from {}, expected at "
            "least {}; refusing to generate from a possibly-broken parse".format(
                entry_count, LEVEL_INFO_PATH, MIN_LEVEL_LOAD_INFO_ENTRIES
            )
        )
    return nick_to_names, entry_count


def build_allowlist(levels_to_extract, nick_to_names, out):
    built_dgos = levels_to_extract + ENGINE_DGOS
    names = set()
    unmatched_levels = []
    for dgo in built_dgos:
        nick = dgo_basename_lower(dgo)
        matched = nick_to_names.get(nick)
        if matched:
            names.update(matched)
        elif dgo in ENGINE_DGOS:
            # expected: verified neither engine CGO's basename is any level's nickname
            print(
                "note: engine DGO {} (nickname '{}') matches no level-load-info "
                "entry, as expected".format(dgo, nick),
                file=out,
            )
        else:
            unmatched_levels.append(dgo)
    if unmatched_levels:
        sys.exit(
            "error: {} levels_to_extract DGO(s) have no matching level-load-info "
            "nickname: {}. Either the mapping rule above no longer holds or "
            "level-info.gc is missing an entry; refusing to emit a possibly-wrong "
            "allowlist.".format(len(unmatched_levels), unmatched_levels)
        )
    return sorted(names)


def emit_block(names, inputs_md5, level_info_md5):
    header_line = "(define {} (new 'static 'boxed-array :type symbol".format(TABLE_SYMBOL)
    pad = " " * (header_line.index("(new 'static 'boxed-array") + 2)
    provenance = "source {} md5 {}, {} md5 {}".format(
        INPUTS_PATH.relative_to(REPO_ROOT).as_posix(),
        inputs_md5,
        LEVEL_INFO_PATH.relative_to(REPO_ROOT).as_posix(),
        level_info_md5,
    )
    lines = ["{} ({})".format(BEGIN_PREFIX, provenance), header_line]
    for name in names:
        lines.append("{}'{}".format(pad, name))
    lines.append("{})".format(pad))
    lines.append("        )")
    lines.append("{} ({})".format(END_PREFIX, provenance))
    return "\n".join(lines) + "\n"


def replace_block(level_gc_text, new_block):
    begin_re = re.escape(BEGIN_PREFIX)
    end_re = re.escape(END_PREFIX)
    pattern = re.compile(
        r"^" + begin_re + r".*?\n.*?^" + end_re + r".*?\n",
        re.S | re.M,
    )
    matches = list(pattern.finditer(level_gc_text))
    if len(matches) != 1:
        sys.exit(
            "error: expected exactly one {} ... {} block in {}, found {}. Seed the "
            "marker block by hand before running this script (see the header "
            "comment for the expected marker text).".format(
                BEGIN_PREFIX, END_PREFIX, LEVEL_GC_PATH, len(matches)
            )
        )
    m = matches[0]
    return level_gc_text[: m.start()] + new_block + level_gc_text[m.end() :]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if goal_src/jakx/engine/level/level.gc's generated block "
        "is not exactly what a fresh run would produce, without writing anything",
    )
    args = parser.parse_args()

    inputs_text = INPUTS_PATH.read_text(encoding="utf-8")
    level_info_text = LEVEL_INFO_PATH.read_text(encoding="utf-8")
    level_gc_text = LEVEL_GC_PATH.read_text(encoding="utf-8")

    levels_to_extract = parse_string_array(inputs_text, "levels_to_extract")
    nick_to_names, entry_count = parse_level_load_info(level_info_text)
    names = build_allowlist(levels_to_extract, nick_to_names, sys.stderr)

    new_block = emit_block(names, md5_of(INPUTS_PATH), md5_of(LEVEL_INFO_PATH))
    updated_text = replace_block(level_gc_text, new_block)

    print(
        "gen_built_levels: {} DGOs in levels_to_extract, {} level-load-info entries "
        "parsed, {} unique built level names".format(
            len(levels_to_extract), entry_count, len(names)
        ),
        file=sys.stderr,
    )

    if args.check:
        if updated_text == level_gc_text:
            print("gen_built_levels: block up to date", file=sys.stderr)
            sys.exit(0)
        sys.exit(
            "error: {}'s generated block is out of date; run "
            "scripts/jakx/{} to refresh it".format(LEVEL_GC_PATH, SCRIPT_NAME)
        )

    if updated_text == level_gc_text:
        print("gen_built_levels: no change", file=sys.stderr)
        return
    LEVEL_GC_PATH.write_text(updated_text, encoding="utf-8")
    print("gen_built_levels: wrote {}".format(LEVEL_GC_PATH), file=sys.stderr)


if __name__ == "__main__":
    main()
