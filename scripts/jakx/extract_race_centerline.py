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
# A race mesh whose track has forks holds more than one race line, and the
# header's own race-line table says which mesh slices each line drives (#161
# follow-up, see RACE-LINE MODE below). Without that table the edge array is
# just "every edge of every branch", and the fork-interleave filter above is a
# geometric guess at splitting it; with it, the split is read out of the data.
#
# usage: python scripts/jakx/extract_race_centerline.py <path to raw .go>
#                                                       [--race-line N] [--symbol NAME]
# example (jungle bake, the shipped table):
#   python scripts/jakx/extract_race_centerline.py decompiler_out/jakx/raw_obj/jungles.go
# example (ice bake, main circuit line):
#   python scripts/jakx/extract_race_centerline.py decompiler_out/jakx/raw_obj/ices.go --race-line 3
#
# RACE-LINE MODE (--race-line N)
# ------------------------------
# The race-mesh basic's fourth stored pointer (field `race-lines`, offset 28)
# is an array of `race-line-count` race-line structures, 64 bytes each, laid
# out exactly as the captured deftype in decompiler/config/jakx/all-types.gc
# (~45442) describes. Each race-line carries its own `slices` pointer: an
# int16 per race-mesh slice, -1 where that slice is NOT on this line and a
# race-line point index where it is. Walking the mesh slices in order and
# keeping the `start-edge` of every slice the chosen line marks valid yields
# that line's edge sequence directly -- no distance gates, no spike removal.
#
# The two modes are separate on purpose: passing no --race-line runs the
# original fork-filter path unchanged, so the shipped jungle table keeps
# regenerating byte for byte.

SCRIPT_NAME = "extract_race_centerline.py"
TABLE_SYMBOL = "*jakx-racer-track*"

# goal_src/jakx/engine/util/types-h.gc: METER_LENGTH = 4096.0, the (meters ...)
# macro's scale factor. Stored positions (and the bbox check below) are in
# this raw engine unit; dividing by METER_LENGTH gives real in-game meters.
METER_LENGTH = 4096.0

HEADER_TAG = b"\xff\xff\xff\xff"
HEADER_SIZE = 32  # tag(4) + version(1) + flags(1) + 4x uint16 + pad(2) + 4x uint32 ptr
EDGE_SIZE = 32  # two vec4 float32 records: left, right

# race-mesh-slice: two int16, (start-edge, end-edge). all-types.gc ~45590.
MESH_SLICE_SIZE = 4
# race-line: the deftype at all-types.gc ~45442, size-assert #x40.
RACE_LINE_SIZE = 64
# a race-line point: four uint16, quantised as offset + (q / RACE_LINE_QUANT) *
# scale per component. The divisor is 32768 and not 65535/65536: 32768 is the
# largest value that appears in any component of any line in any of the 24
# shipped race meshes, and it is the only divisor under which each line's
# decoded closed-loop length reproduces the `length` float stored beside it in
# its own race-line header (verified to 1e-5 relative on all nine lines of
# jungles.go, ices.go and havjungs.go -- see check_race_line_length below).
RACE_LINE_POINT_SIZE = 8
RACE_LINE_QUANT = 32768.0
# tolerance for that self-check, as a fraction of the stored length
RACE_LINE_LENGTH_TOL = 1e-4

