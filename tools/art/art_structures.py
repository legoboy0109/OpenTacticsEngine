"""Structure pictures of the starter art set (docs/GRAPHICS.md "Structures").

A structure picture is one sheet cell drawn across the structure's whole
footprint: its width is `width` squares and its bottom edge sits on the
footprint's bottom edge, so a picture taller than the footprint sticks out
above it (towers, chimneys, flags). Cells here are T * width pixels wide and
T * height + EXTRA * max(width, height) pixels tall. Everything outside the
buildings is transparent, so the terrain underneath shows around them.

Original art, drawn from shapes; no input files.
"""
import art_tiles as tl
from pix import Pix, hexc as hexc_

T = 24
OUT = tl.DARKLINE
STONE = tl.STONE


def extra(w, h):
    """Pixels the picture may stick out above the footprint."""
    return 4 * max(w, h)


def cell_size(w, h):
    return T * w, T * h + extra(w, h)


def plot(p, x0, y0, x1, y1, base, light, dark):
    """A paved foundation the building stands on: outlined, lit top edge."""
    p.rect(x0, y0, x1, y1, OUT)
    p.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, base)
    p.rect(x0 + 1, y0 + 1, x1 - 1, y0 + 1, light)
    p.rect(x0 + 1, y1 - 1, x1 - 1, y1 - 1, dark)


def canvas(w, h):
    cw, ch = cell_size(w, h)
    return Pix(cw, ch)


def city():
    """24 x 28: two houses in front of a small clock tower."""
    p = canvas(1, 1)
    plot(p, 0, 22, 23, 27, 0xC9C3B0, 0xE0DBC8, 0xA8A392)
    # clock tower behind the houses: sticks out above the square
    tower = Pix(T, 28)
    tower.rect(9, 3, 14, 22, 0xD6CFBA)
    tower.rect(9, 3, 14, 3, 0xEDE8D6)
    tower.rect(13, 4, 14, 22, 0xB4AC96)
    for x in range(9, 15):  # pointed roof
        top = 0 if x in (11, 12) else 1
        tower.rect(x, top, x, 3, 0x6F819A)
    tower.rect(11, 0, 12, 0, 0x56667C)
    tower.rect(10, 6, 13, 9, 0xF3EFE0)       # clock face
    tower.rect(11, 7, 12, 8, 0x35465E)
    tower.set(11, 7, 0xF3EFE0)
    tower.rect(11, 12, 12, 15, 0x35465E)     # belfry slit
    tower.outline(OUT)
    p.paste(tower, 0, 0)
    body = tl.city_body()
    p.paste(body, 0, 0)
    return p


def factory():
    """24 x 28: sawtooth hall with two tall chimneys that stick out."""
    p = canvas(1, 1)
    plot(p, 0, 22, 23, 27, 0xA9A9AF, 0xC4C4CA, 0x8C8C94)
    body = tl.factory_body()
    # stretch the chimneys upward by redrawing them taller (rows 0..3 are above the square)
    p.paste(body, 0, 0)
    for x0 in (4, 13):
        p.rect(x0, 0, x0 + 2, 3, 0xA85A44)
        p.rect(x0 + 2, 0, x0 + 2, 3, 0x80402F)
        p.rect(x0, 0, x0 + 2, 0, 0x4A4A52)
        p.set(x0 - 1, 0, OUT); p.set(x0 + 3, 0, OUT)
        p.rect(x0 - 1, 1, x0 - 1, 3, OUT); p.rect(x0 + 3, 1, x0 + 3, 3, OUT)
    return p


def fort():
    """24 x 28: stone keep with battlements and a flag."""
    p = canvas(1, 1)
    plot(p, 0, 22, 23, 27, 0xB4AE9C, 0xD0CAB8, 0x8E8878)
    p.paste(tl.fort_body(), 0, 0)
    # taller flag pole so the banner flies above the square
    p.line(12, 0, 12, 1, 0x4A4038)
    return p


def hq_small():
    """24 x 28 headquarters for games that keep it one square."""
    p = canvas(1, 1)
    plot(p, 0, 22, 23, 27, 0xD9CFA6, 0xEFE7C8, 0xB9B38F)
    p.paste(tl.hq_body(), 0, 0)
    return p


