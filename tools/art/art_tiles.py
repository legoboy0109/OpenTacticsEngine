"""Terrain tiles (24 x 24) of the starter art set. Original, drawn from shapes."""
from pix import Pix, rng

T = 24

# Palette (limited on purpose).
GRASS = (0x88B95F, 0x70A44E, 0xA6D07A)  # base, dark, light
OUT_GREEN = 0x24502B
STONE = (0xB9B3A2, 0x8E8878, 0xD8D3C4)  # base, dark, light
DARKLINE = 0x2A2A33


def grass(seed, tufts=7, flowers=0, base=GRASS):
    p = Pix(T, T, base[0])
    r = rng('grass', seed)
    for _ in range(tufts):
        x, y = r.randrange(1, T - 1), r.randrange(2, T - 1)
        p.set(x, y, base[1])
        p.set(x, y - 1, base[1])
        p.set(x + 1, y, base[1]) if r.random() < 0.5 else p.set(x - 1, y, base[1])
    for _ in range(5):
        p.set(r.randrange(T), r.randrange(T), base[2])
    for _ in range(flowers):
        x, y = r.randrange(2, T - 2), r.randrange(2, T - 2)
        p.set(x, y, 0xF4F0E0)
        p.set(x, y + 1, 0xE8C040) if r.random() < 0.5 else None
    return p


def tree(p, cx, cy):
    p.rect(cx - 1, cy + 3, cx, cy + 6, 0x6B4526)
    p.rect(cx, cy + 3, cx, cy + 6, 0x56391F)
    p.circle(cx, cy, 5, 0x2F6B34)
    p.circle(cx - 1.5, cy - 1.5, 3, 0x45883F)
    p.set(cx - 3, cy - 2, 0x6AAE58)
    p.set(cx - 2, cy - 3, 0x6AAE58)
    for dx, dy in ((2, 3), (3, 1), (1, 4), (3, 2)):
        p.set(cx + dx, cy + dy, 0x245A2B)


def forest():
    p = grass(11, tufts=4)
    for cx, cy in ((6, 7), (17, 7), (11, 14)):
        tree(p, cx, cy)
    # dark ring around the canopies is drawn by re-outlining: keep it light, only on the lower edge
    return p


def mountain():
    p = grass(23, tufts=3)
    # two peaks drawn row by row: (apex x, apex y, half width at y=21)
    def peak(ax, ay, hw, light, dark, snow_rows):
        for y in range(ay, 22):
            half = (y - ay) * hw / (21 - ay)
            for x in range(int(ax - half), int(ax + half) + 1):
                c = light if x <= ax else dark
                if y - ay < snow_rows:
                    # ragged snow line
                    c = 0xF2F5F8 if (x <= ax) else 0xC9D2DD
                    if y - ay == snow_rows - 1 and (x + y) % 2 == 0:
                        c = light if x <= ax else dark
                p.set(x, y, c)
        # a few cracks
        p.line(ax - 1, ay + snow_rows + 2, ax - 3, ay + snow_rows + 6, dark)
        p.line(ax + 2, ay + snow_rows + 4, ax + 3, ay + snow_rows + 8, 0x5A4E40)
    peak(7, 9, 7, 0xA39785, 0x7A6E5C, 3)
    peak(14, 2, 10, 0xA89C8A, 0x6F6351, 5)
    # ground shadow line
    p.rect(0, 21, T - 1, 21, 0x6F6351)
    p.rect(0, 22, T - 1, 23, GRASS[1])
    return p


def finish(built):
    """Outlines a building drawn on a transparent canvas (structures stand on terrain, no ground here)."""
    built.outline(DARKLINE)
    return built


