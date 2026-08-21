import argparse
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# Transcode a Jak X FMV source (a raw MPEG-2 elementary-stream .M2V cutscene under
# iso_data/jakx/STR/) into an MJV1 container: a small header, a frame offset/size
# table, and concatenated baseline-JPEG blobs, one per source frame. This is Lane 0
# of the FMV Road B design (issue 569): the runtime side (MjvVideoReader in
# game/graphics/texture/, landed by a later lane) decodes one JPEG blob per
# displayed frame with the already-linked stb_image, keyed off elapsed playback
# time rather than a frame index. This script never touches goal_src or the
# runtime; it is a standalone offline tool, not wired into Taskfile.yml or CI.
#
# CONTAINER FORMAT (MJV1), all integers little-endian
# -----------------------------------------------------------------------------
# Header (32 bytes, every multi-byte field lands on a natural alignment boundary
# so a C reader can memcpy this straight into a packed struct):
#
#   offset  size  field         meaning
#   0       4     magic         b"MJV1" -- ASCII tag + inline version digit, so a
#                                hex dump alone identifies the format; the numeric
#                                `version` field below is the authoritative one a
#                                reader should branch on.
#   4       2     version       container format version (this is 1)
#   6       2     reserved0     zero; pads magic+version to a 4-byte edge before
#                                the uint32 run starts
#   8       2     width         frame width in pixels; must equal the SOF0 width
#                                baked into every JPEG blob -- present here so a
#                                reader can size its framebuffer once, before
#                                touching any frame
#   10      2     height        frame height in pixels
#   12      4     fps_num       playback frame-rate numerator
#   16      4     fps_den       playback frame-rate denominator (fps =
#                                fps_num/fps_den as a rational, NOT a float32 --
#                                NTSC rates like 30000/1001 have no exact binary-
#                                float representation, and a rational is also how
#                                ffprobe/ffmpeg themselves report r_frame_rate, so
#                                this mirrors the source of truth instead of
#                                pre-rounding it away)
#   20      4     frame_count   number of entries in the frame table, and of JPEG
#                                blobs that follow it
#   24      4     flags         reserved for future use (e.g. bit0 = has an
#                                accompanying audio track); must be 0 in v1
#   28      4     reserved1     zero; pads the header to 32 bytes exactly, so
#                                HEADER_STRUCT.size is a round, hard-codeable
#                                constant and the frame table below starts
#                                8-byte aligned
#
# Frame table (frame_count * 8 bytes, immediately after the header):
#
#   offset  size  field   meaning
#   +0      4     offset  absolute byte offset from file start to this frame's
#                         JPEG SOI byte (0xFFD8)
#   +4      4     size    length in bytes of this frame's JPEG blob, SOI..EOI
#                         inclusive
#
#   Absolute offsets (rather than a size-only table with implied concatenation)
#   cost 4 redundant bytes/frame -- recoverable by a running sum -- but buy O(1)
#   seek-to-frame for a runtime reader instead of an O(n) walk summing every
#   prior size. uint32 offsets cap a single container at 4 GiB, well above any
#   one source movie in this corpus.
#
# Frame data: concatenated raw JPEG byte streams in presentation order, each a
# byte-exact ffmpeg-mjpeg-encoder output (SOI...EOI), no inter-frame padding or
# sync markers -- the table's offset/size already delimits each blob exactly.
#
# Endianness: fixed little (matches the x86/x64 PC port target; no byteswap
# needed at load, unlike the big-endian-derived PS2 formats elsewhere in this
# tree).
#
# FPS CAVEAT (issue 569 risk R4) -- read before trusting fps_num/fps_den
# -----------------------------------------------------------------------------
# For THX.M2V, ffprobe's own two frame-rate fields disagree: avg_frame_rate
# (derived from the MPEG-2 sequence header) reads 25/1, while r_frame_rate (tbr,
# ffmpeg's best guess) reads 30000/1001. This is a genuinely unresolved question,
# not a bug in this script: the container simply records whatever fps_num/
# fps_den the transcode was run with (default 30000/1001, overridable with
# --fps-num/--fps-den), and the runtime reader is insensitive to which one is
# "true" because it clamps past-the-end playback to the last (measured-black)
# frame rather than failing. A visual A/B against a reference playback resolves
# this later at zero code cost: just re-run this script with the corrected
# fps-num/fps-den and nothing downstream needs to change.
#
# USAGE
# -----------------------------------------------------------------------------
#   python scripts/jakx/gen_mjv.py iso_data/jakx/STR/THX.M2V -o out/jakx/fmv/THX.MJV
#   python scripts/jakx/gen_mjv.py iso_data/jakx/STR/THX.M2V -o out/jakx/fmv/THX.MJV -q 2
#   python scripts/jakx/gen_mjv.py out/jakx/fmv/THX.MJV --verify
#
# See issue 569 for the full Road B design (this is Lane 0, the transcoder).

