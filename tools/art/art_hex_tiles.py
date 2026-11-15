"""Hexagon terrain tiles of the starter art set (docs/HEX.md "Starter content").

Same retro look and palette as art_tiles.py, drawn into a hexagon's box (24 x 28 pointy,
28 x 24 flat) and cut to the hexagon at the end (corners transparent). Every function takes a
`Geom` (hex_geom.py) so both layouts share the code.

Join pieces: a hexagon has six neighbours, so a join block is 8 x 8 pieces; piece n sits at column
n % 8, row n / 8 and n is the six-bit mask whose bit i is set when neighbour i counts as the same
(neighbour order of docs/HEX.md). Roads and bridge decks run from the middle toward every set bit;
water draws a bank on every side whose bit is clear.
"""
import math

import art_tiles as tl
from pix import Pix, rng

GRASS = tl.GRASS
BANK = tl.BANK
ROAD_HW = 4.0          # half width of a road arm (pixels)
ROAD_BLOB = 5.5        # radius of the road's middle patch


def grass(g, seed, tufts=7, flowers=0):
    p = Pix(g.w, g.h, GRASS[0])
    r = rng('hexgrass', g.layout, seed)
    for _ in range(int(tufts * g.w * g.h / 576.0 + 0.5)):
        x, y = r.randrange(1, g.w - 1), r.randrange(2, g.h - 1)
        p.set(x, y, GRASS[1])
        p.set(x, y - 1, GRASS[1])
        p.set(x + 1, y, GRASS[1]) if r.random() < 0.5 else p.set(x - 1, y, GRASS[1])
    for _ in range(6):
        p.set(r.randrange(g.w), r.randrange(g.h), GRASS[2])
    for _ in range(flowers):
        x, y = r.randrange(3, g.w - 3), r.randrange(3, g.h - 3)
        p.set(x, y, 0xF4F0E0)
        p.set(x, y + 1, 0xE8C040) if r.random() < 0.5 else None
    return p


def plain(g, variant):
    p = grass(g, variant + 1, tufts=(6, 9, 5)[variant], flowers=3 if variant == 2 else 0)
    return g.mask_out(p)


def forest(g):
    p = grass(g, 11, tufts=4)
    cx, cy = int(g.cx), int(g.cy)
    # three trees (art_tiles.tree): two behind, one in front; canopies stay inside the hexagon
    spots = ((-5, -3), (5, -3), (0, 4)) if g.pointy else ((-6, -3), (5, -3), (-1, 3))
    for dx, dy in spots:
        tl.tree(p, cx + dx, cy + dy)
    return g.mask_out(p)


def _peak(p, ax, ay, hw, base, light, dark, snow_rows):
    """A mountain peak with its apex at (ax, ay) widening to half width hw at row `base`."""
    for y in range(ay, base + 1):
        half = (y - ay) * hw / float(base - ay)
        for x in range(int(ax - half), int(ax + half) + 1):
            c = light if x <= ax else dark
            if y - ay < snow_rows:
                c = 0xF2F5F8 if x <= ax else 0xC9D2DD
                if y - ay == snow_rows - 1 and (x + y) % 2 == 0:
                    c = light if x <= ax else dark
            p.set(x, y, c)
    p.line(ax - 1, ay + snow_rows + 2, ax - 3, ay + snow_rows + 6, dark)
    p.line(ax + 2, ay + snow_rows + 4, ax + 3, ay + snow_rows + 8, 0x5A4E40)


def mountain(g):
    p = grass(g, 23, tufts=3)
    if g.pointy:
        base, (sx, sa), (bx, ba) = 21, (7, 9), (14, 3)
    else:  # the flat hexagon is shorter, so the range sits higher and a little wider
        base, (sx, sa), (bx, ba) = 18, (9, 7), (18, 2)
    _peak(p, sx, sa, 7, base, 0xA39785, 0x7A6E5C, 3)
    _peak(p, bx, ba, 10, base, 0xA89C8A, 0x6F6351, 5)
    p.rect(0, base, g.w - 1, base, 0x6F6351)
    p.rect(0, base + 1, g.w - 1, base + 2, GRASS[1])
    return g.mask_out(p)


# ---- roads --------------------------------------------------------------------------------------

def _road_at(g, arms, px, py):
    """Is the point (px, py) (cell pixels, may lie outside the cell) road for a piece with the arms
    `arms` (six-bit mask)? An arm is a half-line from the middle toward the neighbour, ROAD_HW each
    side: both pieces on either side of a shared edge compute the same line, so roads meet exactly."""
    rx, ry = px - g.cx, py - g.cy
    if rx * rx + ry * ry <= ROAD_BLOB * ROAD_BLOB:
        return True
    for i, (ux, uy) in enumerate(g.pixel_dir_units()):
        if arms >> i & 1:
            along = rx * ux + ry * uy
            across = abs(rx * uy - ry * ux)
            if along >= 0 and across <= ROAD_HW:
                return True
    return False


