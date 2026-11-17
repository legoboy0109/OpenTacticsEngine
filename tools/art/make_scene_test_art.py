#!/usr/bin/env python3
"""Draws the small original pixel art of the story scene tests (tests/fixtures/scene-pres/art/).

  python3 tools/art/make_scene_test_art.py tests/fixtures/scene-pres/art

Everything is made here from plain shapes (no downloaded or copied pictures): two whole-image backgrounds
(a forest dusk and a hall), a sheet of two cells (a hall and a cellar, to test backgrounds that are one cell of a sheet)
and a sheet of three 48 x 48 faces. Run it again to get the same files byte for byte.
"""
import os
import sys

from PIL import Image, ImageDraw

sys.dont_write_bytecode = True


def forest(w=240, h=135):
    im = Image.new('RGB', (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):  # a dusk sky, top to bottom
        t = y / h
        d.line([(0, y), (w, y)], fill=(int(40 + 150 * t), int(30 + 90 * t), int(90 + 40 * t)))
    d.ellipse([w - 70, 16, w - 40, 46], fill=(250, 226, 160))  # the moon
    d.rectangle([0, int(h * 0.72), w, h], fill=(22, 48, 34))  # the ground
    for i in range(9):  # pines of two sizes
        x = 12 + i * 27
        base = int(h * 0.74) + (i % 2) * 6
        height = 50 + (i * 7) % 24
        d.rectangle([x - 2, base - 6, x + 2, base + 4], fill=(60, 36, 24))
        for k in range(4):
            top = base - height + k * 13
            d.polygon([(x, top), (x - 12 + k, top + 22), (x + 12 - k, top + 22)], fill=(14 + k * 5, 70 - k * 6, 44))
    return im


def hall(w, h):
    im = Image.new('RGB', (w, h), (58, 50, 70))
    d = ImageDraw.Draw(im)
    d.rectangle([0, int(h * 0.7), w, h], fill=(96, 80, 64))  # the floor
    for i in range(1, 5):  # columns
        x = i * w // 5
        d.rectangle([x - 5, 6, x + 5, int(h * 0.7)], fill=(150, 142, 160))
        d.rectangle([x - 8, 4, x + 8, 9], fill=(180, 172, 190))
    d.rectangle([w // 2 - 14, int(h * 0.45), w // 2 + 14, int(h * 0.7)], fill=(120, 50, 50))  # a banner
    return im


def cellar(w, h):
    im = Image.new('RGB', (w, h), (30, 34, 40))
    d = ImageDraw.Draw(im)
    for y in range(0, h, 12):  # stone courses
        for x in range((y // 12 % 2) * 10, w, 20):
            d.rectangle([x, y, x + 18, y + 10], outline=(52, 58, 66))
    d.ellipse([w // 2 - 6, 14, w // 2 + 6, 26], fill=(250, 190, 80))  # a lamp
    return im


def face(skin, hair, cloak, eye, brow=None, hairstyle=0):
    im = Image.new('RGBA', (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([6, 36, 41, 47], fill=cloak)  # shoulders
    d.rectangle([20, 30, 27, 38], fill=skin)  # neck
    d.rectangle([12, 8, 35, 33], fill=skin)  # face
    d.rectangle([12, 6, 35, 14 + hairstyle * 2], fill=hair)  # hair
    if hairstyle == 1:
        d.rectangle([9, 8, 13, 26], fill=hair)
        d.rectangle([34, 8, 38, 26], fill=hair)
    d.rectangle([17, 19, 21, 22], fill=(255, 255, 255))
    d.rectangle([26, 19, 30, 22], fill=(255, 255, 255))
    d.rectangle([19, 20, 21, 22], fill=eye)
    d.rectangle([28, 20, 30, 22], fill=eye)
    d.rectangle([16, 16, 22, 17], fill=brow or hair)
    d.rectangle([25, 16, 31, 17], fill=brow or hair)
    d.rectangle([20, 28, 27, 29], fill=(150, 70, 70))  # a mouth
    return im


def main(out):
    os.makedirs(out, exist_ok=True)
    forest().save(os.path.join(out, 'bg_forest.png'))
    sheet = Image.new('RGB', (240, 68))  # two cells of 120 x 68: a hall and a cellar
    sheet.paste(hall(120, 68), (0, 0))
    sheet.paste(cellar(120, 68), (120, 0))
    sheet.save(os.path.join(out, 'bg_places.png'))
    faces = Image.new('RGBA', (144, 48), (0, 0, 0, 0))
    faces.paste(face((232, 190, 150), (90, 60, 40), (50, 80, 170), (40, 60, 120)), (0, 0))  # eirik
    faces.paste(face((240, 205, 170), (190, 120, 50), (50, 140, 90), (30, 100, 60), hairstyle=1), (48, 0))  # lyra
    faces.paste(face((200, 160, 130), (30, 30, 36), (150, 50, 50), (120, 30, 30)), (96, 0))  # gorm
    faces.save(os.path.join(out, 'faces.png'))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'art')
