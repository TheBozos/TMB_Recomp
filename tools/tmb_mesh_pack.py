#!/usr/bin/env python3
"""Converts TMB's 3D models (the hierobjects in the disc's NGP files) into the native mesh pack the port's renderer
loads: done once, offline, from the user's own disc files, into the build tree (never committed). While the game runs,
nothing is read from the prebuilt VIF packets: the runtime looks an object up here by its NGP file and its offset in
that file (obj - the file's load address), at the object's draw hook.

  tools/tmb_mesh_pack.py <game dir> <out.pack>
  tools/tmb_mesh_pack.py <game dir> --dump <FILE.NGP> <object offset>     print one object's decoded strips

What it reads:
- the NGP files (LVL/*.NGP, SHELL/*.NGP: a zip entry, "IE" instead of "PK", raw deflate after the local header;
  ACAR/PCAR car files: raw). Each is a memory image linked at EE 0x980000: a pointer is (value - 0x980000) in the file.
  The .PTR file next to it lists the pointer words (its first count, then their byte offsets); a hierobject is a node
  of type 0 (word 0 bits 0-5) whose +4 (a listed pointer) is its own prebuilt packet, at obj + 0x70 + 0x30 * ntex.
- the hierobject: +8 u64 flags (VU1 program choice), +0x10 bounding sphere, +0x50 u16[5] texture numbers (the file's
  own, before the game adds the file's texture base), +0x5a ntex, +0x64 skeleton (0 = rigid).
- its packet: DMA tags (REF: data elsewhere in the file, next tag after it; CNT / RET: data after the tag, RET ends;
  NEXT: data after the tag, next tag at its address), each tag's upper half two VIF codes. The VIF codes:
    UNPACK S-32 @180            pool size
    UNPACK V3-32 @181 n         pool: n positions (x, y, z floats), then its vertex pass: MSCAL 0 (rigid) or MSCAL 2
                                (0x10, the morph pass: position + delta * VU 31, the weight pktAddVu1ObjAsm sends)
    UNPACK V3-32 @373 n         the morph deltas (3 objects, flag 42, MSCAL 2: CITY, MINICITY, FREEWAY): one per pool
                                position, kept as "aux"
    per batch, closed by MSCNT: UNPACK V4-32 @0 the GIF tag (NLOOP, EOP, PRE, PRIM; word 3 the material header:
                                0 = keep the texture, else texture slot (w3 - 1) / 3),
                                UNPACK V4-8 @1 normal bytes (x, y, z) and the pool index (w)
                                  or V4-32 @1 inline points (x, y, z floats, w the size): sprites (STCYCL 4,4, one
                                  point, one colour, four corners) and ribbons (STCYCL 1,3, two points, four colours
                                  and corners); the VU program expands each point into its corners,
                                UNPACK V4-8 @2 colour (RGBA, 0x80 = 1.0), UNPACK V2-16 @3 s, t (signed 4.12).
                                One triangle strip per batch (whole: no ADC skips inside). A batch with only the @0
                                qword (environment-map objects, one per pool) holds the VU program's parameters.
- pool-less objects (strips only): their strips index the pool the object drawn before them left in VU memory. The
  traversal draws a node's children in order and the NGP linker writes nodes after their children, in child order,
  so the object drawn before is the nearest earlier object with a pool in the file: its last pool is this object's.
  The tool checks, through the .PTR pointers, that the nearest node holding both lists the pool object first, and
  that every index is inside that pool.
- skinned objects (+0x64 != 0) use their own packet (no DMA tag: the per-instance chain the header CALLs, at
  (*(inst + 0x18))[obj + 0x68], REFs the bone matrices, unpacked at TOP + 4k for bone k, then this packet, then RETs).
  STCYCL 1,3, then per pool: S-32 @180 its size, V4-32 @181 the positions (w: weight 0 as an integer, / 65536),
  V4-8 @182 the normal bytes and bone 0's offset (w), V4-16 @183 weight 1, bone 1, weight 2, bone 2 (two bones:
  V2-16, weight 1 and bone 1), all unsigned; MSCALF 4 / 6 / 8 its vertex pass (three, two, one bones: 0x20, 0x30,
  0x40); then per strip V4-32 @0 (a texture header in w: an A+D tag, the strip's GIF tag then at @4; else the GIF tag
  itself), V3-32 @4, S-16 @17 the pool offsets (3 per vertex), V4-8 @18 colours, V2-16 @19 s, t, FLUSH, MSCNT. The
  packet comes in sections, each closed by STCYCL 4,4 and NOPs to the quadword (the chain sends the bones again
  before each); the next starts with its pool's S-32 @180. A bone offset is 4 quadwords per bone: bone = offset / 4.

The pack, all little-endian:
  "TMBMESH4", u32 file count, then per file:
    u16 name length, name (upper case, no directory, e.g. "JUNKYARD.NGP"), u32 image size (inflated), u32 image CRC32,
    u32 object count, then per object (sorted by offset):
      u32 offset (obj - 0x980000 in the image), u32 kind (0 rigid, 1 pool-less, 2 sprite, 3 ribbon, 4 skinned),
      u64 flags (obj+8), f32[4] bounding sphere (obj+0x10), u16 ntex, u16[5] texture numbers (obj+0x50),
      u32 pool object (pool-less: the offset of the object whose pool it uses, else 0xffffffff),
      u32 position count, u32 has aux, then per position f32 x, y, z, w (pools: w = 0; inline points: their w),
        then, if has aux, per position f32 x, y, z, 0 (the @373 block; 0 where none),
      u32 pool source count, then per pool the object uses: u32 the image offset of its x, y, z floats (V3-32 @181
        data: the runtime reads the positions there, where a game that moves a mesh's vertices writes them;
        0xffffffff when they are not in one piece), u32 its first position, u32 its position count, u32 its vertex
        pass (the MSCAL after it, as a VU1 address: 0x00 rigid, 0x10 morph), u32 the image offset of its morph deltas'
        x, y, z floats (V3-32 @373 data; 0xffffffff: none, or not in one piece: the aux block has them),
      u32 parameter count, then per parameter u32 the index of the strip it comes before, f32[4],
      u32 skin vertex count (skinned: one per position, else 0), then per skin vertex 20 bytes: u8 bones (the vertex
        pass's: 1, 2, 3), u8 normal x, y, z (n = 1 - b / 128), u8 bone 0, 1, 2 (the matrix index), u8 pad, i32 weight
        0, 1, 2 (/ 65536; one bone: unweighted),
      u32 strip count, then per strip:
        u16 prim (GIF PRIM, 11 bits), u16 gif flags (bit 0 PRE, bit 1 EOP), u32 material header (GIF tag word 3),
        i32 texture slot ((w3 - 1) / 3, -1 = keep the current texture), u32 vertex count, then per vertex 16 bytes:
          u16 position index (into the object's positions), u8 normal x, y, z (raw bytes: n = 1 - b / 128 for the
          directional modes, (b - 128) / 127 for point lights; 0 for inline points), u8 flags (bit 0 inline point,
          bit 1 ADC: skip the triangle this vertex closes, never set by TMB's strips, bit 2 a corner the VU program
          expands from its point: corner = the vertex's index in the strip),
          u8 r, g, b, a, s16 s, t (4.12: st = s / 4096 plus the object's UV scroll), u16 pad.
"""
import os
import struct
import sys
import zlib