# Known-good decode targets, keyed by input file basename. Add an entry here
# (md5, expected header fields, expected edge file offset, sanity bbox in
# real meters) before pointing this script at a new level's raw .go; a file
# with no entry is refused rather than silently extracted unverified.
#
# "race_lines" is the per-line gate for --race-line mode, keyed by line index:
# point_count is how many mesh slices that line marks valid (and so how many
# centerline points come out), lap_length_m is the closed-loop length of those
# midpoints, and max_step_m is that loop's longest single step including the
# lap seam. A line with no entry is refused, same rule as a file with no entry.
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
        # expected remaining consecutive-direction reversal count after both
        # fork-interleave filter passes below; a future track's own
        # legitimate hairpin could be nonzero, so this lives per-file rather
        # than as a blanket constant
        "expected_reversals": 0,
        # line 0 is the branch the fork-interleave filter happens to keep, so
        # `--race-line 0` here reproduces the default path's output exactly;
        # that equality is the regression gate for this whole mode.
        "race_lines": {
            0: {"point_count": 97, "lap_length_m": 13267.9, "max_step_m": 272.2},
            1: {"point_count": 96, "lap_length_m": 13309.3, "max_step_m": 272.2},
        },
    },
    "ices.go": {
        "md5": "1208a11244af2b7a6e939292278ee661",
        "header": {
            "version": 5,
            "flags": 1,
            "slice_count": 158,
            "edge_count": 155,
            "race_line_count": 5,
            "ai_valid_mask": 0x1F,
        },
        "edges_file_offset": 78560,
        "bbox_m": {"x": (-1757.0, 1726.0), "z": (-1402.0, 1624.0)},
        "expected_reversals": 0,
        # icew's mesh forks three times (slices 7-38, 87-106, 107-126). Lines
        # 3 and 4 are byte-identical and take the wider branch at all three;
        # 0, 1 and 2 each cut one of them. See the race-line-header write-up
        # for the ranking evidence.
        "race_lines": {
            0: {"point_count": 124, "lap_length_m": 16793.9, "max_step_m": 477.7},
            1: {"point_count": 118, "lap_length_m": 16862.2, "max_step_m": 477.7},
            2: {"point_count": 126, "lap_length_m": 17126.2, "max_step_m": 477.7},
            3: {"point_count": 124, "lap_length_m": 17183.6, "max_step_m": 477.7},
            4: {"point_count": 124, "lap_length_m": 17183.6, "max_step_m": 477.7},
        },
    },
    "havjungs.go": {
        "md5": "065fa5fee491955b38ef0f01b488253d",
        "header": {
            "version": 5,
            "flags": 1,
            "slice_count": 188,
            "edge_count": 187,
            "race_line_count": 2,
            "ai_valid_mask": 0x3,
        },
        "edges_file_offset": 112848,
        "bbox_m": {"x": (-910.0, 2855.0), "z": (-3321.0, 554.0)},
        "expected_reversals": 0,
        # havjungw's mesh forks once (slices 33-41). Line 1 holds the flat,
        # arc-length-consistent branch; line 0 takes the short climbing one.
        "race_lines": {
            0: {"point_count": 183, "lap_length_m": 16761.0, "max_step_m": 265.8},
            1: {"point_count": 184, "lap_length_m": 16842.4, "max_step_m": 265.8},
        },
    },
}

# how far a measured lap length / max step may sit above or below its
# KNOWN_FILES figure before the run is refused (the figures are recorded to
# 0.1m, so this is a rounding allowance, not a real tolerance)
LENGTH_SLACK_M = 0.5

BBOX_SLACK_M = 25.0

# Fork-interleave post-filter (#161 follow-up). The jungle race-mesh contains
# a track fork whose two branches' edges interleave in lap-fraction order.
# Two passes: pass 1 (filter_fork_branch) is a distance gate that removes most
# of the interleaved far-branch points without a cascade; pass 2
# (remove_spikes) mops up the survivors a pure distance gate cannot reach,
# since the first cross-branch hop is only ~250m -- inside any gate loose
# enough to spare the track's own legitimate 258-272m long steps elsewhere in
# the loop. A single 250m gate was tried first and rejected: it also caught
# those legitimate long steps, and because the gate compares only to the last
# KEPT point (needed so it can straddle the fork at all), dropping one of them
# left the anchor stale and cascaded into dropping most of the rest of the
# loop.
DIST_GATE_M = 285.0
DIST_GATE = DIST_GATE_M * METER_LENGTH  # 1,167,360 raw quads

# Shared by pass 2 (the out-and-back collapse ceiling) and the final sanity
# check (max step, lap seam included) below -- one number serving both jobs
# on purpose.
MAX_STEP_M = 350.0
MAX_STEP = MAX_STEP_M * METER_LENGTH

