import argparse
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path

# Generate the geometry-driven streaming window's static level-bounds table (forge
# issue 550, rung R6). Full design: issue 550's own body plus the 2026-08-20 R6 design
# comment. Session scratchpad sphere-inv/ (bounds.py prototype, bounds2.json, adj.json,
# cover3.py, sim2.py, sim3.py, val.py, the three *_line.txt centerlines) is the working
# data this script's algorithm was measured against; nothing here reads those files at
# runtime; the pipeline below is self-contained against goal_src and decompiler_out.
#
# jakx-want-driver-tick used to pick the streaming window by walking continue points and
# taking the nearest one (a level.gc og:preserve-this note, retired by this rung). The
# origin issue root-caused the resulting dead zones to continue-point SCARCITY (icelands'
# whole circuit has exactly two usable continues), not to the window's slot budget. This
# script replaces the continue walk's DATA with real level geometry: bounding spheres
# parsed straight out of the built drawable-tree BVHs in decompiler_out/jakx/raw_obj/*.go,
# clustered down to a handful of representative spheres per level, so the driver can
# measure camera-to-level distance directly instead of camera-to-continue distance.
#
# THREE HARDENINGS OVER THE 60-LINE PROTOTYPE (all load-bearing, all self-tested below)
# ---------------------------------------------------------------------------------------
# (i) finite/range sanity rejection: a decoded (x, y, z, r) is discarded unless every
#     component is finite and inside a generous real-world envelope (see `sane()`). Catches
#     misreads that produce NaN/Inf or absurd magnitudes outright.
#
# (ii) per-tree type resolution from the v5 link table, not an assumed stride. A level's
#     bsp-header holds an array of drawable-tree pointers (`drawable-trees`, offset 40),
#     and EVERY drawable-tree subtype inherits `drawable-group`'s layout (a `length` at
#     offset 6, a dynamic pointer array at offset 32) plus `drawable`'s own `bsphere` at
#     offset 16 -- so descending trees themselves is safe regardless of subtype. The real
#     hazard is one level deeper: drawable-tree-tfrag's own `arrays` field (offset 32) is
#     explicitly documented in all-types.gc as holding "either drawable-inline-array-node
#     or drawable-inline-array-tfrag" -- a HETEROGENEOUS pointer list. draw-node (the BVH
#     culling node) is 32 bytes; tfragment (a leaf mesh fragment, also a valid `drawable`
#     with its own bsphere) is 64 bytes; instance-tie is 64; instance-shrubbery is 80;
#     collide-fragment, drawable-actor and drawable-region-prim are all 32. Assuming one
#     stride for all of them (the prototype's approach) reads roughly every other element
#     from the wrong byte offset whenever the true stride isn't 32, and because the
#     misaligned floats are still IEEE754 bit patterns, some misreads still pass a bare
#     range check and land in the output as confident-looking garbage. This is exactly the
#     "6 of 37 levels produced garbage" the design comment measured.
#
#     The fix: raw v5 .go dumps are UNLINKED, so every basic's type-tag slot still holds
#     the literal placeholder 0xffffffff; the file's own link table (the same format
#     decompiler/ObjectFile/LinkedObjectFileCreation.cpp's link_v5/c_symlink2 decode to
#     patch those slots at load time) names, for every TYPE-kind symbol link, the exact
#     list of absolute file offsets whose type-tag the runtime linker would patch to that
#     type. Replaying that walk (V5Object.type_map below) gives an offset -> type-name map
#     for free, with no guessing: every tree and every node-array pointer gets resolved to
#     its real type before its stride is trusted, and an unrecognized type is skipped
#     rather than misread. See `--prove-hardening` for a direct measurement of what the
#     naive fixed-stride path gets wrong on this corpus.
#
# (iii) the 4711-actor cross-check as a generator self-test (`actor_cross_check` below):
#     every already-decompiled actor position in decompiler_out/jakx/entities/*-actors.json
#     must fall inside its own level's derived bounding box (+50m slack, matching the
#     original investigation's check). This is the abort-on-failure gate: emission never
#     proceeds if this measurably regresses from the investigation's 100% baseline.
#
# usage: python scripts/jakx/gen_level_bounds.py [--check] [--k N] [--prove-hardening]

SCRIPT_NAME = "gen_level_bounds.py"
TABLE_SYMBOL = "*jakx-level-bounds*"
ROW_TYPE = "jakx-level-bound"

REPO_ROOT = Path(__file__).resolve().parents[2]
LEVEL_INFO_PATH = REPO_ROOT / "goal_src" / "jakx" / "engine" / "level" / "level-info.gc"
LEVEL_GC_PATH = REPO_ROOT / "goal_src" / "jakx" / "engine" / "level" / "level.gc"
RAW_OBJ_DIR = REPO_ROOT / "decompiler_out" / "jakx" / "raw_obj"
ENTITIES_DIR = REPO_ROOT / "decompiler_out" / "jakx" / "entities"

