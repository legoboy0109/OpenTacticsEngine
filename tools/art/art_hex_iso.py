"""Hexagon starter art for the 2D isometric view (docs/HEX.md "2D isometric", docs/GRAPHICS.md).

Geometry is hex_iso_geom.py (the squashed hexagon of the default measures: tileWidth 2 x T, heightStep tileWidth / 4):
pointy 48 x 28, flat 56 x 24 top faces.

Terrain cell (the contract's "top face and front faces in one cell"): the top face's box, RISE rows of room above it for
trees and peaks, and STEP rows of side faces below it, so 48 x 60 (pointy) / 56 x 56 (flat). The top face of plain, road,
water and bridge cells is the top-down hexagon picture of the same piece (art_hex_tiles.py), resampled by the fixed squash
and cut to exactly the squashed hexagon, so every join piece keeps its meaning (6-bit mask, 8 x 8 block) and roads and
banks still meet across a shared edge. Forest and mountain are drawn standing up. Under the top face: earth (stone for
mountains, planks for bridges, a dark blue wall for water), shaded lit / front / dark by which lower edge it lies under.

Structures stand on a stone plinth shaped like the hexagon (or the seven); units are the square isometric set's sprites.
"""
import art_hex_structures as hst
import art_hex_tiles as htl
import art_tiles as tl
import art_iso
from pix import Pix, hexc

RISE = 20

EARTH = art_iso.EARTH
ROCK = art_iso.ROCK
WATER_WALL = (0x3A6AA0, 0x2B4F7A)
WOOD_WALL = (0x7A5632, 0x5C4026)
PLINTH = (0xD6D1C0, 0xC9C3B0, 0xA8A392, 0x86816F)   # top light, top base, side lit, side shaded
OUT = tl.DARKLINE


def shade(c, k):
    r, g, b, a = c if isinstance(c, tuple) else hexc(c)
    return (int(r * k), int(g * k), int(b * k), a)