MAX_REMOVED = 15  # sanity ceiling on total points removed across both passes
MIN_KEPT = 85  # sanity floor on how many points should survive both passes


def dist3(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5


def filter_fork_branch(midpoints, out):
    """Pass 1: greedily walk the midpoints in array order and drop any point
    whose 3D distance from the last KEPT point is at or over DIST_GATE. Point
    0 is always kept as the walk's anchor. Around the jungle track's fork,
    consecutive raw points alternate between the fork's two branches with
    far, direction-reversing steps, while each branch's own next point stays
    close; this greedy gate keeps one coherent branch and drops most of the
    other branch's interleaved points, without needing to know which branch
    is "correct". Returns (kept, dropped): kept is a list of (original_index,
    point), dropped is a list of (original_index, distance_m_from_last_kept);
    every drop is also reported to out."""
    kept = [(0, midpoints[0])]
    dropped = []
    for i in range(1, len(midpoints)):
        d = dist3(kept[-1][1], midpoints[i])
        if d < DIST_GATE:
            kept.append((i, midpoints[i]))
        else:
            dropped.append((i, d / METER_LENGTH))
            print(
                "pass 1: dropped point index {}: {:.2f}m from last kept point".format(
                    i, d / METER_LENGTH
                ),
                file=out,
            )
    return kept, dropped


def remove_spikes(kept, out):
    """Pass 2: out-and-back spike removal on the pass-1 survivors, treated as
    a closed loop. For each point P with loop-neighbors A (previous) and B
    (next), a negative horizontal (x/z) dot product of delta(A to P) and
    delta(P to B) means the path reverses direction at P -- a spike a plain
    distance gate cannot see, since the first cross-branch hop off the fork
    is only ~250m. P is removed only if that reversal is real (negative dot)
    AND removing it leaves the direct A-to-B step at or under MAX_STEP, so a
    legitimate sharp turn that doesn't shortcut a fork is left alone. Runs
    passes until a full pass removes nothing (removing P changes its
    neighbors' neighbors, so more than one point can need removing in
    sequence). Returns (kept, removed): kept is the surviving list of
    (original_index, point), removed is a list of (original_index,
    ap_distance_m, pb_distance_m); every removal is also reported to out."""
    points = list(kept)
    removed = []
    changed = True
    while changed:
        changed = False
        n = len(points)
        if n < 3:
            break
        for i in range(n):
            idx_a, a = points[(i - 1) % n]
            idx_p, p = points[i]
            idx_b, b = points[(i + 1) % n]
            d1x, d1z = p[0] - a[0], p[2] - a[2]
            d2x, d2z = b[0] - p[0], b[2] - p[2]
            dot = d1x * d2x + d1z * d2z
            if dot < 0 and dist3(a, b) <= MAX_STEP:
                ap_m = dist3(a, p) / METER_LENGTH
                pb_m = dist3(p, b) / METER_LENGTH
                print(
                    "pass 2: removed spike at index {}: {:.2f}m in, {:.2f}m out".format(
                        idx_p, ap_m, pb_m
                    ),
                    file=out,
                )
                removed.append((idx_p, ap_m, pb_m))
                del points[i]
                changed = True
                break
    return points, removed


def count_reversals(points):
    """Count consecutive-direction reversals on a closed loop of points: a
    negative horizontal (x/z) dot product of delta(prev to cur) and
    delta(cur to next), wrapping at both ends. Matches the closed-loop
    condition pass 2 eliminates, so a correctly filtered sequence counts
    zero."""
    n = len(points)
    count = 0
    for i in range(n):
        a = points[(i - 1) % n]
        p = points[i]
        b = points[(i + 1) % n]
        d1x, d1z = p[0] - a[0], p[2] - a[2]
        d2x, d2z = b[0] - p[0], b[2] - p[2]
        if d1x * d2x + d1z * d2z < 0:
            count += 1
    return count


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
        # NOTE ON THE POINTER KEY NAMES. The race-mesh deftype's four trailing
        # pointers are, in order, `slices`, `edges`, `hash` and `race-lines`
        # (all-types.gc ~45637). The third and fourth keys below are therefore
        # misnamed: "race_lines_ptr" is the race-mesh-hash and "fourth_ptr" is
        # the actual race-lines array. The names are kept because callers and
        # the KNOWN_FILES offsets already use them, but do not read
        # "race_lines_ptr" looking for race lines -- a previous investigation
        # lost a leg to exactly that, hunting a 5-entry table inside the hash's
        # 1,792 bytes. read_race_lines() is called with fourth_ptr on purpose.
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
                "race_lines_ptr": ptrs[2],  # actually `hash`
                "fourth_ptr": ptrs[3],  # actually `race-lines`
            }
        )
    return matches