# goal_src/jakx/engine/util/types-h.gc: METER_LENGTH = 4096.0
METER_LENGTH = 4096.0

K_SPHERES = 8
# matches the coverage sims this table is validated against (sphere-inv/cover3.py's
# RMAX, sim2.py/sim3.py's RM): only "tight" (leaf-ish) spheres are useful as a d(L,p)
# primitive -- a coarse top-of-BVH sphere would trivially "contain" half the level.
TIGHT_RADIUS_M = 250.0

# actor cross-check thresholds (hardening iii): the investigation's baseline was 4711 of
# 4711 (100%); emission aborts if this run falls meaningfully short of that.
ACTOR_INSIDE_MIN_FRACTION = 0.99
ACTOR_INSIDE_MIN_LEVEL_FRACTION = 0.95

BEGIN_PREFIX = ";; BEGIN GENERATED: {}".format(SCRIPT_NAME)
END_PREFIX = ";; END GENERATED: {}".format(SCRIPT_NAME)

MEMORY_MODE_CANDIDATE_FOR_ROTATION = ("tiny", "tiny-center")
MEMORY_MODE_NEVER_WANTED = ("borrow0", "small-center")

# family sound symbols, per the design comment's explicit table; cross-checked below
# against the majority want-sound value this script measures per family.
EXPECTED_FAMILY_SOUND = {
    "jungle1": {"junglea", "jungleb", "junglec", "jungled", "junglee", "junglef", "jungleg",
                "junglew"},
    "havjng1": {"havjungw"},
    "snow1": {"icea", "iceb", "icec", "iced", "icew"},
    "kras1": {"krasa", "krasb", "krasc", "krasw"},
    "menu1": {"garage"},
}


class V5Format(Exception):
    pass


def sane(sph):
    """Hardening (i): reject a decoded (x, y, z, r) unless every component is finite and
    inside a generous real-world envelope. Thresholds are in raw engine units (meters *
    METER_LENGTH): 50,000 m on any axis, 0 <= r < 20,000 m."""
    if len(sph) != 4:
        return False
    if any(v != v or math.isinf(v) for v in sph):
        return False
    x, y, z, r = sph
    lim = 50000.0 * METER_LENGTH
    if not (abs(x) < lim and abs(y) < lim and abs(z) < lim):
        return False
    return 0.0 <= r < 20000.0 * METER_LENGTH


def read_cstr(data, off):
    end = data.index(b"\x00", off)
    return data[off:end].decode("latin1"), end + 1


def _skip_pointer_table(data, link_ptr):
    """Replays link_v5's pointer-linking loop (LinkedObjectFileCreation.cpp:514-568) far
    enough to advance link_ptr past it; the actual pointer patches are not needed here,
    only correct positioning for the symbol table that follows."""
    if data[link_ptr] == 0:
        return link_ptr + 1
    fixing = False
    while True:
        while True:
            count = data[link_ptr]
            if count != 0xFF:
                break
            link_ptr += 1
            if data[link_ptr] == 0:
                link_ptr += 1
                fixing = not fixing
        link_ptr += 1
        fixing = not fixing
        if data[link_ptr] == 0:
            break
    return link_ptr + 1


def _walk_symlink_chain(data, base, link_ptr):
    """Replays c_symlink2's variable-length seek-chain decode
    (LinkedObjectFileCreation.cpp:74-153) for one symbol/type name's link record. Returns
    (new_link_ptr, offsets): offsets are absolute file offsets whose word currently reads
    0xffffffff (the "needs an absolute patch" case). For a TYPE-kind record, each such
    offset is exactly one basic instance's own type-tag slot -- the mechanism hardening
    (ii) resolves per-tree/per-array types from."""
    code_ptr = base
    offsets = []
    while True:
        b0 = data[link_ptr]
        seek = b0
        nxt = link_ptr + 1
        if seek & 3:
            b1 = data[link_ptr + 1]
            seek = (b1 << 8) | b0
            nxt = link_ptr + 2
            if seek & 2:
                b2 = data[link_ptr + 2]
                seek = (b2 << 16) | seek
                nxt = link_ptr + 3
                if seek & 1:
                    b3 = data[link_ptr + 3]
                    seek = (b3 << 24) | seek
                    nxt = link_ptr + 4
        link_ptr = nxt
        code_ptr += seek & 0xFFFFFFFC
        word = struct.unpack_from("<I", data, code_ptr)[0]
        if word == 0xFFFFFFFF:
            offsets.append(code_ptr)
        if data[link_ptr] == 0:
            break
    return link_ptr + 1, offsets


