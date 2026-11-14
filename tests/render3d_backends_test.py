#!/usr/bin/env python3
"""Checks that the hardware 3D backend draws what the software backend (the reference) draws.

  python3 tests/render3d_backends_test.py ./build/engine/player/ote-player .

Runs the player windowless with --3d software and --3d hardware on a few scenes and compares
the screenshots. A handful of pixels on triangle edges may differ (the two fill edges by
slightly different rules); anything more fails. On a machine without a usable graphics
driver the hardware backend falls back to software: the scenes are then reported as skipped.
"""
import os
import struct
import subprocess
import sys
import tempfile

SCENES = [
    ('test scene, flat', ['--test-3d', '--frames', '30']),
    ('test scene, perspective, turned', ['--test-3d', '--input', 'wait:10,confirm,wait:10,left,wait:30']),
    ('demo-srpg battle with models', ['--game', 'demo-srpg', '--map', 'bridge', '--style', 'isometric-3d',
                                      '--autoplay', '--seed', '3', '--frames', '320']),
    ('demo-srpg, a unit falling (blended)', ['--game', 'demo-srpg', '--map', 'bridge', '--style', 'isometric-3d',
                                             '--autoplay', '--seed', '3', '--frames', '995']),
    ('demo-srpg, full 3D, camera turned and tilted', ['--game', 'demo-srpg', '--map', 'bridge', '--style', 'full-3d', '--input',
                                                      'wait:150,turnright,turnright,tiltdown,tiltdown,zoomin,wait:40']),
    ('demo-economy, full 3D battle (pictures)', ['--game', 'demo-economy', '--map', 'twin_rivers', '--style', 'full-3d',
                                                 '--autoplay', '--seed', '2', '--frames', '500']),
    ('demo-economy', ['--game', 'demo-economy', '--map', 'twin_rivers', '--style', 'isometric-3d',
                      '--autoplay', '--seed', '2', '--frames', '500']),
]
MAX_DIFFERENT = 0.002   # share of pixels with a channel more than TOLERANCE apart
TOLERANCE = 8


def read_bmp(path):
    """(width, height, rows of bytes top first, bytes per pixel) of an uncompressed 24/32-bit BMP."""
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    rows = [data[offset + r * stride: offset + r * stride + width * bpp] for r in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows, bpp


def shot(player, repo, mode, args, out):
    cmd = [player, '--headless', '--3d', mode, '--no-settings', '--no-saving', '--fixed-dt', '0.0166',
           '--games', os.path.join(repo, 'games'), '--assets', os.path.join(repo, 'assets'),
           '--screenshot', out] + args
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    return res.returncode, res.stdout + res.stderr


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    player, repo = sys.argv[1], sys.argv[2]
    failed = skipped = passed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, args in SCENES:
            soft, hard = os.path.join(tmp, 's.bmp'), os.path.join(tmp, 'h.bmp')
            code_s, _ = shot(player, repo, 'software', args, soft)
            code_h, log = shot(player, repo, 'hardware', args, hard)
            if code_s != 0 or code_h != 0:
                print('FAIL %s: the player ended with an error\n%s' % (name, log[-600:]))
                failed += 1
                continue
            if '3D drawing: OpenGL' not in log:
                why = [l for l in log.splitlines() if 'no hardware backend' in l]
                print('SKIP %s: no hardware backend here %s' % (name, why[:1]))
                skipped += 1
                continue
            w1, h1, a, bpp1 = read_bmp(soft)
            w2, h2, b, bpp2 = read_bmp(hard)
            if (w1, h1, bpp1) != (w2, h2, bpp2):
                print('FAIL %s: pictures differ in size' % name)
                failed += 1
                continue
            different = 0
            for ra, rb in zip(a, b):
                if ra == rb:
                    continue
                for i in range(0, len(ra), bpp1):
                    if any(abs(ra[i + k] - rb[i + k]) > TOLERANCE for k in range(3)):
                        different += 1
            share = different / float(w1 * h1)
            ok = share <= MAX_DIFFERENT
            print('%s %s: %d of %d pixels differ' % ('PASS' if ok else 'FAIL', name, different, w1 * h1))
            passed += ok
            failed += not ok
    print('\n%d passed, %d failed, %d skipped' % (passed, failed, skipped))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
