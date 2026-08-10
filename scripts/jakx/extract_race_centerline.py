import argparse
import hashlib
import struct
import sys
from decimal import Decimal
from pathlib import Path

# Extract the authored race-mesh centerline from a raw jakx level .go object.
#
# Forge issue #161: the bring-up racer (goal_src/jakx/engine/level/level.gc,
# jakx-racer-tick) currently lerps between boot-continue-family points, which
# is not the authored racing line. A real race level bakes a race-mesh basic
# (an unlinked, version-5 structure) holding a table of race-mesh-edge
# records: paired left/right rail vectors whose average is the track
# centerline, ordered along the track by a monotonic lap fraction packed into
# left.w. This script locates that structure by signature scan (never a
# hardcoded file offset -- the link-header skew between a stored pointer and
# its real file offset varies per file), decodes the edges, and prints a GOAL
# static-data fragment with one vector per centerline midpoint, ready to
# paste into level.gc next to the racer code.
#
# usage: python scripts/jakx/extract_race_centerline.py <path to raw .go>
# example (jungle bake):
#   python scripts/jakx/extract_race_centerline.py decompiler_out/jakx/raw_obj/jungles.go

SCRIPT_NAME = "extract_race_centerline.py"
TABLE_SYMBOL = "*jakx-racer-track*"

# goal_src/jakx/engine/util/types-h.gc: METER_LENGTH = 4096.0, the (meters ...)
# macro's scale factor. Stored positions (and the bbox check below) are in
# this raw engine unit; dividing by METER_LENGTH gives real in-game meters.
METER_LENGTH = 4096.0

HEADER_TAG = b"\xff\xff\xff\xff"
HEADER_SIZE = 32  # tag(4) + version(1) + flags(1) + 4x uint16 + pad(2) + 4x uint32 ptr
EDGE_SIZE = 32  # two vec4 float32 records: left, right

# Known-good decode targets, keyed by input file basename. Add an entry here
# (md5, expected header fields, expected edge file offset, sanity bbox in
# real meters) before pointing this script at a new level's raw .go; a file
# with no entry is refused rather than silently extracted unverified.
KNOWN_FILES = {
    "jungles.go": {
        "md5": "e9ef477f93626d33e1e0ad080648f4c8",
        "header": {
            "version": 5,
            "flags": 1,
            "slice_count": 102,
            "edge_count": 101,
            "race_line_count": 2,
            "ai_valid_mask": 0x3,
        },
        "edges_file_offset": 391808,
        "bbox_m": {"x": (537.0, 3702.0), "z": (-4458.0, -1894.0)},
    },
}

BBOX_SLACK_M = 25.0


def find_race_mesh_header(data):
    """Signature-scan for the race-mesh basic's unlinked header: a 0xffffffff
    type tag followed by plausible int16 counts and four in-range, increasing
    pointers. Returns every match; callers assert exactly one rather than
    trusting a hardcoded offset, since the file's actual layout is the only
    source of truth."""
    n = len(data)
    matches = []
    for offset in range(0, n - HEADER_SIZE, 4):
        if data[offset : offset + 4] != HEADER_TAG:
            continue
        version, flags = struct.unpack_from("<BB", data, offset + 4)
        slice_count, edge_count, race_line_count, ai_valid_mask, pad = (
            struct.unpack_from("<HHHHH", data, offset + 6)
        )
        ptrs = struct.unpack_from("<IIII", data, offset + 16)
        if pad != 0:
            continue
        if not (1 <= slice_count <= 10000):
            continue
        if not (1 <= edge_count <= 10000):
            continue
        if not (1 <= race_line_count <= 16):
            continue
        if not (0 <= ai_valid_mask <= 0xFF):
            continue
        if not all(0 < p < n for p in ptrs):
            continue
        if not (ptrs[0] < ptrs[1] < ptrs[2] < ptrs[3]):
            continue
        matches.append(
            {
                "offset": offset,
                "version": version,
                "flags": flags,
                "slice_count": slice_count,
                "edge_count": edge_count,
                "race_line_count": race_line_count,
                "ai_valid_mask": ai_valid_mask,
                "slices_ptr": ptrs[0],
                "edges_ptr": ptrs[1],
                "race_lines_ptr": ptrs[2],
                "fourth_ptr": ptrs[3],
            }
        )
    return matches


def read_edges(data, edges_file_offset, edge_count):
    """Read edge_count race-mesh-edge records starting at edges_file_offset.
    Each record is two 16-byte float32 vectors, left then right; the lap
    fraction is packed in left.w. Returns a list of (left, right) 4-tuples."""
    edges = []
    for i in range(edge_count):
        off = edges_file_offset + i * EDGE_SIZE
        left = struct.unpack_from("<ffff", data, off)
        right = struct.unpack_from("<ffff", data, off + 16)
        edges.append((left, right))
    return edges


