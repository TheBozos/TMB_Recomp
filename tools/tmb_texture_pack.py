#!/usr/bin/env python3
"""Converts TMB's texture files from the user's own disc into the native texture pack the port's renderer loads
(runtime/tmb_textures.cpp): done once, offline, into the build tree (never committed). While the game runs, nothing is
read from its texture records: the runtime only counts the records TMB's texture code sets up and loads, and looks
the textures up here by file and position.

  tools/tmb_texture_pack.py <game dir> <ELF> <out.pack>

What it reads:
- .TEX files (a zip entry, "IE" instead of "PK": raw deflate after the local header): a header (+0 class count,
  +4 its size in quadwords), then records chained by their size (+0, in words). texmLoadTexture skips a record with
  bit 7 of its format word (+0xc) set and sets up the others in order (texmSetupTexture): those are the pack's
  records, in that order. Pixels at +0x80 (a plain raster in the record's format).
- .RTX files (raw): records from the offset its +4 gives in quadwords, each loaded in order by texmLoadResTexture:
  the T8H / T4HL / T4HH textures (pixels at +0x80) and the palettes (entries at +0x10).
- the font (fontDmaFontData's sources in the ELF: a 128 x 128 T4 raster, a 16-entry CT32 palette).

The pack is the fork's generic texture pack (PS2Recomp/tools/native_pack/texture.py, PS2XTEX1; the runtime's loader
and decoders are ps2x::native::packs, PS2Recomp/docs/native_packs.md): per file its name (upper case, no directory)
and its records in the order the game sets them up (the runtime's key scheme counts them so), each psm, width, height
and its texels unpacked; the font as the file "FONT" (its raster, then its palette).
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'PS2Recomp', 'tools'))
from native_pack import texture as native_texture  # noqa: E402
from native_pack.texture import bits, unpack_texels  # noqa: E402,F401
from native_pack.ps2 import u32, elf_bytes  # noqa: E402

CT32, CT24, CT16, CT16S, T8, T4, T8H, T4HL, T4HH = (native_texture.PSMCT32, native_texture.PSMCT24, native_texture.PSMCT16,
                                                    native_texture.PSMCT16S, native_texture.PSMT8, native_texture.PSMT4,
                                                    native_texture.PSMT8H, native_texture.PSMT4HL, native_texture.PSMT4HH)
FONT = {'SCUS_971.01': (0x22aba0, 0x22ab60), 'SCES_503.60': (0x22be20, 0x22bde0)}


def record_size(data, rec, texels_at):
    """A record's size in bytes: its +0 (in words), or for a file's last record, whose +0 the game fills in when it
    links the files (texmRelocate / texmResRelocate), its header and raster."""
    size = u32(data, rec) * 4
    if size:
        return size
    fmt = u32(data, rec + 0xc)
    width, height = 1 << ((fmt >> 8) & 0xf), 1 << ((fmt >> 12) & 0xf)
    return texels_at + (width * height * bits(fmt & 0x3f) + 7) // 8


def record(data, rec, texels_at):
    fmt = u32(data, rec + 0xc)
    psm = fmt & 0x3f
    width, height = 1 << ((fmt >> 8) & 0xf), 1 << ((fmt >> 12) & 0xf)
    size = record_size(data, rec, texels_at)
    return native_texture.record(psm, width, height, data[rec + texels_at:rec + size])  # a short palette: zeros after it


def walk(data, first, texels_at):
    """The records from first to the end of the file (texels_at: where a record's texels start, for the last one's
    size)."""
    rec = first
    while rec + 0x10 <= len(data):
        if u32(data, rec) == 0 and u32(data, rec + 0xc) == 0:
            return  # the file's padding
        yield rec
        rec += (record_size(data, rec, texels_at(rec)) + 15) // 16 * 16


def tex_records(path):
    blob = open(path, 'rb').read()
    if blob[:4] == b'IE\x03\x04':
        name_len, extra_len = struct.unpack_from('<HH', blob, 26)
        data = zlib.decompressobj(-15).decompress(blob[30 + name_len + extra_len:])
    else:
        data = blob
    out = []
    for rec in walk(data, u32(data, 4) * 16, lambda rec: 0x80):
        if (u32(data, rec + 0xc) >> 7) & 1:
            continue  # texmLoadTexture skips it
        out.append(record(data, rec, 0x80))
    return out


def rtx_records(path):
    data = open(path, 'rb').read()
    out = []
    def texels_at(rec):
        return 0x80 if u32(data, rec + 0xc) & 0x3f in (T8H, T4HL, T4HH) else 0x10
    for rec in walk(data, u32(data, 4) * 16, texels_at):
        psm = u32(data, rec + 0xc) & 0x3f
        out.append(record(data, rec, 0x80 if psm in (T8H, T4HL, T4HH) else 0x10))
    return out


def main():
    game, elf, out_path = sys.argv[1:4]
    files = []
    for root, _, names in os.walk(game):
        for name in sorted(names):
            ext = os.path.splitext(name)[1].upper()
            if ext == '.TEX':
                files.append((name.upper(), tex_records(os.path.join(root, name))))
            elif ext == '.RTX':
                files.append((name.upper(), rtx_records(os.path.join(root, name))))
    pixels_at, palette_at = FONT[os.path.basename(elf).upper()]
    font = unpack_texels(T4, elf_bytes(elf, pixels_at, 128 * 64), 128, 128)
    files.append(('FONT', [(T4, 128, 128, font), (CT32, 8, 2, elf_bytes(elf, palette_at, 64))]))
    pack = native_texture.TexturePackWriter()
    for name, records in files:
        pack.add_file(name, records)
    pack.write(out_path)
    print(f'{out_path}: {len(files)} files, {pack.record_count()} records')


if __name__ == '__main__':
    main()
