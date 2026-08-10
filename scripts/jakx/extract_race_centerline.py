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
        # expected remaining consecutive-direction reversal count after both
        # fork-interleave filter passes below; a future track's own
        # legitimate hairpin could be nonzero, so this lives per-file rather
        # than as a blanket constant
        "expected_reversals": 0,
    },
}

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
    if max_step_m > MAX_STEP_M:
        sys.exit(
            "error: max final step is {:.2f}m (including the lap seam), expected "
            "at most {}m".format(max_step_m, MAX_STEP_M)
        )

    reversal_count = count_reversals(final_points)
    expected_reversals = known["expected_reversals"]
    if reversal_count != expected_reversals:
        sys.exit(
            "error: {} remaining consecutive-direction reversal(s) after both "
            "filter passes, expected exactly {}".format(reversal_count, expected_reversals)
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

    emit_goal_fragment(final_points, name, actual_md5, sys.stdout)


if __name__ == "__main__":
    main()
