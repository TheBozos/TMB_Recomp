#!/usr/bin/env python3
"""Makes a texture replacement pack from a texture dump with Real-ESRGAN, on the player's own PC from their own disc's
textures (docs/TEXTURE_UPSCALING.md). Nothing it reads or writes is part of the repository or a release.

  tools/tmb_texture_upscale.py <dump folder> <pack folder> --esrgan <realesrgan-ncnn-vulkan.exe>
                               [--models <its models folder>] [--model realesrgan-x4plus] [--min-size 32] [--detail 1.0]

- The dump: TMB_TEXTURE_DUMP=1 (runtime/tmb_texture_dump.cpp): <identity>.png and index.tsv, alpha as the GS's (128 is
  opaque, up to 255). The pack: <identity>.png 4 times as large each way, the same alpha convention, which the engine
  reads from <user folder>/texture_pack (or video.ini's texture_pack, PS2X_TEXTURE_PACK).
- Colour: Real-ESRGAN (xinntao/Real-ESRGAN, BSD-3-Clause; its ncnn-vulkan executable and models, fetched by the player)
  over the texture with a 16-texel border taken from the opposite edges, so a repeating texture stays seamless. Its model
  removes fine grain (gravel, rust become flat), so the original's high frequencies (bicubic minus its blur) are added
  back, scaled by --detail. Texels with no alpha get their neighbours' colour first, so no dark fringe bleeds in.
- Alpha: bicubic over the same border. Two-valued alpha (cut-outs) is cut again at the midpoint, so edges stay sharp but
  follow the smoothed shape; otherwise held within the texture's own range, and values near opaque (128) snapped to it.
- Flat areas (a texel equal to its 8 neighbours) are kept exactly: a black background stays black.
- The menus and the font (--skip) are left to the engine's texture upscaling: the model garbles small lettering.
- Textures smaller than --min-size on either side are left out (the model invents little from 8x8 or 16x16; the
  engine's own texture upscaling covers them when it is on).

Needs Python 3 with numpy and Pillow.
"""
import argparse
import csv
import os
import shutil
import subprocess
import sys
import time

try:
    import numpy as np
    from PIL import Image, ImageFilter
except ImportError:
    sys.exit('tmb_texture_upscale.py needs numpy and Pillow (python -m pip install numpy pillow)')

PAD = 16
SCALE = 4


def wrap_pad(a, pad):
    return np.pad(a, [(pad, pad), (pad, pad)] + [(0, 0)] * (a.ndim - 2), mode='wrap')


def bleed(rgb, alpha):
    """Colour of transparent texels from their visible neighbours (a few dilation passes), wrap-around."""
    rgb = rgb.astype(np.float32)
    known = alpha > 0
    if known.all() or not known.any():
        return rgb
    for _ in range(16):
        if known.all():
            break
        acc = np.zeros_like(rgb)
        cnt = np.zeros(known.shape, np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            k = np.roll(np.roll(known, dy, 0), dx, 1)
            acc += np.roll(np.roll(rgb, dy, 0), dx, 1) * k[..., None]
            cnt += k
        grow = ~known & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][:, None]
        known = known | grow
    return rgb


def upscale_alpha(alpha):
    h, w = alpha.shape
    values = np.unique(alpha)
    if len(values) == 1:
        return np.full((h * SCALE, w * SCALE), values[0], np.uint8)
    def up(a):
        padded = Image.fromarray(wrap_pad(a, PAD).astype(np.uint8), 'L')
        big = np.asarray(padded.resize((padded.width * SCALE, padded.height * SCALE), Image.BICUBIC), np.float32)
        return big[PAD * SCALE:(PAD + h) * SCALE, PAD * SCALE:(PAD + w) * SCALE]

    big = up(alpha)
    if len(values) == 2:
        lo, hi = float(values[0]), float(values[1])
        return np.where(big >= (lo + hi) / 2, hi, lo).astype(np.uint8)
    # Where the texture is see-through stays exactly so: the shape of its visible texels, smoothed and cut at half
    # (bicubic alone spreads small alphas into transparent areas, which a blended draw shows as a dark box).
    shape = up((alpha > 0).astype(np.uint8) * 255) >= 128 if values[0] == 0 else np.ones(big.shape, bool)
    visible = values[values > 0]
    big = np.clip(big, float(visible.min()), float(visible.max()))
    if len(values) <= 4:
        # A few levels (cut-outs, 0 / 127 / 255): each texel snapped to the nearest one.
        big = visible[np.abs(big[..., None] - visible[None, None, :].astype(np.float32)).argmin(axis=-1)].astype(np.float32)
    elif 128 in values:
        big[np.abs(big - 128) <= 2] = 128
    return np.where(shape, np.rint(big), 0).astype(np.uint8)