def fort_body():
    p = Pix(T, T)
    # keep
    p.rect(4, 9, 19, 21, STONE[0])
    p.rect(4, 9, 19, 9, STONE[2])
    p.rect(17, 10, 19, 21, STONE[1])
    for y in (12, 15, 18):  # brick rows
        p.rect(4, y, 19, y, STONE[1])
    for x in (7, 11, 15):
        p.set(x, 10, STONE[1]); p.set(x + 1, 13, STONE[1]); p.set(x - 2, 16, STONE[1]); p.set(x, 19, STONE[1])
    # battlements
    for x in range(4, 20, 4):
        p.rect(x, 6, x + 1, 8, STONE[0])
        p.rect(x + 1, 6, x + 1, 8, STONE[2])
    p.rect(4, 8, 19, 8, STONE[1])
    # gate
    p.rect(10, 15, 13, 21, 0x3A2F28)
    p.rect(11, 14, 12, 14, 0x3A2F28)
    p.line(11, 16, 11, 21, 0x5A4636)
    # flag
    p.line(12, 1, 12, 6, 0x4A4038)
    p.rect(13, 1, 16, 3, 0xF0E8D0)
    p.set(16, 2, 0xD8CDAE)
    return finish(p)


def city_body():
    p = Pix(T, T)
    def house(x0, y0, x1, wall, roof, roof_dark):
        p.rect(x0, y0, x1, 21, wall)
        p.rect(x1, y0, x1, 21, roof_dark)
        # roof
        for i in range(0, 4):
            p.rect(x0 - 1 + i, y0 - 4 + i, x1 + 1 - i, y0 - 4 + i, roof)
        p.rect(x0 - 1, y0, x1 + 1, y0, roof_dark)
        for wx in range(x0 + 2, x1 - 1, 4):
            for wy in range(y0 + 3, 20, 5):
                p.rect(wx, wy, wx + 1, wy + 1, 0x35465E)
                p.set(wx, wy, 0x6A86A8)
    house(2, 8, 9, 0xE0D6AE, 0xB05A42, 0x8A432F)
    house(12, 11, 21, 0xC9CFD6, 0x6F819A, 0x56667C)
    p.rect(6, 17, 7, 21, 0x4A3A30)
    p.rect(16, 17, 17, 21, 0x4A3A30)
    return finish(p)


def factory_body():
    p = Pix(T, T)
    body, dark, light = 0x8D949C, 0x6A7078, 0xB4BAC2
    p.rect(2, 11, 21, 21, body)
    p.rect(2, 11, 21, 11, light)
    p.rect(19, 12, 21, 21, dark)
    # sawtooth roof
    for x0 in (2, 9, 16):
        for i in range(5):
            p.rect(x0, 6 + i, x0 + i, 6 + i, body)
        p.line(x0, 6, x0 + 4, 10, light)
    # chimneys
    for x0 in (4, 13):
        p.rect(x0, 2, x0 + 2, 10, 0xA85A44)
        p.rect(x0 + 2, 2, x0 + 2, 10, 0x80402F)
        p.rect(x0, 1, x0 + 2, 1, 0x4A4A52)
    # smoke
    p.rect(5, 0, 6, 0, 0xD8DADE)
    p.rect(15, 0, 16, 0, 0xD8DADE)
    # door and windows
    p.rect(8, 15, 14, 21, 0x3A3C44)
    for x in range(8, 15, 2):
        p.rect(x, 15, x, 21, 0x50535C)
    p.rect(8, 14, 14, 14, 0xE0B030)
    for wx in (3, 17):
        p.rect(wx, 14, wx + 1, 15, 0x35465E)
    return finish(p)


def hq_body():
    p = Pix(T, T)
    wall, dark, light, gold = 0xEADFB6, 0xC3B88E, 0xF6F0D4, 0xD8B03A
    p.rect(3, 12, 20, 21, wall)
    p.rect(3, 12, 20, 12, light)
    p.rect(18, 13, 20, 21, dark)
    p.rect(8, 6, 15, 21, wall)
    p.rect(8, 6, 15, 6, light)
    p.rect(13, 7, 15, 21, dark)
    # gold roof over the tower and the wings
    p.rect(7, 4, 16, 6, gold)
    p.rect(9, 3, 14, 3, gold)
    p.rect(3, 10, 6, 11, gold)
    p.rect(17, 10, 20, 11, gold)
    p.rect(7, 6, 16, 6, 0xA88524)
    # flag
    p.line(11, 0, 11, 3, 0x4A4038)
    p.rect(12, 0, 14, 1, 0xF0E8D0)
    # door + windows + star
    p.rect(10, 16, 13, 21, 0x3A2F28)
    p.rect(11, 15, 12, 15, 0x3A2F28)
    for wx in (4, 18, 9):
        wy = 14 if wx != 9 else 8
        p.rect(wx, wy, wx + 1, wy + 2, 0x35465E)
    p.set(15, 9, 0x35465E)
    p.set(12, 9, gold); p.set(12, 8, gold); p.set(12, 10, gold); p.set(11, 9, gold); p.set(13, 9, gold)
    return finish(p)