LINK = 0x980000
RIGID, POOLLESS, SPRITE, RIBBON, SKINNED = range(5)
KIND_NAMES = ['rigid', 'pool-less', 'sprite', 'ribbon', 'skinned']
UNPACK_FORMATS = {0: 'S-32', 1: 'S-16', 2: 'S-8', 4: 'V2-32', 5: 'V2-16', 6: 'V2-8', 8: 'V3-32', 9: 'V3-16',
                  10: 'V3-8', 12: 'V4-32', 13: 'V4-16', 14: 'V4-8', 15: 'V4-5'}


def u16(data, at):
    return struct.unpack_from('<H', data, at)[0]


def u32(data, at):
    return struct.unpack_from('<I', data, at)[0]


def load_image(path):
    blob = open(path, 'rb').read()
    if blob[:4] == b'IE\x03\x04':
        name_len, extra_len = struct.unpack_from('<HH', blob, 26)
        return zlib.decompressobj(-15).decompress(blob[30 + name_len + extra_len:])
    return blob


def load_pointers(path):
    ptr = os.path.splitext(path)[0] + '.PTR'
    for name in os.listdir(os.path.dirname(path) or '.'):
        if name.upper() == os.path.basename(ptr).upper():
            data = open(os.path.join(os.path.dirname(path), name), 'rb').read()
            return [u32(data, 4 + 4 * i) for i in range(u32(data, 0))]
    raise FileNotFoundError(ptr)


def find_objects(image, pointers):
    """Every hierobject: a type 0 node whose listed +4 pointer is its own packet."""
    found = []
    for slot in pointers:
        obj = slot - 4
        if obj < 0 or obj + 0x70 > len(image) or u32(image, obj) & 0x3f:
            continue
        ntex = u16(image, obj + 0x5a)
        if ntex <= 5 and u32(image, obj + 4) - LINK == obj + 0x70 + 0x30 * ntex:
            found.append(obj)
    return sorted(set(found))