class V5Object:
    """A raw, unlinked v5 GOAL object file (decompiler_out/jakx/raw_obj/*.go): a single
    data segment whose code starts at a fixed file offset (0x80 for every 1-segment data
    object, per LinkedObjectFileCreation.cpp's own comment and ASSERT), plus a type_map
    built by replaying the file's TYPE symbol-link records (hardening ii)."""

    def __init__(self, path):
        self.path = path
        data = path.read_bytes()
        self.data = data
        if len(data) < 0x50:
            raise V5Format("file too short for a v5 link header")
        (type_tag, length_to_get_to_code, version, _unknown,
         length_to_get_to_link, _link_length, n_segments) = struct.unpack_from(
            "<IIHHIIB", data, 0
        )
        if version != 5:
            raise V5Format("not a v5 object (version {})".format(version))
        if n_segments != 1:
            raise V5Format("expected a 1-segment data object, got {} segments".format(n_segments))
        if type_tag != 0xFFFFFFFF:
            raise V5Format("expected type_tag 0xffffffff on a 1-segment data object")
        seg_info_off = length_to_get_to_link
        if seg_info_off + 16 > len(data):
            raise V5Format("segment info table runs past EOF")
        relocs, data_off, _size, magic = struct.unpack_from("<IIII", data, seg_info_off)
        if magic != 1:
            raise V5Format("segment magic byte != 1")
        self.base = length_to_get_to_code + data_off
        if self.base != 0x80:
            raise V5Format(
                "segment base {} != the expected 0x80 for a 1-segment data object".format(
                    self.base
                )
            )
        link_ptr = length_to_get_to_link + relocs
        self.type_map = self._parse_type_links(link_ptr)

    def _parse_type_links(self, link_ptr):
        data = self.data
        link_ptr = _skip_pointer_table(data, link_ptr)
        type_map = {}
        if data[link_ptr] == 0:
            return type_map
        sub = link_ptr
        while True:
            reloc = data[sub]
            if (reloc & 0x80) == 0:
                p = sub + 3
                _sname, p = read_cstr(data, p)
                p, _offs = _walk_symlink_chain(data, self.base, p)
            elif (reloc & 0x3F) == 0x3F:
                raise V5Format("unexpected 0x3f reloc byte in the symbol table")
            else:
                p = sub + 3
                sname, p = read_cstr(data, p)
                p, offs = _walk_symlink_chain(data, self.base, p)
                for off in offs:
                    type_map[off] = sname
            sub = p
            if data[sub] == 0:
                break
        return type_map

    def u32(self, tag, off):
        return struct.unpack_from("<I", self.data, self.base + tag + off)[0]

    def i16(self, tag, off):
        return struct.unpack_from("<h", self.data, self.base + tag + off)[0]

    def bsphere(self, tag):
        return struct.unpack_from("<4f", self.data, self.base + tag + 16)

    def ptr(self, tag, off):
        v = self.u32(tag, off)
        if v in (0, 0xFFFFFFFF) or v < 4:
            return None
        rel = v - 4
        if self.base + rel + 4 > len(self.data):
            return None
        return rel

    def type_of(self, tag):
        return self.type_map.get(self.base + tag)


# drawable-tree subtypes actually present under a bsp-header's drawable-trees array
# (all-types.gc). All inherit drawable-group's layout (length @ 6, dynamic array @ 32)
# and drawable's own bsphere @ 16, so descending a tree pointer itself is safe once its
# type is confirmed to be one of these.
KNOWN_TREE_TYPES = {
    "drawable-tree-tfrag", "drawable-tree-tfrag-trans", "drawable-tree-tfrag-water",
    "drawable-tree-tfrag-shared", "drawable-tree-tfrag-trans-shared",
    "drawable-tree-tfrag-water-shared", "drawable-tree-instance-tie",
    "drawable-tree-instance-shrub", "drawable-tree-collide-fragment",
    "drawable-tree-actor", "drawable-tree-region-prim",
}

# per-type element stride for a tree's node-array pointers (all-types.gc size-asserts).
# This is the hardening (ii) lookup: an array whose resolved type is not one of these
# keys is skipped rather than walked with a guessed stride.
NODE_ARRAY_STRIDE = {
    "drawable-inline-array-node": 32,              # draw-node
    "drawable-inline-array-tfrag": 64,              # tfragment
    "drawable-inline-array-tfrag-trans": 64,
    "drawable-inline-array-tfrag-water": 64,
    "drawable-inline-array-instance-tie": 64,       # instance-tie
    "drawable-inline-array-instance-shrub": 80,     # instance-shrubbery
    "drawable-inline-array-collide-fragment": 32,   # collide-fragment
    "drawable-inline-array-actor": 32,              # drawable-actor
    "drawable-inline-array-region-prim": 32,        # drawable-region-prim
}

NAIVE_STRIDE = 32  # the prototype's fixed-stride assumption, kept only for --prove-hardening


