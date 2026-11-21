#!/usr/bin/env python3
"""Flicker detector for emulator screenshots of the Xbox player (docs/XBOX_OG.md "Pacing").

usage: flicker_check.py 'shots/PREFIX-14s-*.png' [--mark R,G,B] [--min-ratio 0.6] [--framemark MODE_WIDTH]

The screenshots are bursts grabbed from the emulator's real output (run-xemu.sh with SHOT_BURST=N, about 30 a second). A frame
that was shown while the chip was still drawing it (or before the 2D layer drawn last was in it) lacks something a complete picture
always has. The check looks for the colour of the battle HUD's side panel (Story Tactics / Hex Wars / Wars at 4:3: 20,23,38, about 11 percent
of the picture): a complete battle picture has it, a picture without the 2D layer drawn last (the HUD) has none. Use a different --mark colour for another game or screen.
Also reports shots that differ a lot from BOTH neighbours while those two agree (a one-frame glitch in a still scene).
--framemark W (the run had OTE_XBOX_FRAMEMARK=1 and the video mode is W pixels wide): reads the frame number the player paints in the
corner of every frame (nxdk_present.cpp drawFrameMark) and checks the order: the numbers of consecutive shots must never go
backwards, and a step between shots (taken about every second blank) must stay small (<= 4); a repeat is allowed only as often as
--max-repeat percent.
Exit status 1 when any shot is torn.
"""
import argparse, glob, sys
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("pattern")
ap.add_argument("--mark", default="20,23,38")
ap.add_argument("--framemark", type=int, default=0)
ap.add_argument("--max-repeat", type=float, default=10.0)
ap.add_argument("--tol", type=int, default=4)
ap.add_argument("--min-ratio", type=float, default=0.6, help="a shot with less than this fraction of the median marker share is torn")
a = ap.parse_args()
files = sorted(glob.glob(a.pattern))
if not files:
    sys.exit("no screenshots match " + a.pattern)
mark = np.array([int(v) for v in a.mark.split(",")], dtype=np.int16)
imgs = [np.asarray(Image.open(f).convert("RGB")).astype(np.int16) for f in files]
share = [100.0 * float((np.abs(im - mark).max(axis=2) <= a.tol).mean()) for im in imgs]
median = float(np.median(share))
torn = [i for i, s in enumerate(share) if s < a.min_ratio * median]
glitch = []
for i in range(1, len(imgs) - 1):
    dn = lambda x, y: float((np.abs(imgs[x] - imgs[y]).max(axis=2) > 40).mean()) * 100
    if dn(i - 1, i + 1) < 0.5 and dn(i, i - 1) > 2 and dn(i, i + 1) > 2:
        glitch.append(i)
print("%d shots; marker share (percent of picture): min %.2f, median %.2f" % (len(files), min(share), median))
print("torn (marker missing): %d %s" % (len(torn), [files[i].rsplit("/", 1)[-1] for i in torn][:20]))
print("one-shot glitches (differs from both neighbours that agree): %d %s" % (len(glitch), [files[i].rsplit("/", 1)[-1] for i in glitch][:20]))

if a.framemark:
    k = imgs[0].shape[1] / a.framemark
    nums = []
    for im in imgs:
        n = 0
        for i in range(16):
            x, y = int((6 + 10 * i) * k), int(6 * k)
            n = (n << 1) | (1 if im[y, x].min() > 128 else 0)
        nums.append(n)
    unmarked = [i for i, n in enumerate(nums) if n == 0]  # no number painted: the frame was shown before it was complete
    seq = [n for n in nums if n]
    steps = [(seq[i + 1] - seq[i]) & 0xFFFF for i in range(len(seq) - 1)]
    back = [i for i, d in enumerate(steps) if d > 0x8000 or d == 0 and False]
    big = [i for i, d in enumerate(steps) if 4 < d <= 0x8000]
    rep = sum(1 for d in steps if d == 0)
    print("frame numbers %d..%d; shown without its number (incomplete): %d; steps between shots: min %d, max %d; backwards %d, "
          "jumps over 4: %d, repeats %d (%.1f%%)" % (seq[0], seq[-1], len(unmarked), min(d if d < 0x8000 else d - 0x10000 for d in steps),
          max(d if d < 0x8000 else d - 0x10000 for d in steps), len(back), len(big), rep, 100.0 * rep / len(steps)))
    if unmarked or back or big or 100.0 * rep / len(steps) > a.max_repeat:
        torn = torn or [-1]
sys.exit(1 if torn or glitch else 0)