def hq():
    """48 x 56 (2 x 2): town-hall style headquarters. A gold-roofed tower and flag rise
    above the footprint; two wings, a door with steps and a star emblem."""
    p = canvas(2, 2)
    wall, dark, light = 0xEADFB6, 0xC3B88E, 0xF6F0D4
    gold, gold_d, window, door = 0xD8B03A, 0xA88524, 0x35465E, 0x3A2F28
    plot(p, 0, 42, 47, 55, 0xD9CFA6, 0xEFE7C8, 0xB9B38F)

    def block(x0, y0, x1, y1):
        p.rect(x0, y0, x1, y1, wall)
        p.rect(x0, y0, x1, y0, light)
        p.rect(x1 - 2, y0 + 1, x1, y1, dark)

    b = Pix(48, 56)
    p_ = p
    p = b
    # wings
    block(2, 28, 17, 46)
    block(30, 28, 45, 46)
    p.rect(1, 26, 18, 28, gold)
    p.rect(29, 26, 46, 28, gold)
    p.rect(1, 28, 18, 28, gold_d)
    p.rect(29, 28, 46, 28, gold_d)
    for wx in (5, 11):
        for wy in (32, 39):
            p.rect(wx, wy, wx + 2, wy + 3, window)
            p.set(wx, wy, 0x6A86A8)
            p.rect(wx + 30, wy, wx + 32, wy + 3, window)
            p.set(wx + 30, wy, 0x6A86A8)
    # central hall
    block(14, 22, 33, 46)
    p.rect(13, 20, 34, 22, gold)
    p.rect(13, 22, 34, 22, gold_d)
    # tower
    block(18, 8, 29, 22)
    for i in range(6):  # roof
        p.rect(17 + i, 6 - i + 2, 30 - i, 6 - i + 2, gold)
    p.rect(17, 8, 30, 8, gold_d)
    p.rect(22, 2, 25, 3, gold)
    p.line(23, 0, 23, 2, 0x4A4038)  # flag pole
    p.rect(24, 0, 29, 3, 0xF0E8D0)  # flag
    p.rect(29, 1, 29, 2, 0xD8CDAE)
    p.rect(21, 12, 22, 15, window)  # tower windows
    p.rect(25, 12, 26, 15, window)
    # emblem: a small gold star on the hall
    for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
        p.set(23 + dx, 26 + dy, gold)
    # door with steps
    p.rect(20, 34, 27, 46, door)
    p.rect(21, 33, 26, 33, door)
    p.rect(23, 34, 23, 46, 0x5A4636)
    p.rect(18, 46, 29, 46, 0xB9B38F)
    # hall windows
    for wx in (16, 29):
        p.rect(wx, 28, wx + 2, 31, window)
        p.rect(wx, 36, wx + 2, 39, window)
        p.set(wx, 28, 0x6A86A8); p.set(wx, 36, 0x6A86A8)
    p.outline(OUT)
    p_.paste(b, 0, 0)
    return p_


def town():
    """24 x 28: the city on a golden plot with a red pennant on the clock tower (a city's first upgrade)."""
    p = city()
    swap = {hexc_(0xC9C3B0): 0xD9C58A, hexc_(0xE0DBC8): 0xEEDCA6, hexc_(0xA8A392): 0xB5A268}
    for y in range(22, 28):  # the paved plot only: the houses stand above row 22 and on it at the foot
        for x in range(24):
            if p.get(x, y) in swap:
                p.set(x, y, swap[p.get(x, y)])
    p.line(12, 0, 12, 2, 0x4A4038)      # pennant pole on the tower roof
    p.rect(13, 0, 16, 1, 0xC03A2E)
    return p


def metropolis():
    """24 x 28: three glass towers of different heights on a blue-grey plot (a town's upgrade)."""
    p = canvas(1, 1)
    plot(p, 0, 22, 23, 27, 0xB9C6D4, 0xD6E0EA, 0x8E9DB0)
    glass, light, dark, win = 0x7FA4C4, 0xB9D3E8, 0x5A7FA0, 0xDCEBF6
    for x0, x1, top in ((1, 7, 10), (9, 14, 1), (16, 22, 6)):
        t = Pix(x1 - x0 + 1, 22 - top)
        t.rect(0, 0, t.w - 1, t.h - 1, glass)
        t.rect(0, 0, t.w - 1, 0, light)
        t.rect(t.w - 2, 1, t.w - 1, t.h - 1, dark)
        for wy in range(2, t.h - 2, 3):
            for wx in range(1, t.w - 2, 2):
                t.rect(wx, wy, wx, wy, win)
        t.outline(OUT)
        p.paste(t, x0 - 1 + 1, top - 1 + 1)
    p.rect(11, 0, 12, 0, 0xE0B030)       # gilded cap on the tallest tower
    p.rect(11, 6, 12, 7, 0xE0B030)
    return p


# Structure id -> {(width, height): picture function}. The starter set can draw these.
PICTURES = {
    'city': {(1, 1): city},
    'town': {(1, 1): town},
    'metropolis': {(1, 1): metropolis},
    'factory': {(1, 1): factory},
    'fort': {(1, 1): fort},
    'hq': {(2, 2): hq, (1, 1): hq_small},
}