def read_edges(data, edges_file_offset, edge_count):
    """Read edge_count race-mesh-edge records starting at edges_file_offset.
    Each record is two 16-byte float32 vectors, left then right; the lap
    fraction is packed in left.w. Returns a list of (left, right) 4-tuples.

    The array on disk actually holds edge_count + 1 records: the extra last
    one repeats edge 0's left and right positions exactly, with lap-dist 1.0
    instead of 0.0 -- the lap seam's closing edge (verified byte-identical in
    jungles.go, ices.go and havjungs.go). Reading only edge_count of them is
    therefore right, not an off-by-one: it takes each distinct edge once, and
    the seam is closed by wrapping the loop instead. Every mesh slice's
    start-edge is <= edge_count - 1, so a start-edge selection never needs the
    duplicate."""
    edges = []
    for i in range(edge_count):
        off = edges_file_offset + i * EDGE_SIZE
        left = struct.unpack_from("<ffff", data, off)
        right = struct.unpack_from("<ffff", data, off + 16)
        edges.append((left, right))
    return edges


def read_mesh_slices(data, slices_file_offset, slice_count):
    """Read slice_count race-mesh-slice records: two int16, (start-edge,
    end-edge), indices into the edge array. A slice is the strip of track
    between those two edges, so consecutive slices normally step the edge
    index by one; around a fork the start-edge order interleaves the
    branches, which is exactly the interleave the geometric filter has to
    guess at and the race-line slice map states outright."""
    return [
        struct.unpack_from("<hh", data, slices_file_offset + i * MESH_SLICE_SIZE)
        for i in range(slice_count)
    ]


def read_race_lines(data, race_lines_file_offset, race_line_count, skew):
    """Read the race-mesh's `race-lines` array: race_line_count race-line
    structures of RACE_LINE_SIZE bytes each, per the deftype in all-types.gc.
    Stored pointers inside each record are rebased to file offsets with the
    same skew the caller derived for the mesh itself."""
    lines = []
    for i in range(race_line_count):
        off = race_lines_file_offset + i * RACE_LINE_SIZE
        scale = struct.unpack_from("<ffff", data, off)
        origin = struct.unpack_from("<ffff", data, off + 16)
        (length,) = struct.unpack_from("<f", data, off + 32)
        flags, points_per_slice, point_count, gap_index_count, slice_count, extra_points = (
            struct.unpack_from("<Hhhhhh", data, off + 36)
        )
        points_ptr, gap_indices_ptr, slices_ptr, pad = struct.unpack_from("<IIII", data, off + 48)
        lines.append(
            {
                "index": i,
                "scale": scale,
                "origin": origin,
                "length": length,
                "flags": flags,
                "points_per_slice": points_per_slice,
                "point_count": point_count,
                "gap_index_count": gap_index_count,
                "slice_count": slice_count,
                "extra_points": extra_points,
                "points_file_offset": points_ptr + skew,
                "gap_indices_file_offset": gap_indices_ptr + skew,
                "slices_file_offset": slices_ptr + skew,
                "pad": pad,
            }
        )
    return lines