def water(frame, deep, mid, light, seed):
    """Two-frame shimmer: the highlights drift sideways and swap rows."""
    p = Pix(T, T, mid)
    r = rng('water', seed)
    marks = [(r.randrange(T), y, r.randrange(3, 6)) for y in (1, 4, 7, 10, 13, 16, 19, 22)]
    for x, y, n in marks:
        if frame:
            x += 2
            y = (y + 1) % T
        for i in range(n):
            p.set((x + i) % T, y, light)
        for i in range(n):
            p.set((x + i + 1) % T, (y + 1) % T, deep) if i % 2 == 0 else None
    return p


def wall_tile():
    p = Pix(T, T, 0x6E6A66)
    for y in range(0, T, 6):
        p.rect(0, y, T - 1, y, 0x4A4744)
        off = 0 if (y // 6) % 2 == 0 else 6
        for x in range(off, T, 12):
            p.rect(x, y, x, y + 5, 0x4A4744)
    p.rect(0, 1, T - 1, 1, 0x8A8680) if False else None
    return p


def road_mask(arms, x, y, half=4):
    """Is (x, y) road for a piece with `arms` (JoinBit set), arms may run off the tile?"""
    cx0, cx1 = 12 - half, 11 + half
    inside_c = cx0 <= x <= cx1 and cx0 <= y <= cx1
    if inside_c:
        return True
    if arms & 1 and cx0 <= x <= cx1 and y < cx0:
        return True
    if arms & 4 and cx0 <= x <= cx1 and y > cx1:
        return True
    if arms & 2 and cx0 <= y <= cx1 and x > cx1:
        return True
    if arms & 8 and cx0 <= y <= cx1 and x < cx0:
        return True
    return False


def road_piece(arms, seed=1):
    p = grass(70 + arms, tufts=4)
    for y in range(T):
        for x in range(T):
            if road_mask(arms, x, y):
                edge = any(not road_mask(arms, x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                p.set(x, y, 0xA89A78 if edge else 0xD3C9A6)
    # pebbles
    r = rng('road', arms)
    for _ in range(6):
        x, y = r.randrange(T), r.randrange(T)
        if road_mask(arms, x, y) and all(road_mask(arms, x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            p.set(x, y, 0xB8AC88)
    # a lone square (no neighbours) reads as a small round patch
    if arms == 0:
        p = grass(70, tufts=4)
        p.circle(11.5, 11.5, 5, 0xA89A78)
        p.circle(11.5, 11.5, 4, 0xD3C9A6)
    return p


def bridge_piece(arms):
    p = water(0, 0x3873B0, 0x4A8FCB, 0x9CCBF0, 2)
    h0, h1 = 6, 17  # deck band (12 px wide)
    deck, plank, rail = 0xB07F4C, 0x8C6238, 0x5B3F24
    def band(horizontal, lo, hi):
        for a in range(lo, hi + 1):
            for b in range(h0, h1 + 1):
                x, y = (a, b) if horizontal else (b, a)
                edge = b in (h0, h1)
                alt = (a % 4) == 0
                p.set(x, y, rail if edge else (plank if alt else deck))
    if arms & 10 or arms == 0:  # E/W arm or standalone: horizontal deck
        lo = 0 if arms & 8 else 6
        hi = 23 if arms & 2 else 17
        if arms == 0:
            lo, hi = 4, 19
        band(True, lo, hi)
    if arms & 5:
        lo = 0 if arms & 1 else 6
        hi = 23 if arms & 4 else 17
        band(False, lo, hi)
    return p