def unpack_size(fmt, count):
    vn, vl = (fmt >> 2) & 3, fmt & 3
    return ((32, 16, 8, 5)[vl] * (vn + 1) * count + 31) // 32 * 4


def vif_codes(data, pos, end):
    """The VIF codes in data[pos:end]: (offset, cmd, num, imm, payload offset)."""
    while pos + 4 <= end:
        code = u32(data, pos)
        cmd, num, imm = (code >> 24) & 0x7f, (code >> 16) & 0xff, code & 0xffff
        at = pos
        pos += 4
        if cmd >= 0x60:
            count = num or 256
            yield at, cmd, count, imm, pos
            pos += unpack_size(cmd & 0xf, count)
        elif cmd in (0x50, 0x51):
            yield at, cmd, imm or 65536, imm, pos
            pos += (imm or 65536) * 16
        elif cmd == 0x4a:
            yield at, cmd, num or 256, imm, pos
            pos += (num or 256) * 8
        elif cmd in (0x30, 0x31):
            yield at, cmd, 0, imm, pos
            pos += 16
        elif cmd == 0x20:
            yield at, cmd, 0, imm, pos
            pos += 4
        else:
            yield at, cmd, num, imm, pos


def packet_segments(image, start):
    """The DMA chain from start: [(VIF bytes, their offset in the image)] (the tag's two VIF codes first)."""
    segments = []
    at = start
    for _ in range(256):
        lo, addr = u32(image, at), u32(image, at + 4)
        qwc, tag = lo & 0xffff, (lo >> 28) & 7
        segments.append((image[at + 8:at + 16], at + 8))
        if tag in (3, 4):  # REF / REFS
            segments.append((image[addr - LINK:addr - LINK + qwc * 16], addr - LINK))
            at += 16
        elif tag in (1, 6, 7):  # CNT / RET / END
            segments.append((image[at + 16:at + 16 + qwc * 16], at + 16))
            if tag != 1:
                return segments
            at += 16 + qwc * 16
        elif tag == 2:  # NEXT
            segments.append((image[at + 16:at + 16 + qwc * 16], at + 16))
            at = addr - LINK
        else:
            raise ValueError(f'DMA tag {tag} at {at:#x}')
    raise ValueError(f'DMA chain from {start:#x} does not end')


def packet_stream(image, obj):
    """The object's VIF stream: what its DMA chain sends, in order (an UNPACK in a tag's upper half carries on into
    the data after it)."""
    return packet_stream_mapped(image, obj)[0]


def packet_stream_mapped(image, obj):
    """The same, with where each piece of it is in the image: (stream, [(stream offset, image offset, length)])."""
    pieces, at = [], 0
    segments = packet_segments(image, obj + 0x70 + 0x30 * u16(image, obj + 0x5a))
    for data, offset in segments:
        pieces.append((at, offset, len(data)))
        at += len(data)
    return b''.join(data for data, _ in segments), pieces


def image_offset(pieces, at, length):
    """The image offset of stream bytes [at, at + length) when one piece holds them all, else None."""
    for start, offset, size in pieces:
        if start <= at and at + length <= start + size:
            return offset + at - start
    return None


class Strip:
    def __init__(self):
        self.tag = None  # the V4-32 @0 qword: four words
        self.slot1 = None  # ('bytes', [(nx, ny, nz, index)]) or ('floats', [(x, y, z, w)])
        self.colours = None
        self.uvs = None


class Pool:
    def __init__(self, positions, source=None):
        self.positions = positions  # [(x, y, z, 0.0)]
        self.source = source  # the image offset of its x, y, z floats (None: not in one piece)
        self.aux = None  # V3-32 @373 (the morph deltas): [(x, y, z, 0.0)], one per position
        self.aux_source = None  # the image offset of its x, y, z floats (None: none, or not in one piece)
        self.vertex_pass = 0  # the MSCAL after it, as a VU1 address (0x00 rigid, 0x10 morph)