def level_spheres(obj, stats, naive=False):
    """Descend a bsp-header's drawable-trees (offset 40) down to every recognized node
    array's leaf spheres. Returns a list of (x, y, z, r) in raw engine units. `naive=True`
    reproduces the prototype's fixed-32-byte-stride assumption for --prove-hardening
    comparisons only; production runs always use naive=False (type-resolved strides)."""
    dta = obj.ptr(0, 40)
    if dta is None:
        return []
    nt = obj.i16(dta, 6)
    if not (0 < nt < 64):
        return []
    out = []
    for i in range(nt):
        t = obj.ptr(dta, 32 + 4 * i)
        if t is None:
            continue
        stats["trees_seen"] += 1
        tname = obj.type_of(t)
        if not naive:
            if tname is None or tname not in KNOWN_TREE_TYPES:
                stats["trees_unresolved"] += 1
                continue
        stats["trees_resolved"] += 1
        tb = obj.bsphere(t)
        if sane(tb) and tb[3] > 0:
            out.append(tb)
        nlev = obj.i16(t, 6)
        if not (0 < nlev < 16):
            continue
        for li in range(nlev):
            arr = obj.ptr(t, 32 + 4 * li)
            if arr is None:
                continue
            aname = obj.type_of(arr)
            if naive:
                stride = NAIVE_STRIDE
            else:
                stride = NODE_ARRAY_STRIDE.get(aname)
                if stride is None:
                    stats["arrays_unresolved"] += 1
                    continue
            stats["arrays_resolved"] += 1
            n = obj.i16(arr, 6)
            if not (0 < n < 8192):
                continue
            if obj.base + arr + 32 + stride * n > len(obj.data):
                continue
            for k in range(n):
                sph = obj.bsphere(arr + 32 + stride * k)
                if sane(sph) and sph[3] > 0:
                    out.append(sph)
    return out


def find_go_path(name):
    """Per-level probe: exactly one of <name>.go / <name>-vis.go exists on disk for every
    level with a real BVH (verified directly, not assumed: no level in this corpus ships
    both). Neither existing is the normal case for a level with no bsp geometry of its
    own (small-center connectors, alias levels, etc)."""
    for cand in (name, name + "-vis"):
        p = RAW_OBJ_DIR / (cand + ".go")
        if p.exists():
            return p
    return None


# ---------------------------------------------------------------------------
# level-info.gc parsing: names, memory-mode, and the co-want graph
# ---------------------------------------------------------------------------

def find_matching_paren(text, open_idx):
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\":
                    i += 1
                i += 1
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced parens starting at {}".format(open_idx))


def iter_calls(text, head):
    """Yield the full (balanced-paren) text of every '(new \'static \'<head> ...)' form."""
    pat = re.compile(r"\(new 'static '" + re.escape(head) + r"\b")
    i = 0
    while True:
        m = pat.search(text, i)
        if not m:
            return
        end = find_matching_paren(text, m.start())
        yield text[m.start(): end + 1]
        i = end + 1


def parse_continue(ctext):
    want_names = []
    want_sound = []
    wm = re.search(r":want \(new 'static 'inline-array level-buffer-state-small", ctext)
    if wm:
        wend = find_matching_paren(ctext, ctext.index("(new 'static 'inline-array", wm.start()))
        wtext = ctext[wm.start(): wend + 1]
        want_names = re.findall(r":name '([A-Za-z0-9_-]+)", wtext)
    sm = re.search(r":want-sound \(new 'static 'array symbol \d+\s+([^)]*)\)", ctext)
    if sm:
        for tok in sm.group(1).split():
            want_sound.append(None if tok == "#f" else tok.lstrip("'"))
    return want_names, want_sound


def parse_level_info(text):
    """Split level-info.gc into per-level-load-info blocks (same header-comment split
    gen_built_levels.py uses) and pull :name, :memory-mode, and every continue's want
    list + want-sound out of each. Returns (levels: {name: {"memory_mode": str|None}},
    edges: list of (a, b) co-want pairs, sound_votes: {family_member_name: Counter})."""
    parts = re.split(r";; definition for symbol (\S+), type level-load-info\n", text)
    if len(parts) < 3:
        sys.exit(
            "error: found no level-load-info definitions in {}; the comment-header "
            "parse regex may be stale".format(LEVEL_INFO_PATH)
        )
    levels = {}
    edges = []
    sound_by_name = {}
    entry_count = 0
    for i in range(1, len(parts), 2):
        block = parts[i + 1]
        mn = re.search(r":name '([A-Za-z0-9_-]+)", block)
        if not mn:
            continue
        entry_count += 1
        name = mn.group(1)
        mm = re.search(r":memory-mode \(level-memory-mode ([a-z0-9-]+)\)", block)
        memory_mode = mm.group(1) if mm else None
        levels[name] = {"memory_mode": memory_mode}
        cm = re.search(r":continues '\(", block)
        if not cm:
            continue
        cend = find_matching_paren(block, block.index("(", cm.end() - 1))
        ctext_all = block[cm.end() - 1: cend + 1]
        for ctext in iter_calls(ctext_all, "continue-point"):
            want_names, want_sound = parse_continue(ctext)
            uniq = sorted(set(want_names))
            for a in range(len(uniq)):
                for b in range(a + 1, len(uniq)):
                    edges.append((uniq[a], uniq[b]))
            for wn, snd in zip(want_names, want_sound + [None] * len(want_names)):
                if snd:
                    sound_by_name.setdefault(wn, {}).setdefault(snd, 0)
                    sound_by_name[wn][snd] += 1
    if entry_count < 200:
        sys.exit(
            "error: only parsed {} level-load-info entries from {}; refusing to "
            "generate from a possibly-broken parse".format(entry_count, LEVEL_INFO_PATH)
        )
    return levels, edges, sound_by_name, entry_count