def format_float(x):
    """Render a float32-derived value as a GOAL literal: full precision so it
    round-trips, and always plain decimal notation. Two traps here, both hit
    during the #161 investigation: repr() alone can print a denormal float32
    in scientific notation (e.g. "1e-40"), which GOAL's literal parser does
    not accept -- no scientific-notation float literal exists anywhere in
    goal_src; and a low-precision format string like '%.1f' silently
    flattens the same denormal to "0.0". Route anything repr() would put in
    scientific notation through Decimal for exact fixed-point digits
    instead."""
    if x == 0.0:
        return "0.0"
    s = repr(float(x))
    if "e" in s or "E" in s:
        s = format(Decimal(x), "f")
    return s


def emit_goal_fragment(midpoints, source_name, source_md5, out):
    header_line = "(define {} (new 'static 'boxed-array :type vector".format(TABLE_SYMBOL)
    indent = header_line.index("(new 'static 'boxed-array") + 2
    pad = " " * indent

    print(
        ";; BEGIN GENERATED: {} (source {}, md5 {})".format(SCRIPT_NAME, source_name, source_md5),
        file=out,
    )
    print(header_line, file=out)
    for x, y, z in midpoints:
        print(
            "{}(new 'static 'vector :x {} :y {} :z {} :w 1.0)".format(
                pad, format_float(x), format_float(y), format_float(z)
            ),
            file=out,
        )
    print("{})".format(pad), file=out)
    print("        )", file=out)
    print(
        ";; END GENERATED: {} (source {}, md5 {})".format(SCRIPT_NAME, source_name, source_md5),
        file=out,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("go_path", type=Path, help="path to the raw .go level object")
    args = parser.parse_args()

    go_path = args.go_path
    name = go_path.name
    known = KNOWN_FILES.get(name)
    if known is None:
        sys.exit(
            "error: no known-good decode target for '{}'. Add an entry to "
            "KNOWN_FILES in {} (md5, expected header fields, sanity bbox) "
            "before extracting a new level's centerline.".format(name, SCRIPT_NAME)
        )

    data = go_path.read_bytes()
    actual_md5 = hashlib.md5(data).hexdigest()
    if actual_md5 != known["md5"]:
        sys.exit(
            "error: {} md5 mismatch: expected {}, got {}. Stopping rather than "
            "extracting from an unverified input.".format(go_path, known["md5"], actual_md5)
        )

    matches = find_race_mesh_header(data)
    if len(matches) != 1:
        sys.exit(
            "error: expected exactly one race-mesh header signature match, found {}: {}".format(
                len(matches), matches
            )
        )
    header = matches[0]
    expected_header = known["header"]
    mismatches = [
        "{}: expected {}, got {}".format(key, expected_header[key], header[key])
        for key in expected_header
        if header[key] != expected_header[key]
    ]
    if mismatches:
        sys.exit("error: header field mismatch:\n  " + "\n  ".join(mismatches))

    header_offset = header["offset"]
    skew = (header_offset + 32) - header["slices_ptr"]
    edges_file_offset = header["edges_ptr"] + skew
    if edges_file_offset != known["edges_file_offset"]:
        sys.exit(
            "error: derived edges file offset {} does not match the known-good "
            "value {} (skew derived as {})".format(
                edges_file_offset, known["edges_file_offset"], skew
            )
        )

    edges = read_edges(data, edges_file_offset, header["edge_count"])

    lap_fractions = [left[3] for left, _right in edges]
    monotonic = all(
        lap_fractions[i] < lap_fractions[i + 1] for i in range(len(lap_fractions) - 1)
    )
    if not monotonic:
        sys.exit("error: lap fractions are not strictly monotonic across edges")

    midpoints = [
        (
            (left[0] + right[0]) / 2.0,
            (left[1] + right[1]) / 2.0,
            (left[2] + right[2]) / 2.0,
        )
        for left, right in edges
    ]

    xs_m = [m[0] / METER_LENGTH for m in midpoints]
    zs_m = [m[2] / METER_LENGTH for m in midpoints]
    bbox = known["bbox_m"]
    x_lo, x_hi = bbox["x"]
    z_lo, z_hi = bbox["z"]
    if not (x_lo - BBOX_SLACK_M <= min(xs_m) and max(xs_m) <= x_hi + BBOX_SLACK_M):
        sys.exit(
            "error: midpoint X bbox (meters) {:.2f}..{:.2f} outside expected "
            "{}..{} (+/- {})".format(min(xs_m), max(xs_m), x_lo, x_hi, BBOX_SLACK_M)
        )
    if not (z_lo - BBOX_SLACK_M <= min(zs_m) and max(zs_m) <= z_hi + BBOX_SLACK_M):
        sys.exit(
            "error: midpoint Z bbox (meters) {:.2f}..{:.2f} outside expected "
            "{}..{} (+/- {})".format(min(zs_m), max(zs_m), z_lo, z_hi, BBOX_SLACK_M)
        )

    print(
        "extractor sanity: {} points, lap fractions strictly monotonic, "
        "bbox (meters) X {:.2f}..{:.2f} Z {:.2f}..{:.2f}".format(
            len(midpoints), min(xs_m), max(xs_m), min(zs_m), max(zs_m)
        ),
        file=sys.stderr,
    )

    emit_goal_fragment(midpoints, name, actual_md5, sys.stdout)


if __name__ == "__main__":
    main()