class Mesh:
    def __init__(self, offset, kind, flags, sphere, textures):
        self.offset, self.kind, self.flags, self.sphere, self.textures = offset, kind, flags, sphere, textures
        self.pool_from = 0xffffffff
        self.positions = []  # (x, y, z, w)
        self.sources = []  # (image offset of a pool's x, y, z floats or 0xffffffff, first position, count,
        #                     vertex pass, image offset of its morph deltas or 0xffffffff)
        self.aux = []  # per position (x, y, z, 0) from a pool's @373 block (the morph deltas), or zeros
        self.params = []  # (index of the strip it precedes, (f32 x4)): batches with only a V4-32 @0 qword
        self.strips = []  # (prim, gif flags, material header, slot, [(pos index, nx, ny, nz, vflags, rgba, s, t)])
        self.skin = []  # skinned: per position (bones, (nx, ny, nz), (bone 0, 1, 2), (weight 0, 1, 2))


def decode_strips(image, obj, inherited_pool):
    """Decode a rigid / sprite / ribbon object's packet: (its pools [Pool], its batches [(Strip, Pool)], problems)."""
    pools, strips, problems = [], [], []
    current_pool = inherited_pool
    cycle = (4, 4)
    strip = Strip()
    data, pieces = packet_stream_mapped(image, obj)
    for at, cmd, num, imm, payload in vif_codes(data, 0, len(data)):
        if cmd == 0x01:
            cycle = ((imm >> 8) & 0xff, imm & 0xff)
        elif cmd >= 0x60:
            fmt, addr = cmd & 0xf, imm & 0x3ff
            if addr == 180 and fmt == 0:
                continue
            if addr == 181 and fmt == 8:
                current_pool = Pool([struct.unpack_from('<3f', data, payload + 12 * i) + (0.0,) for i in range(num)],
                                    image_offset(pieces, payload, 12 * num))
                pools.append(current_pool)
            elif addr == 373 and fmt == 8 and current_pool is not None and len(current_pool.positions) == num:
                current_pool.aux = [struct.unpack_from('<3f', data, payload + 12 * i) + (0.0,) for i in range(num)]
                current_pool.aux_source = image_offset(pieces, payload, 12 * num)
            elif addr == 0 and fmt == 12 and num == 1:
                strip.tag = struct.unpack_from('<4I', data, payload)
            elif addr == 1 and fmt == 14:
                strip.slot1 = ('bytes', [tuple(data[payload + 4 * i:payload + 4 * i + 4]) for i in range(num)])
            elif addr == 1 and fmt == 12:
                strip.slot1 = ('floats', [struct.unpack_from('<4f', data, payload + 16 * i) for i in range(num)])
            elif addr == 2 and fmt == 14:
                strip.colours = [tuple(data[payload + 4 * i:payload + 4 * i + 4]) for i in range(num)]
            elif addr == 3 and fmt == 5:
                strip.uvs = [struct.unpack_from('<2h', data, payload + 4 * i) for i in range(num)]
            else:
                problems.append(f'UNPACK {UNPACK_FORMATS.get(fmt, "?")} n={num} @{addr} cycle {cycle} '
                                f'at stream +{at:#x}')
        elif cmd == 0x17:  # MSCNT: one batch
            strips.append((strip, current_pool))
            strip = Strip()
        elif cmd in (0x14, 0x15):  # MSCAL / MSCALF: the vertex pass over the pool just sent
            if current_pool is not None and current_pool in pools:
                if imm not in (0, 2):
                    problems.append(f'vertex pass MSCAL {imm:#x} at stream +{at:#x}')
                current_pool.vertex_pass = imm * 8
        elif cmd in (0x00, 0x10, 0x11, 0x13):  # NOP, FLUSHE / FLUSH / FLUSHA
            pass
        else:
            problems.append(f'VIF cmd {cmd:#x} at stream +{at:#x}')
    return pools, strips, problems


SKIN_PASSES = {4: 3, 6: 2, 8: 1}  # MSCALF imm (0x20, 0x30, 0x40) -> bones per vertex


def skinned_packet(image, obj):
    """A skinned object's packet: [(at, cmd, num, imm, payload)] from obj+4. It comes in sections (the chain REFs
    each after the bones again: the bones go to the TOP buffer the section's vertex pass runs on), each closed by
    STCYCL 4,4 (and NOPs to the quadword); the next one starts with its pool's size (S-32 @180)."""
    at = u32(image, obj + 4) - LINK
    codes = []
    while True:
        for code in vif_codes(image, at, min(len(image), at + 0x40000)):
            if code[1] == 0x01 and code[3] == 0x0404:
                at = (code[0] + 4 + 15) & ~15  # STCYCL 4,4, then NOPs to the quadword
                break
            codes.append(code)
        else:
            raise ValueError(f'skinned packet of {obj:#x} does not end')
        if at + 8 > len(image) or u32(image, at) != 0x600100b4:
            return codes