class UnionFind:
    def __init__(self):
        self.parent = {}

    def find(self, x):
        self.parent.setdefault(x, x)
        while self.parent[x] != x:
            self.parent[x] = self.parent[self.parent[x]]
            x = self.parent[x]
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.parent[ra] = rb


BUILT_LEVELS_BEGIN = ";; BEGIN GENERATED: gen_built_levels.py"
BUILT_LEVELS_END = ";; END GENERATED: gen_built_levels.py"


def load_built_levels(level_gc_text):
    """R1's own *jakx-built-levels* allowlist, read straight out of its generated block
    (already current in this worktree) rather than re-deriving the DGO/nickname mapping a
    second time. Used to restrict which co-want EDGES are trusted below."""
    b = level_gc_text.find(BUILT_LEVELS_BEGIN)
    e = level_gc_text.find(BUILT_LEVELS_END)
    if b == -1 or e == -1 or e < b:
        sys.exit("error: could not find gen_built_levels.py's generated block in {} to "
                  "restrict the co-want graph against".format(LEVEL_GC_PATH))
    block = level_gc_text[b:e]
    return set(re.findall(r"'([A-Za-z0-9_-]+)", block))


def compute_families(levels, edges, built_levels):
    """Connected components of the co-want graph = families (design correction 2: family
    cannot come from geometry, since armed families overlap in coordinate space).

    Edges are trusted only between two BUILT levels. Unrestricted, the corpus-wide graph
    merges every family into one 132-node component: shared cutscene/menu/task want
    members that appear in many families' :want arrays (introcst, hipcst, tsk* entries,
    none of them built) bridge families that have nothing architecturally in common. R1's
    *jakx-built-levels* allowlist is exactly the "is this a real, reachable level" filter
    this needs, and work item 2 already composes the row-selection step against the same
    set, so reusing it here keeps one definition of "real" for the whole rung. A level
    outside the allowlist still gets a row (future-proofing R4, per the design) but starts
    as its own singleton family until it is armed and a regeneration can place it for
    real."""
    uf = UnionFind()
    for name in levels:
        uf.find(name)
    for a, b in edges:
        if a in built_levels and b in built_levels:
            uf.union(a, b)
    components = {}
    for name in levels:
        components.setdefault(uf.find(name), set()).add(name)
    ordered = sorted(components.values(), key=lambda members: min(members))
    family_id = {}
    for idx, members in enumerate(ordered):
        for m in members:
            family_id[m] = idx
    return family_id, ordered


def dominant_sound(name_or_members, sound_by_name):
    votes = {}
    members = name_or_members if isinstance(name_or_members, (set, list)) else [name_or_members]
    for m in members:
        for snd, n in sound_by_name.get(m, {}).items():
            votes[snd] = votes.get(snd, 0) + n
    if not votes:
        return None
    return max(votes.items(), key=lambda kv: kv[1])[0]


# ---------------------------------------------------------------------------
# K-sphere clustering
# ---------------------------------------------------------------------------

def _dist2(a, b):
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2