SCRIPT_NAME = "gen_mjv.py"

FFMPEG = r"C:\Users\alex\AppData\Local\Microsoft\WinGet\Links\ffmpeg.exe"
FFPROBE = r"C:\Users\alex\AppData\Local\Microsoft\WinGet\Links\ffprobe.exe"

MAGIC = b"MJV1"
VERSION = 1
DEFAULT_QUALITY = 5
DEFAULT_FPS_NUM = 30000
DEFAULT_FPS_DEN = 1001
MAX_DIMENSION = 2048  # sanity ceiling for --verify's header validation

HEADER_STRUCT = struct.Struct("<4sHHHHIIIII")
# < little-endian, magic(4s) version(H) reserved0(H) width(H) height(H)
#   fps_num(I) fps_den(I) frame_count(I) flags(I) reserved1(I)
assert HEADER_STRUCT.size == 32, HEADER_STRUCT.size
FRAME_ENTRY_STRUCT = struct.Struct("<II")  # offset(I) size(I)
assert FRAME_ENTRY_STRUCT.size == 8


def probe_dimensions(input_path):
    """Real pixel width/height from the source stream via ffprobe, rather than
    hard-coding THX's known 640x448 -- this script is meant to run over the
    other 42 movies in the corpus too."""
    proc = subprocess.run(
        [FFPROBE, "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "csv=p=0", str(input_path)],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"ffprobe failed on {input_path}:\n{proc.stderr}")
    w, h = proc.stdout.strip().split(",")[:2]
    return int(w), int(h)


def extract_jpeg_frames(input_path, frame_dir, quality):
    """Decode every frame of input_path to baseline JPEG stills via ffmpeg's
    native mjpeg encoder, at constant quality `quality` (1=best/largest,
    31=worst/smallest, ffmpeg mjpeg -q:v scale).

    -fps_mode passthrough (formerly -vsync 0) is load-bearing: without it
    ffmpeg retimes output to a target rate and will duplicate or drop frames,
    breaking the 1:1 source-frame <-> JPEG-blob mapping the container assumes.

    -pix_fmt yuvj420p is the R5 fix: the source is yuv420p(tv) (luma 16-235),
    and stb_image (like every JFIF-conformant decoder) assumes full-range
    YCbCr on decode. Without forcing full-range output here, black source
    pixels bake into the JPEG at tv-range 16 and stb_image would then decode
    them to RGB(16,16,16) instead of RGB(0,0,0) -- a visibly dark-grey screen
    where the movie is meant to be black. yuvj420p is the JFIF full-range
    pixel format; ffmpeg range-converts into it on the way to the encoder."""
    pattern = os.path.join(frame_dir, "frame_%06d.jpg")
    cmd = [
        FFMPEG, "-y", "-i", str(input_path),
        "-fps_mode", "passthrough",
        "-pix_fmt", "yuvj420p",
        "-q:v", str(quality),
        pattern,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"ffmpeg failed (q={quality}):\n{proc.stderr}")
    frames = sorted(
        os.path.join(frame_dir, f) for f in os.listdir(frame_dir)
        if f.startswith("frame_") and f.endswith(".jpg")
    )
    if not frames:
        raise RuntimeError(f"ffmpeg produced zero frames for {input_path}")
    return frames


def pack_container(frames, out_path, width, height, fps_num, fps_den):
    """Write the MJV1 container: header, then the frame_count * 8-byte offset/
    size table, then the concatenated JPEG blobs, in that order."""
    frame_count = len(frames)
    sizes = [os.path.getsize(f) for f in frames]
    data_start = HEADER_STRUCT.size + FRAME_ENTRY_STRUCT.size * frame_count

    offsets = []
    running = data_start
    for s in sizes:
        offsets.append(running)
        running += s

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as out:
        out.write(HEADER_STRUCT.pack(
            MAGIC, VERSION, 0, width, height,
            fps_num, fps_den, frame_count, 0, 0,
        ))
        for off, sz in zip(offsets, sizes):
            out.write(FRAME_ENTRY_STRUCT.pack(off, sz))
        for f in frames:
            with open(f, "rb") as fh:
                out.write(fh.read())

    return sum(sizes)


def sof_marker_of(data):
    """Walk JPEG markers in an in-memory blob looking for the Start-Of-Frame
    marker and return it: 0xC0 is SOF0 (baseline DCT), 0xC2 is SOF2
    (progressive DCT). stb_image supports both, but this container is meant to
    hold baseline frames (matching what ffmpeg's mjpeg encoder actually emits
    at every quality this tool exposes), so --verify treats anything other
    than SOF0 as a mismatch worth flagging."""
    i = 0
    while i < len(data) - 1:
        if data[i] != 0xFF:
            i += 1
            continue
        m = data[i + 1]
        if m in (0xD8, 0x01) or 0xD0 <= m <= 0xD7:
            i += 2
            continue
        if m == 0xFF:
            i += 1
            continue
        if i + 3 >= len(data):
            break
        seglen = (data[i + 2] << 8) + data[i + 3]
        if m in (0xC0, 0xC1, 0xC2, 0xC3):  # SOF0..SOF3
            return m
        if m == 0xDA:  # SOS: scan data follows, no more markers to parse safely
            break
        i += 2 + seglen
    return None


def read_header(f):
    header_raw = f.read(HEADER_STRUCT.size)
    if len(header_raw) != HEADER_STRUCT.size:
        raise RuntimeError(f"file too short for a {HEADER_STRUCT.size}-byte header")
    magic, version, reserved0, width, height, fps_num, fps_den, frame_count, flags, reserved1 = \
        HEADER_STRUCT.unpack(header_raw)
    return dict(magic=magic, version=version, reserved0=reserved0, width=width,
                height=height, fps_num=fps_num, fps_den=fps_den,
                frame_count=frame_count, flags=flags, reserved1=reserved1)


def validate_container(path):
    """Full structural validation: header sanity, then every table entry's
    bounds (not just a sample). Returns (header, entries, issues)."""
    issues = []
    file_size = os.path.getsize(path)
    with open(path, "rb") as f:
        header = read_header(f)
        if header["magic"] != MAGIC:
            issues.append(f"bad magic {header['magic']!r}, want {MAGIC!r}")
        if header["version"] != VERSION:
            issues.append(f"bad version {header['version']}, want {VERSION}")
        if header["flags"] != 0:
            issues.append(f"flags must be 0 in v1, got {header['flags']}")
        if not (0 < header["width"] <= MAX_DIMENSION and 0 < header["height"] <= MAX_DIMENSION):
            issues.append(f"width/height out of range: {header['width']}x{header['height']}")
        if header["frame_count"] == 0:
            issues.append("frame_count is 0")

        data_start = HEADER_STRUCT.size + FRAME_ENTRY_STRUCT.size * header["frame_count"]
        if data_start > file_size:
            issues.append(f"header + table ({data_start} B) exceeds file size ({file_size} B)")
            return header, [], issues

        table_raw = f.read(FRAME_ENTRY_STRUCT.size * header["frame_count"])
        entries = [FRAME_ENTRY_STRUCT.unpack_from(table_raw, i * 8)
                   for i in range(header["frame_count"])]

    for idx, (off, sz) in enumerate(entries):
        if off < data_start:
            issues.append(f"entry {idx}: offset {off} precedes the frame table (data starts at {data_start})")
        if off + sz > file_size:
            issues.append(f"entry {idx}: offset+size {off + sz} exceeds file size {file_size}")
        if sz == 0:
            issues.append(f"entry {idx}: zero-length frame")

    return header, entries, issues


def verify_container(path):
    """--verify: validate the header and every table entry's bounds, then
    extract first/middle/last frames to standalone .jpg files and confirm
    each is mjpeg at the header's own width/height via ffprobe, and walk
    frame 0's JPEG markers confirming SOF0 (baseline), not SOF2. Prints a
    report and returns True only if every check passed."""
    print(f"=== verifying {path} ===")
    header, entries, issues = validate_container(path)
    print(f"header: magic={header['magic']!r} version={header['version']} "
          f"{header['width']}x{header['height']} fps={header['fps_num']}/{header['fps_den']} "
          f"frame_count={header['frame_count']} flags={header['flags']}")

    if issues:
        print(f"BOUNDS: {len(issues)} problem(s):")
        for msg in issues:
            print(f"  - {msg}")
        return False
    print(f"bounds: clean ({len(entries)} table entries, all in range)")

    frame_count = header["frame_count"]
    sample_idxs = sorted({0, frame_count // 2, frame_count - 1})
    ok = True
    frame0_blob = None

    with open(path, "rb") as f, tempfile.TemporaryDirectory(prefix="gen_mjv_verify_") as tmpdir:
        probe_results = []
        for idx in sample_idxs:
            off, sz = entries[idx]
            f.seek(off)
            blob = f.read(sz)
            if idx == 0:
                frame0_blob = blob
            if blob[:2] != b"\xff\xd8" or blob[-2:] != b"\xff\xd9":
                print(f"  frame {idx}: MISSING SOI/EOI markers")
                ok = False
                continue
            tmp_path = os.path.join(tmpdir, f"frame_{idx}.jpg")
            with open(tmp_path, "wb") as tmp:
                tmp.write(blob)
            proc = subprocess.run(
                [FFPROBE, "-v", "error", "-select_streams", "v:0",
                 "-show_entries", "stream=width,height,codec_name",
                 "-of", "csv=p=0", tmp_path],
                capture_output=True, text=True,
            )
            codec, w, h = None, None, None
            if proc.returncode == 0 and proc.stdout.strip():
                parts = proc.stdout.strip().split(",")
                if len(parts) == 3:
                    codec, w, h = parts[0], int(parts[1]), int(parts[2])
            match = (codec == "mjpeg" and w == header["width"] and h == header["height"])
            probe_results.append((idx, sz, codec, w, h, match))
            if not match:
                ok = False

        print(f"extracted frames: {len(sample_idxs)}/{len(sample_idxs)} probed")
        for idx, sz, codec, w, h, match in probe_results:
            status = "OK" if match else "MISMATCH"
            print(f"  frame {idx}: {sz} B, ffprobe={codec},{w},{h}  [{status}]")

    if frame0_blob is not None:
        marker = sof_marker_of(frame0_blob)
        names = {0xC0: "SOF0 (baseline)", 0xC1: "SOF1 (ext-sequential)",
                 0xC2: "SOF2 (progressive)", 0xC3: "SOF3 (lossless)"}
        print(f"frame 0 SOF marker: {names.get(marker, marker)}")
        if marker != 0xC0:
            ok = False

    return ok


def transcode(input_path, output_path, quality, fps_num, fps_den):
    width, height = probe_dimensions(input_path)
    with tempfile.TemporaryDirectory(prefix="gen_mjv_") as tmpdir:
        frames = extract_jpeg_frames(input_path, tmpdir, quality)
        total_jpeg_bytes = pack_container(frames, output_path, width, height, fps_num, fps_den)
        frame_count = len(frames)
    container_size = os.path.getsize(output_path)
    print(f"wrote {output_path}")
    print(f"  {width}x{height}  frame_count={frame_count}  fps={fps_num}/{fps_den}  q={quality}")
    print(f"  jpeg payload: {total_jpeg_bytes} B   container: {container_size} B")
    return container_size


def main():
    ap = argparse.ArgumentParser(
        prog=SCRIPT_NAME,
        description="Transcode a Jak X .M2V FMV source into an MJV1 baseline-JPEG "
                     "container (issue 569 Road B, Lane 0), or verify an existing one.")
    ap.add_argument("input", help="source .M2V path, or .MJV path with --verify")
    ap.add_argument("-o", "--output", help="output .MJV path (required unless --verify)")
    ap.add_argument("-q", "--quality", type=int, default=DEFAULT_QUALITY,
                     help=f"ffmpeg mjpeg -q:v (1=best/largest .. 31=worst/smallest); "
                          f"default {DEFAULT_QUALITY}")
    ap.add_argument("--fps-num", type=int, default=DEFAULT_FPS_NUM,
                     help=f"container fps numerator; default {DEFAULT_FPS_NUM} (see the fps "
                          f"caveat in this file's header comment)")
    ap.add_argument("--fps-den", type=int, default=DEFAULT_FPS_DEN,
                     help=f"container fps denominator; default {DEFAULT_FPS_DEN}")
    ap.add_argument("--verify", action="store_true",
                     help="verify an existing container instead of transcoding; "
                          "INPUT is the .MJV path")
    args = ap.parse_args()

    if args.verify:
        ok = verify_container(Path(args.input))
        print("PASS" if ok else "FAIL")
        sys.exit(0 if ok else 1)

    if not args.output:
        ap.error("-o/--output is required unless --verify")

    transcode(Path(args.input), Path(args.output), args.quality, args.fps_num, args.fps_den)


if __name__ == "__main__":
    main()