def sample(src, g, ig, x, y):
    """The top-down pixel under squashed pixel (x, y) (nearest; at the cut corners the nearest opaque one toward the
    middle)."""
    sx = min(g.w - 1, (2 * x + 1) * g.w // (2 * ig.w))
    sy = min(g.h - 1, (2 * y + 1) * g.h // (2 * ig.h))
    dx = 1 if sx < g.w / 2 else -1
    dy = 1 if sy < g.h / 2 else -1
    for r in range(0, 6):
        for ox, oy in (((0, 0),) if r == 0 else ((dx * r, 0), (0, dy * r), (dx * r, dy * r))):
            c = src.get(sx + ox, sy + oy)
            if c is not None:
                return c
    return src.get(g.w // 2, g.h // 2)


def lift(g, ig, src):
    """The top face: a top-down hexagon picture `src` squashed to the isometric hexagon (a Pix of box size)."""
    top = Pix(ig.w, ig.h)
    for y in range(ig.h):
        l, r = ig.row_span(y)
        for x in range(l, r):
            top.p[y][x] = sample(src, g, ig, x, y)
    return top


def faces(p, ig, sides, lip=None, top_row=RISE):
    """Side faces: `step` rows under the lower edge of the top face drawn at row `top_row` of p. `sides` = (lit, dark);
    the flat bottom of a flat hexagon is in between. `lip` colours the first two rows (the turf's edge)."""
    lit, dark = sides
    front = tuple((a + b) // 2 for a, b in zip(hexc(lit) if not isinstance(lit, tuple) else lit,
                                                hexc(dark) if not isinstance(dark, tuple) else dark))
    by_dir = {3: lit if ig.pointy else front, 4: lit, 2: dark}
    for x in range(ig.w):
        last = ig.last_row(x)
        if last < 0:
            continue
        c = p.get(x, top_row + last)
        if c is not None:
            p.set(x, top_row + last, shade(c, 0.88))   # the lower rim a shade darker: hexagons read apart
        side = by_dir[ig.face_dir(x)]
        for d in range(1, ig.step + 1):
            col = side
            if lip is not None and d <= 2:
                col = shade(lip, 0.92 if side == lit else 0.74)
            elif d == ig.step:
                col = shade(side, 0.8)
            p.set(x, top_row + last + d, col)


def cell_of(g, ig, tile, sides, lip=None, edge=False):
    """One terrain cell. `edge` darkens the top face's border a little so hexagons read apart on open ground (not on
    join pieces: roads and banks run across the border and must meet their neighbour's)."""
    p = Pix(*ig.cell)
    top = lift(g, ig, tile)
    if edge:
        for y in range(ig.h):
            for x in range(ig.w):
                if ig.inside(x, y) and not all(ig.inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    top.p[y][x] = shade(top.p[y][x], 0.92)
    p.paste(top, 0, RISE)
    faces(p, ig, sides, lip)
    return p


def forest(g, ig):
    p = cell_of(g, ig, htl.grass(g, 11, tufts=4), EARTH, lip=tl.GRASS[1], edge=True)
    cx, cy = ig.w // 2, RISE + ig.h // 2
    # a clump of trees standing on the top face: back ones first, the nearest in front
    spots = [(-12, -7), (11, -7), (-17, 3), (17, 3), (-4, -2), (4, 5), (-9, 8)] if ig.pointy else \
        [(-17, -5), (14, -6), (-5, -4), (22, 2), (-22, 2), (4, 4), (-10, 6)]
    for dx, dy in sorted(spots, key=lambda s: s[1]):
        tl.tree(p, cx + dx, cy + dy - 3)
    return p


def mountain(g, ig):
    p = cell_of(g, ig, htl.grass(g, 23, tufts=3), ROCK, lip=0x8E8878, edge=True)
    base = RISE + ig.h * 3 // 5
    htl._peak(p, ig.w * 3 // 10, base - 25, 12, base, 0xA39785, 0x7A6E5C, 4)
    htl._peak(p, ig.w * 11 // 20, base - 33, 17, base, 0xA89C8A, 0x6F6351, 6)
    p.rect(ig.w // 8, base, ig.w - ig.w // 8, base, 0x6F6351)
    return p


def terrain_sheet(g, ig, build):
    """The tile sheet in the same 16 x 26 layout as the top-down one (art_hex_art.build_tile_sheet)."""
    cw, ch = ig.cell
    sheet = Pix(build.SHEET_COLS * cw, build.SHEET_ROWS * ch)

    def put(cell, col, row):
        sheet.paste(cell, col * cw, row * ch)
    for v in range(3):
        put(cell_of(g, ig, htl.plain(g, v), EARTH, lip=tl.GRASS[1], edge=True), *build.TILE_CELLS['plain%d' % v])
    put(forest(g, ig), *build.TILE_CELLS['forest'])
    put(mountain(g, ig), *build.TILE_CELLS['mountain'])
    for n in range(build.PIECES):
        col, row = n % build.BLOCK, n // build.BLOCK
        put(cell_of(g, ig, htl.road_piece(g, n), EARTH, lip=tl.GRASS[1]), build.ROAD_AT[0] + col, build.ROAD_AT[1] + row)
        for f in range(build.FRAMES):
            put(cell_of(g, ig, htl.water_piece(g, n, f, tl.RIVER), WATER_WALL),
                build.WATER_AT[0] + build.BLOCK * f + col, build.WATER_AT[1] + row)
            put(cell_of(g, ig, htl.bridge_piece(g, n, f), WOOD_WALL),
                build.BRIDGE_AT[0] + build.BLOCK * f + col, build.BRIDGE_AT[1] + row)
    return sheet


# ---- structures ----------------------------------------------------------------------------------------------

PLINTH_DEPTH = 3


def _plinth(p, ig, boxes, oy_shift):
    """Paving in the shape of the union of the hexagons whose top-left corners are `boxes`, drawn `oy_shift` rows up
    from where they sit in p, and PLINTH_DEPTH rows of stone side faces under its lower edge."""
    def covered(x, y):
        return any(ig.inside(x - bx, y - by) for bx, by in boxes)
    xs = [bx for bx, _ in boxes]
    ys = [by for _, by in boxes]
    x0, x1 = min(xs), max(xs) + ig.w
    y0, y1 = min(ys), max(ys) + ig.h
    top_light, top_base, side_lit, side_dark = PLINTH
    for y in range(y0, y1):
        for x in range(x0, x1):
            if not covered(x, y):
                continue
            if not all(covered(x + dx, y + dy) for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0))):
                c = OUT
            elif not covered(x, y - 2):
                c = top_light
            elif (x // 4 + y // 4) % 2 == 0 and (x + y) % 3 == 0:
                c = shade(top_base, 0.93)
            else:
                c = top_base
            p.set(x, y, c)
    for x in range(x0, x1):
        last = max((y for y in range(y0, y1) if covered(x, y)), default=None)
        if last is None:
            continue
        mid = (x0 + x1) // 2
        for d in range(1, PLINTH_DEPTH + 1):
            p.set(x, last + d, side_lit if x < mid else side_dark)


def _single(g, ig, body_fn):
    cw, ch = ig.single_size()
    p = Pix(cw, ch)
    oy = ch - PLINTH_DEPTH - ig.h
    _plinth(p, ig, [(0, oy)], 0)
    body = body_fn()
    foot = oy + ig.h // 2 + 6   # the building stands a little in front of the plinth's middle
    p.paste(body, (cw - body.w) // 2, foot - body.h)
    return p


def city(g, ig):
    return _single(g, ig, tl.city_body)


def factory(g, ig):
    return _single(g, ig, tl.factory_body)


def hq(g, ig):
    return _single(g, ig, tl.hq_body)


def fortress(g, ig):
    """The structure with rings: paving in the shape of the seven hexagons, six towers on the outer ones joined by walls,
    a keep with a flag on the middle one. Picture: ring_size(), the box of the seven at its bottom."""
    w, h = ig.ring_size()
    p = Pix(w, h)
    box_h = ig.ring_box_height()
    # the middle hexagon's middle: the middle of the box of the seven, which sits on the picture's bottom edge
    mx, my = w // 2, h - box_h // 2 - PLINTH_DEPTH
    offs = ig.middle_offsets()
    middles = [(mx, my)] + [(mx + dx, my + dy) for dx, dy in offs]
    boxes = [(cx - ig.w // 2, cy - ig.h // 2) for cx, cy in middles]
    _plinth(p, ig, boxes, 0)
    outer = middles[1:]
    foot = [(cx, cy + 5) for cx, cy in outer]

    def layer(draw):
        q = Pix(w, h)
        draw(q)
        q.outline(OUT)
        for yy in range(h):
            for xx in range(w):
                if q.p[yy][xx] is not None:
                    p.p[yy][xx] = q.p[yy][xx]
    ring = [(i, (i + 1) % 6) for i in range(6)]
    back = [e for e in ring if (foot[e[0]][1] + foot[e[1]][1]) / 2.0 < my + 4]
    front = [e for e in ring if e not in back]
    towers_back = [i for i in range(6) if foot[i][1] <= my + 4]
    towers_front = [i for i in range(6) if i not in towers_back]

    def draw_back(q):
        for a, b in back:
            hst._wall(q, foot[a], foot[b])
        for i in towers_back:
            hst._tower(q, int(foot[i][0]), int(foot[i][1]))

    def draw_keep(q):
        hst._keep(q, mx, my + 8)

    def draw_front(q):
        for a, b in front:
            hst._wall(q, foot[a], foot[b])
        for i in towers_front:
            hst._tower(q, int(foot[i][0]), int(foot[i][1]))
    layer(draw_back)
    layer(draw_keep)
    layer(draw_front)
    return p


# Structure id -> (picture function, rings), like art_hex_structures.PICTURES.
PICTURES = {'city': (city, 0), 'factory': (factory, 0), 'hq': (hq, 0), 'fortress': (fortress, 1)}
