#!/usr/bin/env python3
"""Renders one of tools/xbox-og/scenes/*.txt on the host with the software 3D backend (the reference picture).

  python3 -I tools/xbox-og/host_scene.py PLAYER SCENE.txt OUT.bmp [--scale N]

The scene file is what goes into the console's args.txt (docs/XBOX_OG.md "Testing without a controller"); here the same player
arguments run headless: `ote-player --headless --no-audio --3d software --no-settings --no-saving ...`, temporary settings and
saves, no window, no sound. The console draws the 3D at the screen's pixels (576x324 for the 480x270 canvas on a 640x480 screen),
the host at the game's size unless --scale N makes its "display" N times bigger (--3d-scale N, docs/RENDER3D.md "Resolution").
"""
import os
import subprocess
import sys
import tempfile


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    player, scene, out = argv[1:4]
    scale = argv[argv.index('--scale') + 1] if '--scale' in argv else None
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    words = []
    for line in open(scene):
        line = line.strip()
        if not line or line.startswith('#') or '=' in line.split(' ')[0] and not line.startswith('-'):
            continue  # comments and NAME=value environment words
        words.append(line)
    # drop the console's screenshot path: the host writes its own
    if '--screenshot' in words:
        i = words.index('--screenshot')
        del words[i:i + 2]
    with tempfile.TemporaryDirectory() as tmp:
        cmd = [player, '--headless', '--no-audio', '--3d', 'software', '--no-settings', '--no-saving', '--saves', tmp,
               '--games', os.path.join(repo, 'games'), '--assets', os.path.join(repo, 'assets'), '--screenshot', out] + words
        if scale:
            cmd += ['--3d-scale', scale]
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        if res.returncode != 0 or not os.path.exists(out):
            sys.stderr.write(res.stdout[-2000:] + res.stderr[-2000:])
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