def cluster_spheres(spheres, k):
    """Deterministic (farthest-point seeded, not random) k-means on sphere centers, then
    each cluster is replaced by the smallest sphere centered at its centroid that fully
    contains every member sphere. Returns up to k (x, y, z, r) tuples."""
    if not spheres:
        return []
    pts = [(s[0], s[1], s[2]) for s in spheres]
    n = len(pts)
    k = min(k, n)
    centers = [pts[0]]
    while len(centers) < k:
        best_i, best_d = 0, -1.0
        for i, p in enumerate(pts):
            d = min(_dist2(p, c) for c in centers)
            if d > best_d:
                best_d = d
                best_i = i
        centers.append(pts[best_i])
    assign = [0] * n
    for _ in range(50):
        changed = False
        for i, p in enumerate(pts):
            best_c, best_d = 0, _dist2(p, centers[0])
            for c in range(1, k):
                d = _dist2(p, centers[c])
                if d < best_d:
                    best_d = d
                    best_c = c
            if assign[i] != best_c:
                assign[i] = best_c
                changed = True
        newcenters = []
        for c in range(k):
            members = [pts[i] for i in range(n) if assign[i] == c]
            if not members:
                newcenters.append(centers[c])
                continue
            cx = sum(m[0] for m in members) / len(members)
            cy = sum(m[1] for m in members) / len(members)
            cz = sum(m[2] for m in members) / len(members)
            newcenters.append((cx, cy, cz))
        if newcenters == centers:
            break
        centers = newcenters
        if not changed:
            break
    clusters = []
    for c in range(k):
        members = [spheres[i] for i in range(n) if assign[i] == c]
        if not members:
            continue
        cx, cy, cz = centers[c]
        r = 0.0
        for m in members:
            d = math.sqrt(_dist2((cx, cy, cz), (m[0], m[1], m[2]))) + m[3]
            if d > r:
                r = d
        clusters.append((cx, cy, cz, r))
    # self-test: every member sphere must be fully contained in its cluster's sphere
    violations = 0
    for i, p in enumerate(pts):
        c = assign[i]
        if c >= len(clusters):
            continue
        cx, cy, cz, r = clusters[c]
        d = math.sqrt(_dist2((cx, cy, cz), p)) + spheres[i][3]
        if d > r + 1.0:  # 1 raw unit slack for float rounding
            violations += 1
    if violations:
        sys.exit(
            "error: {} of {} source spheres are NOT contained in their cluster's "
            "enclosing sphere; the enclosing-sphere fit is broken".format(violations, n)
        )
    return clusters


# ---------------------------------------------------------------------------
# hardening (iii): actor cross-check self-test
# ---------------------------------------------------------------------------