def decode_skinned(image, mesh, obj):
    """Fill a skinned mesh: its pools (positions and skin vertices) and strips. Returns the problems."""
    problems = []
    pool_base, pool_size, pool = None, 0, {}
    strip = {}
    for at, cmd, num, imm, payload in skinned_packet(image, obj):
        addr, usn = imm & 0x3ff, (imm >> 14) & 1
        if cmd == 0x01:
            if imm != 0x0103:
                problems.append(f'STCYCL {imm:#x}')
        elif cmd >= 0x60:
            fmt = cmd & 0xf
            if addr == 180 and fmt == 0:
                pool = {'size': u32(image, payload)}
            elif addr == 181 and fmt == 12:
                pool['pos'] = [struct.unpack_from('<3fi', image, payload + 16 * i) for i in range(num)]
            elif addr == 182 and fmt == 14 and usn:
                pool['nb'] = [tuple(image[payload + 4 * i:payload + 4 * i + 4]) for i in range(num)]
            elif addr == 183 and fmt == 13 and usn:
                pool['wb'] = [struct.unpack_from('<4H', image, payload + 8 * i) for i in range(num)]
            elif addr == 183 and fmt == 5 and usn:
                pool['wb'] = [struct.unpack_from('<2H', image, payload + 4 * i) + (0, 0) for i in range(num)]
            elif addr == 0 and fmt == 12 and num == 1:
                strip['tag0'] = struct.unpack_from('<4I', image, payload)
            elif addr == 4 and fmt == 8 and num == 1:
                strip['tag4'] = struct.unpack_from('<3I', image, payload)
            elif addr == 17 and fmt == 1 and usn:
                strip['offsets'] = [u16(image, payload + 2 * i) for i in range(num)]
            elif addr == 18 and fmt == 14 and usn:
                strip['colours'] = [tuple(image[payload + 4 * i:payload + 4 * i + 4]) for i in range(num)]
            elif addr == 19 and fmt == 5 and not usn:
                strip['uvs'] = [struct.unpack_from('<2h', image, payload + 4 * i) for i in range(num)]
            else:
                problems.append(f'UNPACK {UNPACK_FORMATS.get(fmt, "?")} n={num} @{addr} usn {usn}')
        elif cmd == 0x15:  # MSCALF: the vertex pass over the pool
            bones = SKIN_PASSES.get(imm)
            n = pool.get('size', -1)
            if bones is None or not all(len(pool.get(k, ())) == n for k in ('pos', 'nb', 'wb')):
                problems.append(f'pool: MSCALF {imm:#x}, size {n}, {[len(pool.get(k, ())) for k in ("pos", "nb", "wb")]}')
                pool_base = None
                continue
            pool_base, pool_size = len(mesh.positions), n
            for (x, y, z, w0), (nx, ny, nz, b0), (w1, b1, w2, b2) in zip(pool['pos'], pool['nb'], pool['wb']):
                offsets = (b0, b1, b2)[:bones]
                if any(o % 4 for o in offsets):
                    problems.append(f'bone offset {offsets} not a matrix')
                mesh.positions.append((x, y, z, 0.0))
                mesh.aux.append((0.0, 0.0, 0.0, 0.0))
                ids = [o // 4 for o in offsets] + [0] * (3 - bones)
                weights = (w0, w1, w2) if bones == 3 else (w0, w1, 0) if bones == 2 else (0, 0, 0)
                mesh.skin.append((bones, (nx, ny, nz), tuple(ids), weights))
            pool = {}
        elif cmd == 0x17:  # MSCNT: one strip
            needed = ('tag0', 'tag4', 'offsets', 'colours', 'uvs') if strip.get('tag0', (0, 0, 0, 0))[3] else                 ('tag0', 'offsets', 'colours', 'uvs')
            if pool_base is None or any(k not in strip for k in needed):
                problems.append('skinned strip without its pool / tag / offsets / colours / uvs')
                strip = {}
                continue
            header = strip['tag0'][3]
            tag = strip['tag4'] if header else strip['tag0'][:3]
            n = len(strip['offsets'])
            if (tag[0] & 0x7fff) != n or len(strip['colours']) != n or len(strip['uvs']) != n or tag[2] != 0x412:
                problems.append(f'skinned strip: NLOOP {tag[0] & 0x7fff}, {n} offsets, regs {tag[2]:#x}')
                strip = {}
                continue
            vertices = []
            for offset, rgba, (s_, t_) in zip(strip['offsets'], strip['colours'], strip['uvs']):
                offset &= 0x7fff
                if offset % 3 or offset // 3 >= pool_size:
                    problems.append(f'skinned strip: pool offset {offset} (pool {pool_size})')
                    break
                vertices.append((pool_base + offset // 3, 0, 0, 0, 0, rgba, s_, t_))
            else:
                prim = (tag[1] >> 15) & 0x7ff
                gif_flags = ((tag[1] >> 14) & 1) | (((tag[0] >> 15) & 1) << 1)
                mesh.strips.append((prim, gif_flags, header, (header - 1) // 3 if header else -1, vertices))
            strip = {}
        elif cmd in (0x00, 0x10, 0x11, 0x13):
            pass
        else:
            problems.append(f'VIF cmd {cmd:#x}')
    return problems


def build_mesh(image, obj, kind, inherited_pool):
    flags = struct.unpack_from('<Q', image, obj + 8)[0]
    sphere = struct.unpack_from('<4f', image, obj + 0x10)
    textures = (u16(image, obj + 0x5a),) + struct.unpack_from('<5H', image, obj + 0x50)
    mesh = Mesh(obj, kind, flags, sphere, textures)
    if kind == SKINNED:
        return mesh, decode_skinned(image, mesh, obj)
    pools, strips, problems = decode_strips(image, obj, inherited_pool)
    pool_base = {}  # the pools this object uses, in order of first use, then inline positions
    for strip, pool in strips:
        if strip.tag is not None and strip.slot1 is None and strip.colours is None and strip.uvs is None:
            mesh.params.append((len(mesh.strips), struct.unpack('<4f', struct.pack('<4I', *strip.tag))))
            continue
        if strip.tag is None or strip.slot1 is None or strip.colours is None or strip.uvs is None:
            problems.append('strip without its tag / slot 1 / colours / uvs')
            continue
        vertices = []
        if strip.slot1[0] == 'floats':
            # sprites (one point, one colour, four corners) and ribbons (two points, four colours and corners): the
            # VU program expands each point into corners; vertex i uses point i * points / corners
            count, points, colours = len(strip.uvs), strip.slot1[1], strip.colours
            if count % len(points) or count % len(colours):
                problems.append(f'inline strip: {len(points)} points, {len(colours)} colours, {count} corners')
                continue
            base = len(mesh.positions)
            mesh.positions.extend(points)
            mesh.aux.extend([(0.0, 0.0, 0.0, 0.0)] * len(points))
            for i in range(count):
                s, t = strip.uvs[i]
                vflags = 1 | (4 if count != len(points) else 0)
                vertices.append((base + i * len(points) // count, 0, 0, 0, vflags,
                                 colours[i * len(colours) // count], s, t))
        else:
            n = strip.tag[0] & 0x7fff
            if not (len(strip.slot1[1]) == len(strip.colours) == len(strip.uvs) == n):
                problems.append(f'strip counts {n} {len(strip.slot1[1])} {len(strip.colours)} {len(strip.uvs)}')
                continue
            if pool is None:
                problems.append('pool index without a pool')
                continue
            if id(pool) not in pool_base:
                pool_base[id(pool)] = len(mesh.positions)
                mesh.sources.append((0xffffffff if pool.source is None else pool.source, len(mesh.positions),
                                     len(pool.positions), pool.vertex_pass,
                                     0xffffffff if pool.aux_source is None else pool.aux_source))
                if pool.vertex_pass == 0x10 and pool.aux is None:
                    problems.append('morph pass without its deltas')
                mesh.positions.extend(pool.positions)
                mesh.aux.extend(pool.aux or [(0.0, 0.0, 0.0, 0.0)] * len(pool.positions))
            bad = [w for _, _, _, w in strip.slot1[1] if w >= len(pool.positions)]
            if bad:
                problems.append(f'pool index {max(bad)} >= pool size {len(pool.positions)}')
                continue
            for i in range(n):
                nx, ny, nz, index = strip.slot1[1][i]
                s, t = strip.uvs[i]
                vertices.append((pool_base[id(pool)] + index, nx, ny, nz, 0, strip.colours[i], s, t))
        word1 = strip.tag[1]
        prim = (word1 >> 15) & 0x7ff
        gif_flags = ((word1 >> 14) & 1) | (((strip.tag[0] >> 15) & 1) << 1)
        w3 = strip.tag[3]
        mesh.strips.append((prim, gif_flags, w3, (w3 - 1) // 3 if w3 else -1, vertices))
    if len(mesh.positions) > 0xffff:
        problems.append(f'{len(mesh.positions)} positions')
    return mesh, problems


def object_kind(image, obj):
    if u32(image, obj + 0x64):
        return SKINNED
    flags = struct.unpack_from('<Q', image, obj + 8)[0]
    if not flags & (7 << 33):
        mode = flags & (3 << 37)
        if mode == 1 << 37:
            return RIBBON
        if mode == 1 << 38:
            return SPRITE
    return RIGID


def has_pool(image, obj):
    data = packet_stream(image, obj)
    for _, cmd, _, imm, _ in vif_codes(data, 0, len(data)):
        if cmd >= 0x60 and cmd & 0xf == 8 and imm & 0x3ff == 181:
            return True
    return False


def parents(image, pointers, starts):
    """Child pointer -> its node: for each listed pointer to a node, (the node holding the pointer, the pointer's
    offset in it), the node being the nearest pointer target at or before it."""
    import bisect
    refs = {}
    for slot in pointers:
        target = u32(image, slot) - LINK
        i = bisect.bisect_right(starts, slot) - 1
        if i >= 0:
            refs.setdefault(target, []).append((starts[i], slot - starts[i]))
    return refs


def ancestors(node, refs, depth=24):
    """node -> {ancestor: (child slot offset path)} for every chain of parents (first chain wins)."""
    out, frontier = {}, [(node, ())]
    for _ in range(depth):
        nxt = []
        for n, path in frontier:
            for parent, off in refs.get(n, ()):
                if parent not in out and parent != n:
                    out[parent] = path + (off,)
                    nxt.append((parent, path + (off,)))
        frontier = nxt
    return out


def convert_file(path):
    image = load_image(path)
    pointers = load_pointers(path)
    objects = find_objects(image, pointers)
    starts = sorted({u32(image, s) - LINK for s in pointers if 0 <= u32(image, s) - LINK < len(image)})
    refs = None
    meshes, problems = [], []
    last_pool, last_pool_obj = None, None
    stats = {'poolless_checked': 0, 'poolless_unchecked': []}
    for obj in objects:
        kind = object_kind(image, obj)
        inherited = None
        if kind == RIGID and not has_pool(image, obj):
            kind = POOLLESS
            inherited = last_pool
            if last_pool_obj is not None:
                if refs is None:
                    refs = parents(image, pointers, starts)
                # a common ancestor of the two whose child slot leading to the pool object comes first
                a, b = ancestors(last_pool_obj, refs), ancestors(obj, refs)
                common = sorted((len(a[n]) + len(b[n]), n) for n in a if n in b)
                if common and a[common[0][1]][-1] < b[common[0][1]][-1]:
                    stats['poolless_checked'] += 1
                else:
                    stats['poolless_unchecked'].append(obj)
        mesh, issues = build_mesh(image, obj, kind, inherited)
        if kind == POOLLESS and last_pool_obj is not None:
            mesh.pool_from = last_pool_obj
        problems.extend(f'{os.path.basename(path)} {obj:#x}: {p}' for p in issues)
        meshes.append(mesh)
        if kind in (RIGID,):
            pools, _, _ = decode_strips(image, obj, None)
            if pools:
                last_pool, last_pool_obj = pools[-1], obj
    return image, meshes, problems, stats


def write_pack(out_path, files):
    with open(out_path, 'wb') as out:
        out.write(b'TMBMESH4')
        out.write(struct.pack('<I', len(files)))
        for name, image, meshes in files:
            encoded = name.encode()
            out.write(struct.pack('<H', len(encoded)) + encoded)
            out.write(struct.pack('<III', len(image), zlib.crc32(image) & 0xffffffff, len(meshes)))
            for m in meshes:
                out.write(struct.pack('<IIQ4fH5HI', m.offset, m.kind, m.flags, *m.sphere, *m.textures, m.pool_from))
                has_aux = any(a != (0.0, 0.0, 0.0, 0.0) for a in m.aux)
                out.write(struct.pack('<II', len(m.positions), int(has_aux)))
                out.write(b''.join(struct.pack('<4f', *p) for p in m.positions))
                if has_aux:
                    out.write(b''.join(struct.pack('<4f', *a) for a in m.aux))
                out.write(struct.pack('<I', len(m.sources)))
                out.write(b''.join(struct.pack('<IIIII', *src) for src in m.sources))
                out.write(struct.pack('<I', len(m.params)))
                out.write(b''.join(struct.pack('<I4f', i, *q) for i, q in m.params))
                out.write(struct.pack('<I', len(m.skin)))
                out.write(b''.join(struct.pack('<8B3i', bones, *normal, *ids, 0, *weights) for bones, normal, ids, weights in m.skin))
                out.write(struct.pack('<I', len(m.strips)))
                for prim, gif_flags, w3, slot, vertices in m.strips:
                    out.write(struct.pack('<HHIiI', prim, gif_flags, w3, slot, len(vertices)))
                    out.write(b''.join(struct.pack('<H4B4B2hH', v[0], v[1], v[2], v[3], v[4], *v[5], v[6], v[7], 0)
                                       for v in vertices))


def ngp_files(game):
    found = []
    for root, _, names in os.walk(game):
        for name in sorted(names):
            if name.upper().endswith('.NGP'):
                found.append((name.upper(), os.path.join(root, name)))
    return sorted(found)


def dump(game, name, offset):
    for fname, path in ngp_files(game):
        if fname == os.path.basename(name.replace(chr(92), '/')).upper():
            break
    else:
        sys.exit(f'{name}: not found under {game}')
    image, meshes, problems, _ = convert_file(path)
    for m in meshes:
        if m.offset == offset:
            break
    else:
        sys.exit(f'{name}: no object at {offset:#x}')
    print(f'{fname} object {m.offset:#x} (EE {m.offset + LINK:#x}): {KIND_NAMES[m.kind]}, flags {m.flags:#018x}, '
          f'sphere ({m.sphere[0]:.3f}, {m.sphere[1]:.3f}, {m.sphere[2]:.3f}, r {m.sphere[3]:.3f}), '
          f'ntex {m.textures[0]}, textures {list(m.textures[1:1 + m.textures[0]])}')
    if m.pool_from != 0xffffffff:
        print(f'  pool from object {m.pool_from:#x}')
    for source, first, count, vertex_pass, deltas in m.sources:
        print(f'  pool: positions {first}-{first + count - 1} at {source:#x}, vertex pass {vertex_pass:#x}' +
              (f', morph deltas at {deltas:#x}' if deltas != 0xffffffff else ''))
    for i, (bones, normal, ids, weights) in enumerate(m.skin[:400]):
        print(f'  skin [{i:4}] {bones} bones {ids[:bones]} weights {[w / 65536 for w in weights[:bones]]} n {normal}')
    print(f'  {len(m.positions)} positions, {len(m.strips)} strips, '
          f'{sum(len(s[4]) for s in m.strips)} vertices')
    params = dict(m.params)
    for i, (prim, gif_flags, w3, slot, vertices) in enumerate(m.strips):
        if i in params:
            print('  parameters (%.4f, %.4f, %.4f, %.4f)' % params[i])
        print(f'  strip {i}: prim {prim:#x} gif flags {gif_flags} material {w3} slot {slot}, {len(vertices)} vertices')
        for pos, nx, ny, nz, vflags, rgba, s, t in vertices:
            x, y, z, w = m.positions[pos]
            aux = m.aux[pos] if any(m.aux[pos]) else None
            print(f'    [{pos:4}] ({x:10.3f} {y:10.3f} {z:10.3f} {w:7.3f})  n {nx:3} {ny:3} {nz:3}  f {vflags}  '
                  f'rgba {rgba[0]:3} {rgba[1]:3} {rgba[2]:3} {rgba[3]:3}  st {s / 4096:8.4f} {t / 4096:8.4f}' +
                  (f'  aux ({aux[0]:.6g} {aux[1]:.6g} {aux[2]:.6g})' if aux else ''))
    for p in problems:
        if f' {offset:#x}:' in p:
            print('  problem:', p)


def main():
    args = sys.argv[1:]
    if len(args) == 4 and args[1] == '--dump':
        dump(args[0], args[2], int(args[3], 0))
        return
    if len(args) != 2:
        sys.exit(__doc__.split('\n\n')[1])
    game, out_path = args
    files, problems, counts = [], [], [0] * 5
    checked, unchecked = 0, []
    for name, path in ngp_files(game):
        image, meshes, issues, stats = convert_file(path)
        files.append((name, image, meshes))
        problems.extend(issues)
        checked += stats['poolless_checked']
        unchecked.extend((name, o) for o in stats['poolless_unchecked'])
        for m in meshes:
            counts[m.kind] += 1
    write_pack(out_path, files)
    print(f'{out_path}: {len(files)} files, {sum(counts)} objects (' +
          ', '.join(f'{n} {k}' for k, n in zip(KIND_NAMES, counts)) + ')')
    print(f'pool-less objects: {checked} under the same node as their pool object, after it; {len(unchecked)} not')
    for name, o in unchecked[:10]:
        print(f'  not checked: {name} {o:#x}')
    for p in problems[:40]:
        print('problem:', p)
    if problems:
        print(f'{len(problems)} problems')


if __name__ == '__main__':
    main()