def read_race_line_slice_map(data, race_line, slice_count):
    """Read a race-line's per-mesh-slice map: one int16 per race-mesh slice,
    -1 where the line does not drive that slice and otherwise the index of
    that slice's last race-line point. Length is the MESH's slice count,
    which every shipped line's own slice-count field agrees with."""
    return list(
        struct.unpack_from(
            "<" + "h" * slice_count, data, race_line["slices_file_offset"]
        )
    )


def read_race_line_points(data, race_line, count):
    """Decode `count` race-line points: four uint16 each, dequantised as
    origin + (q / RACE_LINE_QUANT) * scale. Returns 3-tuples in raw engine
    units; the fourth component is always zero in the shipped data (every
    line's scale.w and origin.w are 0.0), so it is dropped."""
    pts = []
    for i in range(count):
        q = struct.unpack_from(
            "<HHHH", data, race_line["points_file_offset"] + i * RACE_LINE_POINT_SIZE
        )
        pts.append(
            tuple(
                race_line["origin"][c] + (q[c] / RACE_LINE_QUANT) * race_line["scale"][c]
                for c in range(3)
            )
        )
    return pts


def check_race_line_length(data, race_line, slice_map):
    """Self-check on the whole race-line decode. The points the slice map
    references (0 .. max mapped index) form a closed loop whose length is the
    `length` float stored in the race-line header; the trailing
    `extra-points` are not on it. Getting the point stride, the quantisation
    divisor, the pointer rebasing or the slice map wrong all move this number,
    so agreement to RACE_LINE_LENGTH_TOL is strong evidence the structure is
    being read as authored. Returns (measured_m, stored_m, relative_error)."""
    mapped = [v for v in slice_map if v >= 0]
    pts = read_race_line_points(data, race_line, max(mapped) + 1)
    n = len(pts)
    measured = sum(dist3(pts[i], pts[(i + 1) % n]) for i in range(n)) / METER_LENGTH
    stored = race_line["length"]
    return measured, stored, abs(measured - stored) / stored


def select_edges_for_race_line(mesh_slices, slice_map):
    """The centerline edge sequence for one race line: walk the mesh slices in
    order and take the start-edge of every slice the line marks valid. The
    result is already ordered along the track and contains no duplicates,
    because a fork's two branches never share a start-edge on the same line."""
    return [mesh_slices[i][0] for i, v in enumerate(slice_map) if v >= 0]


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


