#!/usr/bin/env python3
"""Compares a screenshot made on the Xbox (in xemu) with the same frame drawn by the host's software 3D backend.

  python3 -I tools/xbox-og/compare_shot.py XBOX.bmp HOST.bmp [OUT.png] [--rect X,Y,W,H] [--mask X,Y,W,H ...]

XBOX.bmp is the player's own `--screenshot` on the console: the whole 640x480 (or 1280x720) output, margins included.
HOST.bmp is the same scene from `ote-player --headless --3d software --screenshot` at the game's own size (480x270).
--rect is where the game's canvas lies in the Xbox picture (the log line `screen fit: 576x324 canvas on 640x480 at 32,78`);
the default is the 640x480 mode at 90% safe area. The host picture is scaled up to that rectangle (the console draws the 3D
at the real size, the host at the logical size), so differences at edges and in fine detail are expected: the numbers
are for finding what is wrong (a colour, a missing thing), not for exact matching. --mask excludes rectangles in the
logical canvas (the HUD text, the overlay).

Prints: mean absolute difference per channel (0..255), share of pixels differing by more than 24 in some channel, and the
same after blurring both pictures a little (which removes the edge and scaling noise). OUT.png: Xbox | host | difference.
"""
import sys

from PIL import Image, ImageChops, ImageFilter


def parse_rect(text):
    return tuple(int(v) for v in text.split(','))


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('--')]
    rect = (32, 78, 576, 324)
    masks = []
    i = 1
    while i < len(argv):
        if argv[i] == '--rect':
            rect = parse_rect(argv[i + 1])
            i += 2
        elif argv[i] == '--mask':
            masks.append(parse_rect(argv[i + 1]))
            i += 2
        else:
            i += 1
    args = [a for a in argv[1:] if not a.startswith('--') and ',' not in a]
    if len(args) < 2:
        print(__doc__)
        return 2
    xbox = Image.open(args[0]).convert('RGB')
    host = Image.open(args[1]).convert('RGB')
    x, y, w, h = rect
    xbox = xbox.crop((x, y, x + w, y + h))
    host = host.resize((w, h), Image.BILINEAR)
    # The masks are in logical pixels of the host picture's size; the host picture was 480x270 by default.
    hw, hh = Image.open(args[1]).size
    xbox_m, host_m = xbox.copy(), host.copy()
    for mx, my, mw, mh in masks:
        box = (int(mx * w / hw), int(my * h / hh), int((mx + mw) * w / hw), int((my + mh) * h / hh))
        grey = Image.new('RGB', (box[2] - box[0], box[3] - box[1]), (128, 128, 128))
        xbox_m.paste(grey, box[:2])
        host_m.paste(grey, box[:2])

    def stats(a, b):
        diff = ImageChops.difference(a, b)
        px = list(diff.getdata())
        n = len(px)
        mean = [sum(p[c] for p in px) / n for c in range(3)]
        bad = sum(1 for p in px if max(p) > 24) / n
        return mean, bad

    mean, bad = stats(xbox_m, host_m)
    bmean, bbad = stats(xbox_m.filter(ImageFilter.GaussianBlur(2)), host_m.filter(ImageFilter.GaussianBlur(2)))
    print('mean abs diff R,G,B: %.1f %.1f %.1f   pixels off by >24: %.2f%%' % (mean[0], mean[1], mean[2], bad * 100))
    print('after a 2 px blur:   %.1f %.1f %.1f   pixels off by >24: %.2f%%' % (bmean[0], bmean[1], bmean[2], bbad * 100))
    if len(args) > 2:
        diff = ImageChops.difference(xbox_m, host_m).point(lambda v: min(255, v * 3))
        sheet = Image.new('RGB', (w * 3, h))
        sheet.paste(xbox, (0, 0))
        sheet.paste(host, (w, 0))
        sheet.paste(diff, (w * 2, 0))
        sheet.save(args[2])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