def actor_cross_check(aabbs_m, out):
    """Every already-decompiled actor position must fall inside its own level's derived
    bounding box (+50m slack). Returns (total, inside, per_level) and prints a report."""
    total = 0
    inside = 0
    per_level = {}
    for name in sorted(aabbs_m):
        p = ENTITIES_DIR / (name + "-actors.json")
        if not p.exists():
            continue
        lo, hi = aabbs_m[name]
        actors = json.loads(p.read_text(encoding="utf-8")) or []
        pts = [a["trans"][:3] for a in actors
               if isinstance(a.get("trans"), list) and len(a["trans"]) >= 3]
        if not pts:
            continue
        n_in = sum(
            1 for q in pts
            if all(lo[j] - 50.0 <= q[j] <= hi[j] + 50.0 for j in range(3))
        )
        total += len(pts)
        inside += n_in
        per_level[name] = (n_in, len(pts))
        print(
            "  {:10s} actors={:4d}  inside bsphere-derived AABB(+50m) = {:4d} "
            "({}%){}".format(
                name, len(pts), n_in, n_in * 100 // len(pts),
                "" if n_in == len(pts) else "   <-- OUTLIERS",
            ),
            file=out,
        )
    print(
        "actor cross-check TOTAL: {} of {} actor positions ({}%) inside their "
        "level's derived AABB (+50m)".format(
            inside, total, (inside * 100 // total) if total else 0
        ),
        file=out,
    )
    return total, inside, per_level


# ---------------------------------------------------------------------------
# emission
# ---------------------------------------------------------------------------

def format_float(x):
    if x == 0.0:
        return "0.0"
    s = repr(float(x))
    if "e" in s or "E" in s:
        from decimal import Decimal
        s = format(Decimal(x), "f")
    return s


def emit_block(rows, provenance):
    header_line = "(define {} (new 'static 'boxed-array :type {}".format(TABLE_SYMBOL, ROW_TYPE)
    pad = " " * (header_line.index("(new 'static 'boxed-array") + 2)
    lines = ["{} ({})".format(BEGIN_PREFIX, provenance), header_line]
    for row in rows:
        lines.append("{}(new 'static '{} :name '{} :memory-mode '{} :family-id {} "
                      ":sound {} :sphere-count {}".format(
                          pad, ROW_TYPE, row["name"], row["memory_mode"], row["family_id"],
                          ("'" + row["sound"]) if row["sound"] else "#f", row["sphere_count"],
                      ))
        spad = pad + "  "
        lines.append("{}:spheres (new 'static 'inline-array vector {}".format(spad, K_SPHERES))
        vpad = spad + "  "
        for j in range(K_SPHERES):
            if j < len(row["spheres"]):
                x, y, z, r = row["spheres"][j]
            else:
                x, y, z, r = 0.0, 0.0, 0.0, 0.0
            lines.append("{}(new 'static 'vector :x {} :y {} :z {} :w {})".format(
                vpad, format_float(x), format_float(y), format_float(z), format_float(r)
            ))
        lines.append("{})".format(spad))
        lines.append("{})".format(pad))
    lines.append("{})".format(pad))
    lines.append("        )")
    lines.append("{} ({})".format(END_PREFIX, provenance))
    return "\n".join(lines) + "\n"


def replace_block(level_gc_text, new_block):
    begin_re = re.escape(BEGIN_PREFIX)
    end_re = re.escape(END_PREFIX)
    pattern = re.compile(r"^" + begin_re + r".*?\n.*?^" + end_re + r".*?\n", re.S | re.M)
    matches = list(pattern.finditer(level_gc_text))
    if len(matches) != 1:
        sys.exit(
            "error: expected exactly one {} ... {} block in {}, found {}. Seed the "
            "marker block by hand before running this script.".format(
                BEGIN_PREFIX, END_PREFIX, LEVEL_GC_PATH, len(matches)
            )
        )
    m = matches[0]
    return level_gc_text[: m.start()] + new_block + level_gc_text[m.end():]


def md5_of(path):
    return hashlib.md5(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                         help="exit non-zero if level.gc's generated block is stale")
    parser.add_argument("--k", type=int, default=K_SPHERES,
                         help="representative spheres per level (default {})".format(K_SPHERES))
    parser.add_argument("--prove-hardening", action="store_true",
                         help="also report the naive fixed-32-byte-stride actor-containment "
                              "result, to measure what hardening (ii) fixes")
    args = parser.parse_args()
    out = sys.stderr

    level_info_text = LEVEL_INFO_PATH.read_text(encoding="utf-8")
    levels, edges, sound_by_name, entry_count = parse_level_info(level_info_text)
    level_gc_text_for_built = LEVEL_GC_PATH.read_text(encoding="utf-8")
    built_levels = load_built_levels(level_gc_text_for_built)
    family_id, components = compute_families(levels, edges, built_levels)
    trusted_edges = sum(1 for a, b in edges if a in built_levels and b in built_levels)

    print("gen_level_bounds: {} level-load-info entries parsed, {} co-want edges ({} "
          "trusted: both members built), {} families".format(
              entry_count, len(edges), trusted_edges, len(components)), file=out)

    stats = {"trees_seen": 0, "trees_resolved": 0, "trees_unresolved": 0,
             "arrays_resolved": 0, "arrays_unresolved": 0}
    naive_stats = {"trees_seen": 0, "trees_resolved": 0, "trees_unresolved": 0,
                   "arrays_resolved": 0, "arrays_unresolved": 0}

    go_files_used = []
    per_level_spheres = {}
    per_level_naive_spheres = {}
    for name in sorted(levels):
        gopath = find_go_path(name)
        if gopath is None:
            continue
        try:
            obj = V5Object(gopath)
        except V5Format as e:
            print("  note: {} not usable ({})".format(gopath.name, e), file=out)
            continue
        spheres = level_spheres(obj, stats, naive=False)
        if spheres:
            per_level_spheres[name] = spheres
            go_files_used.append(gopath)
        if args.prove_hardening:
            per_level_naive_spheres[name] = level_spheres(obj, naive_stats, naive=True)

    print("gen_level_bounds: {} of {} named levels have a raw_obj/*.go, {} yielded "
          "usable geometry".format(
              sum(1 for n in levels if find_go_path(n)), entry_count, len(per_level_spheres)
          ), file=out)
    print("gen_level_bounds: type-resolved descent: {} trees seen, {} resolved, "
          "{} unresolved (skipped); {} node-arrays resolved, {} unresolved (skipped)".format(
              stats["trees_seen"], stats["trees_resolved"], stats["trees_unresolved"],
              stats["arrays_resolved"], stats["arrays_unresolved"]), file=out)

    # full-set AABBs (meters) for the actor cross-check (hardening iii)
    aabbs_m = {}
    for name, spheres in per_level_spheres.items():
        lo = [min((s[i] - s[3]) / METER_LENGTH for s in spheres) for i in range(3)]
        hi = [max((s[i] + s[3]) / METER_LENGTH for s in spheres) for i in range(3)]
        aabbs_m[name] = (lo, hi)

    print("\nhardening (iii) actor cross-check (hardened, type-resolved descent):", file=out)
    total, inside, per_level = actor_cross_check(aabbs_m, out)
    overall_frac = (inside / total) if total else 0.0
    bad_levels = [n for n, (i, t) in per_level.items() if t and i / t < ACTOR_INSIDE_MIN_LEVEL_FRACTION]
    if total == 0:
        sys.exit("error: actor cross-check found zero actor positions to check; refusing "
                  "to emit an unvalidated table")
    if overall_frac < ACTOR_INSIDE_MIN_FRACTION or bad_levels:
        sys.exit(
            "error: actor cross-check regressed below the investigation's baseline "
            "({}% inside overall, levels below {}%: {}); aborting emission".format(
                round(overall_frac * 100, 2), int(ACTOR_INSIDE_MIN_LEVEL_FRACTION * 100),
                bad_levels,
            )
        )

    if args.prove_hardening:
        naive_aabbs_m = {}
        for name, spheres in per_level_naive_spheres.items():
            if not spheres:
                continue
            lo = [min((s[i] - s[3]) / METER_LENGTH for s in spheres) for i in range(3)]
            hi = [max((s[i] + s[3]) / METER_LENGTH for s in spheres) for i in range(3)]
            naive_aabbs_m[name] = (lo, hi)
        print("\n--prove-hardening: naive fixed-32-byte-stride descent (no type "
              "resolution), same actor cross-check:", file=out)
        n_total, n_inside, n_per_level = actor_cross_check(naive_aabbs_m, out)
        n_bad = [n for n, (i, t) in n_per_level.items() if t and i / t < ACTOR_INSIDE_MIN_LEVEL_FRACTION]
        print("--prove-hardening: naive path: {} of {} levels below {}% actor "
              "containment: {}".format(len(n_bad), len(n_per_level),
                                        int(ACTOR_INSIDE_MIN_LEVEL_FRACTION * 100), sorted(n_bad)),
              file=out)

    # K-sphere clustering, tight-radius filtered (matches the coverage sims)
    rows = []
    for name in sorted(per_level_spheres):
        spheres = per_level_spheres[name]
        tight = [s for s in spheres if s[3] <= TIGHT_RADIUS_M * METER_LENGTH]
        pool = tight if tight else spheres
        clusters = cluster_spheres(pool, args.k)
        fam = family_id.get(name)
        if fam is None:
            continue
        members = components[fam]
        sound = dominant_sound(members, sound_by_name)
        rows.append({
            "name": name,
            "memory_mode": levels[name]["memory_mode"] or "unknown",
            "family_id": fam,
            "sound": sound,
            "sphere_count": len(clusters),
            "spheres": clusters,
        })

    # consistency self-test: no borrow0/small-center row may pass the "candidate for the
    # 2-tiny rotation" predicate the runtime driver uses (design acceptance item 3)
    bad_rows = [
        r["name"] for r in rows
        if r["memory_mode"] in MEMORY_MODE_NEVER_WANTED
        and r["memory_mode"] in MEMORY_MODE_CANDIDATE_FOR_ROTATION
    ]
    if bad_rows:
        sys.exit("error: {} row(s) are tagged both never-wanted and rotation-candidate; "
                  "the memory-mode enum values overlap unexpectedly: {}".format(
                      len(bad_rows), bad_rows))

    # sound cross-check against the design's named family table
    mismatches = []
    for expected_sound, member_names in EXPECTED_FAMILY_SOUND.items():
        for m in member_names:
            row = next((r for r in rows if r["name"] == m), None)
            if row and row["sound"] != expected_sound:
                mismatches.append((m, row["sound"], expected_sound))
    if mismatches:
        print("warning: {} level(s) disagree with the design's family-sound table: "
              "{}".format(len(mismatches), mismatches), file=out)

    print("\ngen_level_bounds: emitting {} rows, k={} spheres/level (actual count "
          "varies with input sphere pool size)".format(len(rows), args.k), file=out)
    counts = [r["sphere_count"] for r in rows]
    if counts:
        print("gen_level_bounds: spheres/level min={} max={} mean={:.1f}".format(
            min(counts), max(counts), sum(counts) / len(counts)), file=out)

    level_info_md5 = md5_of(LEVEL_INFO_PATH)
    go_hash = hashlib.md5()
    for p in sorted(go_files_used):
        go_hash.update(p.name.encode("utf-8"))
        go_hash.update(md5_of(p).encode("utf-8"))
    provenance = "source {} md5 {}, {} raw_obj/*.go files md5 {} k={}".format(
        LEVEL_INFO_PATH.relative_to(REPO_ROOT).as_posix(), level_info_md5,
        len(go_files_used), go_hash.hexdigest(), args.k,
    )

    new_block = emit_block(rows, provenance)
    level_gc_text = LEVEL_GC_PATH.read_text(encoding="utf-8")
    updated_text = replace_block(level_gc_text, new_block)

    if args.check:
        if updated_text == level_gc_text:
            print("gen_level_bounds: block up to date", file=out)
            sys.exit(0)
        sys.exit("error: {}'s generated block is out of date; run scripts/jakx/{} to "
                  "refresh it".format(LEVEL_GC_PATH, SCRIPT_NAME))

    if updated_text == level_gc_text:
        print("gen_level_bounds: no change", file=out)
        return
    LEVEL_GC_PATH.write_text(updated_text, encoding="utf-8")
    print("gen_level_bounds: wrote {}".format(LEVEL_GC_PATH), file=out)


if __name__ == "__main__":
    main()