def flat(im):
    """Texels equal to their 8 neighbours (colour and alpha, wrap-around), at the output size: kept exactly, so a
    black or flat background stays as it was (the model and the grain would tint it, and a blended draw shows that)."""
    a = im.astype(np.int16)
    same = np.ones(a.shape[:2], bool)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            same &= (np.abs(np.roll(np.roll(a, dy, 0), dx, 1) - a) <= 1).all(axis=-1)
    return np.kron(same, np.ones((SCALE, SCALE), bool))


def detail(rgb, sigma):
    """The original's high frequencies at the output size: bicubic minus its blur, over the wrapped border."""
    h, w = rgb.shape[:2]
    padded = Image.fromarray(wrap_pad(np.clip(rgb, 0, 255).astype(np.uint8), PAD), 'RGB')
    big = padded.resize((padded.width * SCALE, padded.height * SCALE), Image.BICUBIC)
    hf = np.asarray(big, np.float32) - np.asarray(big.filter(ImageFilter.GaussianBlur(sigma)), np.float32)
    return hf[PAD * SCALE:(PAD + h) * SCALE, PAD * SCALE:(PAD + w) * SCALE]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('dump')
    p.add_argument('pack')
    p.add_argument('--esrgan', required=True, help='realesrgan-ncnn-vulkan executable')
    p.add_argument('--models', help="its models folder (default: 'models' beside it)")
    p.add_argument('--model', default='realesrgan-x4plus')
    p.add_argument('--min-size', type=int, default=32)
    p.add_argument('--detail', type=float, default=1.0, help='how much of the original grain is added back (0: none)')
    p.add_argument('--work', help='scratch folder (default: <pack>/.work, removed after)')
    p.add_argument('--skip', default='SHELL1,FONT',
                   help="files whose textures are left to the engine's upscaling (comma-separated; default the menus and "
                        "the font: the model garbles small lettering)")
    args = p.parse_args()

    models = args.models or os.path.join(os.path.dirname(os.path.abspath(args.esrgan)), 'models')
    work = args.work or os.path.join(args.pack, '.work')
    src_dir, out_dir = os.path.join(work, 'in'), os.path.join(work, 'out')
    for d in (args.pack, src_dir, out_dir):
        os.makedirs(d, exist_ok=True)

    skip = {name.strip().upper().split('.')[0] for name in args.skip.split(',') if name.strip()}
    with open(os.path.join(args.dump, 'index.tsv'), encoding='utf-8') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    todo = []
    for row in rows:
        ident, w, h = row['identity'], int(row['width']), int(row['height'])
        if min(w, h) < args.min_size or os.path.exists(os.path.join(args.pack, ident + '.png')):
            continue
        if row['texels'].split('#')[0].split('.')[0] in skip:
            continue
        im = np.asarray(Image.open(os.path.join(args.dump, ident + '.png')).convert('RGBA'))
        rgb = bleed(im[..., :3], im[..., 3])
        Image.fromarray(wrap_pad(np.clip(rgb, 0, 255).astype(np.uint8), PAD), 'RGB').save(os.path.join(src_dir, ident + '.png'))
        todo.append((ident, im, rgb))
    print('%d textures to upscale (%d in the dump, %d below %d texels or already done)' % (len(todo), len(rows), len(rows) - len(todo),
                                                                                           args.min_size))
    if not todo:
        return
    start = time.time()
    subprocess.run([args.esrgan, '-i', src_dir, '-o', out_dir, '-n', args.model, '-m', models, '-s', str(SCALE), '-f', 'png'],
                   check=True, stdout=subprocess.DEVNULL)
    print('Real-ESRGAN: %.1f s' % (time.time() - start))
    for ident, im, rgb in todo:
        h, w = im.shape[:2]
        out = np.asarray(Image.open(os.path.join(out_dir, ident + '.png')).convert('RGB'), np.float32)
        out = out[PAD * SCALE:(PAD + h) * SCALE, PAD * SCALE:(PAD + w) * SCALE]
        if args.detail:
            out = out + args.detail * detail(rgb, 3.0)
        a = upscale_alpha(im[..., 3])
        pixels = np.dstack([np.clip(np.rint(out), 0, 255).astype(np.uint8), a])
        keep = flat(im)
        pixels[keep] = np.kron(im, np.ones((SCALE, SCALE, 1), np.uint8))[keep]
        Image.fromarray(pixels, 'RGBA').save(os.path.join(args.pack, ident + '.png'), compress_level=6)
    if not args.work:
        shutil.rmtree(work, ignore_errors=True)
    print('%d replacements written to %s' % (len(todo), args.pack))


if __name__ == '__main__':
    main()
