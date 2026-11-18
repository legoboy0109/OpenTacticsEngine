#!/usr/bin/env python3
"""Draws the small original pixel art of the stage tests (tests/fixtures/stage-pres/art/).

  python3 tools/art/make_stage_test_art.py tests/fixtures/stage-pres/art

faces.png: five 48 x 48 faces in a row, three rows: 0 = the plain face, 1 = mouth open (a "talk" frame), 2 = eyes shut (a "blink"
frame). Every face looks to the LEFT (the nose sticks out on the left side), so the stage's turning them towards the middle of the
screen can be seen. bg_forest.png: the forest of the scene tests. Everything is plain shapes, no downloaded pictures; run it again
to get the same files byte for byte.
"""
import os
import sys

from PIL import Image, ImageDraw

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_scene_test_art import forest  # noqa: E402

PEOPLE = [  # skin, hair, cloak, eye, hairstyle
    ((232, 190, 150), (90, 60, 40), (50, 80, 170), (40, 60, 120), 0),   # eirik
    ((240, 205, 170), (190, 120, 50), (50, 140, 90), (30, 100, 60), 1),  # lyra
    ((200, 160, 130), (30, 30, 36), (150, 50, 50), (120, 30, 30), 0),    # gorm
    ((225, 180, 150), (200, 200, 210), (120, 80, 150), (90, 50, 120), 1),  # mira
    ((180, 140, 110), (120, 40, 30), (200, 160, 50), (110, 80, 20), 0),  # tobin
]


def face(skin, hair, cloak, eye, hairstyle, mouth_open=False, eyes_shut=False):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([6, 36, 41, 47], fill=cloak)       # shoulders
    d.rectangle([20, 30, 27, 38], fill=skin)       # neck
    d.rectangle([12, 8, 35, 33], fill=skin)        # face
    d.rectangle([12, 6, 35, 14 + hairstyle * 2], fill=hair)  # hair
    if hairstyle == 1:
        d.rectangle([9, 8, 13, 26], fill=hair)
        d.rectangle([34, 8, 38, 26], fill=hair)
    d.rectangle([7, 21, 12, 27], fill=skin)        # the nose: the face looks to the left
    d.rectangle([7, 27, 12, 27], fill=(170, 110, 90))
    if eyes_shut:
        d.rectangle([16, 21, 22, 21], fill=(60, 40, 40))
        d.rectangle([25, 21, 31, 21], fill=(60, 40, 40))
    else:
        d.rectangle([16, 19, 21, 22], fill=(255, 255, 255))
        d.rectangle([25, 19, 30, 22], fill=(255, 255, 255))
        d.rectangle([16, 20, 18, 22], fill=eye)    # the pupils look left too
        d.rectangle([25, 20, 27, 22], fill=eye)
    d.rectangle([16, 16, 22, 17], fill=hair)
    d.rectangle([25, 16, 31, 17], fill=hair)
    if mouth_open:
        d.rectangle([18, 28, 26, 31], fill=(120, 40, 50))
    else:
        d.rectangle([18, 29, 26, 29], fill=(150, 70, 70))
    return im


def main(out):
    os.makedirs(out, exist_ok=True)
    forest().save(os.path.join(out, 'bg_forest.png'))
    sheet = Image.new('RGBA', (48 * len(PEOPLE), 48 * 3), (0, 0, 0, 0))
    for i, p in enumerate(PEOPLE):
        sheet.paste(face(*p), (48 * i, 0))
        sheet.paste(face(*p, mouth_open=True), (48 * i, 48))
        sheet.paste(face(*p, eyes_shut=True), (48 * i, 96))
    sheet.save(os.path.join(out, 'faces.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'art')