def emit_goal_fragment(midpoints, source_name, source_md5, out, symbol=TABLE_SYMBOL, race_line=None):
    header_line = "(define {} (new 'static 'boxed-array :type vector".format(symbol)
    indent = header_line.index("(new 'static 'boxed-array") + 2
    pad = " " * indent

    # the race-line suffix is appended only in race-line mode, so a default
    # run's markers stay character for character what is already in level.gc
    provenance = "source {}, md5 {}".format(source_name, source_md5)
    if race_line is not None:
        provenance += ", race line {}".format(race_line)

    print(";; BEGIN GENERATED: {} ({})".format(SCRIPT_NAME, provenance), file=out)
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
    print(";; END GENERATED: {} ({})".format(SCRIPT_NAME, provenance), file=out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("go_path", type=Path, help="path to the raw .go level object")
    parser.add_argument(
        "--race-line",
        type=int,
        default=None,
        metavar="N",
        help="select edges from race line N's own slice map instead of running "
        "the geometric fork-interleave filter. Required for any mesh with a "
        "fork the filter is not tuned for.",
    )
    parser.add_argument(
        "--symbol",
        default=TABLE_SYMBOL,
        help="GOAL symbol to define (default {})".format(TABLE_SYMBOL),
    )
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

    if args.race_line is not None:
        run_race_line_mode(
            data, header, skew, edges, known, name, actual_md5, args.race_line, args.symbol
        )
        return

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

    pass1_kept, pass1_dropped = filter_fork_branch(midpoints, sys.stderr)
    pass2_kept, pass2_removed = remove_spikes(pass1_kept, sys.stderr)

    total_removed = len(pass1_dropped) + len(pass2_removed)
    final_points = [p for _idx, p in pass2_kept]

    if total_removed > MAX_REMOVED:
        sys.exit(
            "error: fork filter removed {} points across both passes, expected "
            "at most {}".format(total_removed, MAX_REMOVED)
        )
    if len(final_points) < MIN_KEPT:
        sys.exit(
            "error: only {} points remain after fork filtering, expected at "
            "least {}".format(len(final_points), MIN_KEPT)
        )

    # step distances around the final, closed loop: consecutive kept points
    # plus the lap seam back to point 0
    steps_m = [
        dist3(final_points[i], final_points[(i + 1) % len(final_points)]) / METER_LENGTH
        for i in range(len(final_points))
    ]
    max_step_m = max(steps_m)
    min_step_m = min(steps_m)
    xs_m, zs_m, reversal_count = check_loop_geometry(
        final_points, known, MAX_STEP_M, "after both filter passes"
    )

    print(
        "extractor sanity: {} points after both fork-filter passes ({} pass 1, "
        "{} pass 2, {} total removed), lap fractions strictly monotonic, bbox "
        "(meters) X {:.2f}..{:.2f} Z {:.2f}..{:.2f}, step range (meters, incl. "
        "lap seam) {:.2f}..{:.2f}, reversals {}".format(
            len(final_points),
            len(pass1_dropped),
            len(pass2_removed),
            total_removed,
            min(xs_m),
            max(xs_m),
            min(zs_m),
            max(zs_m),
            min_step_m,
            max_step_m,
            reversal_count,
        ),
        file=sys.stderr,
    )

    emit_goal_fragment(final_points, name, actual_md5, sys.stdout, symbol=args.symbol)


def check_loop_geometry(final_points, known, max_step_m_limit, reversal_context):
    """Gates shared by both selection modes: no step (lap seam included) over
    the limit, the expected number of consecutive-direction reversals, and a
    bbox inside the file's known-good one. Returns (xs_m, zs_m,
    reversal_count) so the caller can report them."""
    steps_m = [
        dist3(final_points[i], final_points[(i + 1) % len(final_points)]) / METER_LENGTH
        for i in range(len(final_points))
    ]
    max_step_m = max(steps_m)
    if max_step_m > max_step_m_limit:
        sys.exit(
            "error: max final step is {:.2f}m (including the lap seam), expected "
            "at most {}m".format(max_step_m, max_step_m_limit)
        )

    reversal_count = count_reversals(final_points)
    expected_reversals = known["expected_reversals"]
    if reversal_count != expected_reversals:
        sys.exit(
            "error: {} remaining consecutive-direction reversal(s) {}, expected "
            "exactly {}".format(reversal_count, reversal_context, expected_reversals)
        )

    xs_m = [m[0] / METER_LENGTH for m in final_points]
    zs_m = [m[2] / METER_LENGTH for m in final_points]
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
    return xs_m, zs_m, reversal_count


def run_race_line_mode(data, header, skew, edges, known, name, actual_md5, line_index, symbol):
    """--race-line N: take the centerline from race line N's own slice map
    rather than from the geometric fork filter, then run the same closed-loop
    gates. Emits the GOAL fragment on stdout and a sanity line on stderr."""
    known_lines = known.get("race_lines")
    if not known_lines or line_index not in known_lines:
        sys.exit(
            "error: no known-good race line {} for '{}'. Add it to that file's "
            "'race_lines' entry in KNOWN_FILES in {} (point count, lap length, "
            "max step) before baking it.".format(line_index, name, SCRIPT_NAME)
        )
    known_line = known_lines[line_index]

    if not (0 <= line_index < header["race_line_count"]):
        sys.exit(
            "error: race line {} out of range: the mesh holds {}".format(
                line_index, header["race_line_count"]
            )
        )

    mesh_slices = read_mesh_slices(data, header["slices_ptr"] + skew, header["slice_count"])
    race_lines = read_race_lines(
        data, header["fourth_ptr"] + skew, header["race_line_count"], skew
    )
    race_line = race_lines[line_index]
    if race_line["slice_count"] != header["slice_count"]:
        sys.exit(
            "error: race line {}'s slice-count {} disagrees with the mesh's {}; "
            "the slice map cannot be read".format(
                line_index, race_line["slice_count"], header["slice_count"]
            )
        )

    slice_map = read_race_line_slice_map(data, race_line, header["slice_count"])
    measured_m, stored_m, rel_err = check_race_line_length(data, race_line, slice_map)
    if rel_err > RACE_LINE_LENGTH_TOL:
        sys.exit(
            "error: race line {}'s decoded point loop measures {:.2f}m but its "
            "header stores {:.2f}m (relative error {:.2e}, tolerance {:.0e}). The "
            "race-line structure is not being read as authored; refusing to "
            "bake.".format(line_index, measured_m, stored_m, rel_err, RACE_LINE_LENGTH_TOL)
        )

    selected = select_edges_for_race_line(mesh_slices, slice_map)
    if len(selected) != len(set(selected)):
        sys.exit("error: race line {}'s slice map yields duplicate edges".format(line_index))
    if selected != sorted(selected):
        sys.exit(
            "error: race line {}'s selected edges are not in increasing order".format(line_index)
        )
    if len(selected) != known_line["point_count"]:
        sys.exit(
            "error: race line {} selects {} edges, expected the known-good {}".format(
                line_index, len(selected), known_line["point_count"]
            )
        )

    lap_fractions = [edges[e][0][3] for e in selected]
    if not all(lap_fractions[i] < lap_fractions[i + 1] for i in range(len(lap_fractions) - 1)):
        sys.exit(
            "error: lap fractions are not strictly monotonic across race line "
            "{}'s selected edges".format(line_index)
        )

    final_points = [
        (
            (edges[e][0][0] + edges[e][1][0]) / 2.0,
            (edges[e][0][1] + edges[e][1][1]) / 2.0,
            (edges[e][0][2] + edges[e][1][2]) / 2.0,
        )
        for e in selected
    ]

    xs_m, zs_m, reversal_count = check_loop_geometry(
        final_points,
        known,
        known_line["max_step_m"],
        "on race line {}'s selected edges".format(line_index),
    )

    steps_m = [
        dist3(final_points[i], final_points[(i + 1) % len(final_points)]) / METER_LENGTH
        for i in range(len(final_points))
    ]
    lap_length_m = sum(steps_m)
    if abs(lap_length_m - known_line["lap_length_m"]) > LENGTH_SLACK_M:
        sys.exit(
            "error: race line {}'s centerline lap measures {:.2f}m, expected the "
            "known-good {}m (+/- {})".format(
                line_index, lap_length_m, known_line["lap_length_m"], LENGTH_SLACK_M
            )
        )

    invalid = [i for i, v in enumerate(slice_map) if v < 0]
    print(
        "extractor sanity: race line {} of {}, {} points from its slice map ({} "
        "of {} mesh slices off this line: {}), race-line point loop {:.2f}m vs "
        "stored {:.2f}m (rel err {:.2e}), lap fractions strictly monotonic, "
        "centerline lap {:.2f}m, bbox (meters) X {:.2f}..{:.2f} Z {:.2f}..{:.2f}, "
        "step range (meters, incl. lap seam) {:.2f}..{:.2f}, reversals {}".format(
            line_index,
            header["race_line_count"],
            len(final_points),
            len(invalid),
            header["slice_count"],
            invalid,
            measured_m,
            stored_m,
            rel_err,
            lap_length_m,
            min(xs_m),
            max(xs_m),
            min(zs_m),
            max(zs_m),
            min(steps_m),
            max(steps_m),
            reversal_count,
        ),
        file=sys.stderr,
    )

    emit_goal_fragment(
        final_points, name, actual_md5, sys.stdout, symbol=symbol, race_line=line_index
    )


if __name__ == "__main__":
    main()