def road_piece(g, n):
    p = grass(g, 70 + n, tufts=4)
    def road(x, y):
        return _road_at(g, n, x + 0.5, y + 0.5)
    for y in range(g.h):
        for x in range(g.w):
            if road(x, y):
                edge = any(not road(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                # outside the cell the neighbour hexagon continues the road, so only mark an edge pixel
                # when the cell's own hexagon says "not road" next to it
                edge = edge and any(g.inside(x + dx, y + dy) and not road(x + dx, y + dy)
                                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                p.set(x, y, 0xA89A78 if edge else 0xD3C9A6)
    r = rng('hexroad', g.layout, n)
    for _ in range(6):
        x, y = r.randrange(g.w), r.randrange(g.h)
        if all(road(x + dx, y + dy) for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1))):
            p.set(x, y, 0xB8AC88)
    return g.mask_out(p)


# ---- water --------------------------------------------------------------------------------------

def _clip(poly, g, i):
    """Keeps the part of the convex polygon `poly` (cell pixels) at least BANK inside edge i."""
    out = []
    for k in range(len(poly)):
        a, b = poly[k], poly[(k + 1) % len(poly)]
        da, db = g.edge_depth(i, *a) - BANK, g.edge_depth(i, *b) - BANK
        if da >= 0:
            out.append(a)
        if (da >= 0) != (db >= 0):
            t = da / (da - db)
            out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return out


def _dist_to_poly(poly, px, py):
    """0 when (px, py) is inside the convex clockwise-or-not polygon, else the distance to it."""
    if len(poly) < 3:
        return 99.0
    sign, best = 0, 1e9
    inside = True
    for k in range(len(poly)):
        (ax, ay), (bx, by) = poly[k], poly[(k + 1) % len(poly)]
        ex, ey = bx - ax, by - ay
        cross = ex * (py - ay) - ey * (px - ax)
        s = 1 if cross > 0 else -1
        sign = sign or s
        if s != sign:
            inside = False
        t = max(0.0, min(1.0, ((px - ax) * ex + (py - ay) * ey) / float(ex * ex + ey * ey or 1)))
        best = min(best, math.hypot(px - (ax + ex * t), py - (ay + ey * t)))
    return 0.0 if inside else best


def water(g, frame, pal):
    """Two-frame shimmer (as art_tiles.water, but on a w x h box): highlights drift and swap rows."""
    deep, mid, light, seed = pal
    p = Pix(g.w, g.h, mid)
    r = rng('hexwater', g.layout, seed)
    for y in range(1, g.h, 3):
        x, n = r.randrange(g.w), r.randrange(3, 6)
        if frame:
            x, y = x + 2, (y + 1) % g.h
        for i in range(n):
            p.set((x + i) % g.w, y, light)
        for i in range(0, n, 2):
            p.set((x + i + 1) % g.w, (y + 1) % g.h, deep)
    return p


def water_piece(g, mask, frame, pal):
    """One of the 64 water join pieces; `mask` = neighbours that ARE water. Each side without a water
    neighbour gets a BANK wide sand bank with a foam line (as the square water): the water "core" is the
    hexagon cut back by BANK on those sides, and the bank is graded by the Euclidean distance from that
    core, which rounds the corners of a lake or a bend. Bank and foam outline are the same in every
    frame; only the foam gaps and the open water shimmer."""
    p = water(g, frame, pal)
    big = [(g.cx + x * 3, g.cy + y * 3) for x, y in g.corners]
    core = big
    for i in range(6):
        if not mask >> i & 1:
            core = _clip(core, g, i)
    if mask == 63:
        return g.mask_out(p)
    for y in range(g.h):
        for x in range(g.w):
            e = _dist_to_poly(core, x + 0.5, y + 0.5)
            if e <= 0:
                continue
            if e <= 1.5:
                p.set(x, y, tl.SHALLOW if (x + y + 2 * frame) % 4 == 0 else tl.FOAM)
            elif e <= 2.5:
                p.set(x, y, tl.SHORE_WET)
            elif e <= BANK - 0.5:
                p.set(x, y, tl.SHORE_SAND_DARK if (x * 3 + y * 5) % 11 == 0 else tl.SHORE_SAND)
            else:
                p.set(x, y, tl.SHORE_FRINGE)
    return g.mask_out(p)


def bridge_piece(g, arms, frame):
    """Water with a plank deck along every arm (arms = neighbours that are not water). Banks on the
    sides with an arm, like the square bridge. A piece with no arm is a short deck along the axis that
    exists in the layout (left-right pointy, up-down flat)."""
    p = water_piece(g, ~arms & 63, frame, tl.RIVER)
    deck, plank, rail = 0xB07F4C, 0x8C6238, 0x5B3F24
    units = g.pixel_dir_units()
    if arms == 0:
        axis = 1 if g.pointy else 0
        runs = [(axis, 7.0), (axis + 3, 7.0)]      # (direction, how far from the middle)
    else:
        runs = [(i, 99.0) for i in range(6) if arms >> i & 1]
    for y in range(g.h):
        for x in range(g.w):
            rx, ry = x + 0.5 - g.cx, y + 0.5 - g.cy
            for i, reach in runs:
                ux, uy = units[i]
                along, across = rx * ux + ry * uy, abs(rx * uy - ry * ux)
                if -5.5 <= along <= reach and across <= 5.5:
                    on_rail = across > 4.5
                    p.set(x, y, rail if on_rail else (plank if int(math.floor(along + 0.5)) % 4 == 0 else deck))
                    break
    return g.mask_out(p)
