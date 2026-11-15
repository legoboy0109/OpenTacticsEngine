"""Structure pictures on hexagons (docs/HEX.md "Structures").

One-hexagon structures: a picture cell as wide as the hexagon's box and EXTRA pixels taller, the
box on the bottom of the cell, so towers and chimneys rise above it as on squares. The buildings are
the square set's (art_tiles *_body) standing on a paving that has the shape of the lower half of the
hexagon.

The structure with rings (the fortress): one picture across the box of the seven hexagons (pointy
3T x 3L/2 + L, flat turned), bottom edge on the box's bottom, drawn on paving that has exactly the
shape of the seven hexagons (everything else transparent so the terrain shows around it).
"""
import art_tiles as tl
from pix import Pix

EXTRA = 4
OUT = tl.DARKLINE
STONE = tl.STONE


def cell_size(g):
    return g.w, g.h + EXTRA


def _plot(g, p, oy, top_row, base, light, dark):
    """Paving in the shape of the part of the hexagon below row `top_row`, outlined."""
    def in_plot(x, y):
        return y >= top_row and g.inside(x, y)
    for y in range(g.h):
        for x in range(g.w):
            if not in_plot(x, y):
                continue
            if not all(in_plot(x + dx, y + dy) for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0))):
                c = OUT
            elif not in_plot(x, y - 2):
                c = light
            elif not in_plot(x, y + 2) or not (in_plot(x - 1, y + 1) and in_plot(x + 1, y + 1)):
                c = dark
            else:
                c = base
            p.set(x, oy + y, c)


def _single(g, body_fn, plot_colours, behind=None):
    cw, ch = cell_size(g)
    p = Pix(cw, ch)
    _plot(g, p, EXTRA, g.h // 2 + 1 if g.pointy else g.h // 2 - 3, *plot_colours)
    # the 24 x 24 square-set bodies: pointy puts the body's foot at row 21 of the hexagon (full width
    # there); flat is wider than the body and shorter, so the body goes to the top of the cell
    bx, by = (0, EXTRA) if g.pointy else ((g.w - 24) // 2, 0)
    if behind:
        p.paste(behind(), bx, by)
    p.paste(body_fn(), bx, by)
    return p


def city(g):
    def tower():
        t = Pix(24, 24)
        t.rect(9, 3, 14, 22, 0xD6CFBA)
        t.rect(9, 3, 14, 3, 0xEDE8D6)
        t.rect(13, 4, 14, 22, 0xB4AC96)
        for x in range(9, 15):
            t.rect(x, 0 if x in (11, 12) else 1, x, 3, 0x6F819A)
        t.rect(11, 0, 12, 0, 0x56667C)
        t.rect(10, 6, 13, 9, 0xF3EFE0)
        t.rect(11, 7, 12, 8, 0x35465E)
        t.set(11, 7, 0xF3EFE0)
        t.rect(11, 12, 12, 15, 0x35465E)
        t.outline(OUT)
        return t
    return _single(g, tl.city_body, (0xC9C3B0, 0xE0DBC8, 0xA8A392), behind=tower)


def factory(g):
    def body():
        p = tl.factory_body()
        for x0 in (4, 13):  # taller chimneys, as the square factory
            p.rect(x0, 0, x0 + 2, 3, 0xA85A44)
            p.rect(x0 + 2, 0, x0 + 2, 3, 0x80402F)
            p.rect(x0, 0, x0 + 2, 0, 0x4A4A52)
            p.set(x0 - 1, 0, OUT)
            p.set(x0 + 3, 0, OUT)
            p.rect(x0 - 1, 1, x0 - 1, 3, OUT)
            p.rect(x0 + 3, 1, x0 + 3, 3, OUT)
        return p
    return _single(g, body, (0xA9A9AF, 0xC4C4CA, 0x8C8C94))


def hq(g):
    return _single(g, tl.hq_body, (0xD9CFA6, 0xEFE7C8, 0xB9B38F))


# ---- the fortress (rings: 1) ---------------------------------------------------------------------

WALL_H, TOWER_H = 7, 13


def _tower(p, bx, by):
    """A tower whose foot is at (bx, by): lit left, shaded right, battlements."""
    x0, x1, top = bx - 5, bx + 4, by - TOWER_H
    p.rect(x0, top, x1, by, STONE[0])
    p.rect(x0, top, x0 + 1, by, STONE[2])
    p.rect(x1 - 2, top, x1, by, STONE[1])
    for x in range(x0, x1 + 1, 4):                 # merlons
        p.rect(x, top - 3, min(x + 1, x1), top - 1, STONE[0])
        p.rect(x, top - 3, x, top - 1, STONE[2])
    p.rect(x0, top, x1, top, STONE[1])
    p.rect(bx - 1, top + 4, bx, top + 7, 0x3A2F28)  # arrow slit
    p.rect(x0, by - 1, x1, by, STONE[1])


def _keep(p, bx, by):
    x0, x1, top = bx - 10, bx + 10, by - 24
    p.rect(x0, top, x1, by, 0xD8D3C4)
    p.rect(x0, top, x1, top, 0xEFEBDD)
    p.rect(x1 - 4, top, x1, by, STONE[0])
    for y in range(top + 5, by, 5):
        p.rect(x0, y, x1, y, 0xC4BFAE)
    for x in range(x0, x1 + 1, 4):
        p.rect(x, top - 3, x + 1, top - 1, 0xD8D3C4)
        p.rect(x, top - 3, x, top - 1, 0xEFEBDD)
    p.rect(bx - 3, by - 9, bx + 2, by, 0x3A2F28)    # gate
    p.rect(bx - 2, by - 10, bx + 1, by - 10, 0x3A2F28)
    p.line(bx - 1, by - 9, bx - 1, by, 0x5A4636)
    for wx in (bx - 8, bx + 6):
        p.rect(wx, by - 18, wx + 1, by - 14, 0x35465E)
        p.set(wx, by - 18, 0x6A86A8)
    p.rect(bx - 1, by - 20, bx, by - 19, 0xD8B03A)  # gold crest
    p.line(bx, top - 4, bx, top - 10, 0x4A4038)     # flag
    p.rect(bx + 1, top - 10, bx + 5, top - 8, 0xF0E8D0)
    p.set(bx + 5, top - 9, 0xD8CDAE)


def _wall(p, a, b):
    """A wall between two tower feet: a ribbon of small front faces, lit on top."""
    n = int(max(abs(b[0] - a[0]), abs(b[1] - a[1]))) * 2
    for k in range(n + 1):
        x = a[0] + (b[0] - a[0]) * k / float(n)
        y = a[1] + (b[1] - a[1]) * k / float(n)
        xi, yi = int(round(x)), int(round(y))
        p.rect(xi - 2, yi - WALL_H, xi + 2, yi, STONE[0])
        p.rect(xi - 2, yi - WALL_H, xi + 2, yi - WALL_H, STONE[2])
        p.rect(xi - 2, yi, xi + 2, yi, STONE[1])
        if (xi + yi) % 5 == 0:
            p.rect(xi - 2, yi - 3, xi + 2, yi - 3, STONE[1])


def fortress(g):
    w, h = g.ring_size()
    p = Pix(w, h)
    (mx, my), outer = g.ring_centres()
    # paving shaped like the seven hexagons (box origins of the seven)
    boxes = [(int(round(cx - g.cx)), int(round(cy - g.cy))) for cx, cy in [(mx, my)] + outer]

    def covered(x, y):
        return any(g.inside(x - ox, y - oy) for ox, oy in boxes)
    for y in range(h):
        for x in range(w):
            if not covered(x, y):
                continue
            if not all(covered(x + dx, y + dy) for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0))):
                c = OUT
            elif not covered(x, y - 2):
                c = 0xC8C4B2
            elif (x // 4 + y // 4) % 2 == 0 and (x + y) % 3 == 0:
                c = 0x9C9886
            else:
                c = 0xB0AC98
            p.set(x, y, c)
    foot = [(cx, cy + 4) for cx, cy in outer]

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
            _wall(q, foot[a], foot[b])
        for i in towers_back:
            _tower(q, int(foot[i][0]), int(foot[i][1]))

    def draw_keep(q):
        _keep(q, int(mx), int(my) + 10)

    def draw_front(q):
        for a, b in front:
            _wall(q, foot[a], foot[b])
        for i in towers_front:
            _tower(q, int(foot[i][0]), int(foot[i][1]))
    layer(draw_back)
    layer(draw_keep)
    layer(draw_front)
    return p


# Structure id -> (picture function, rings). The hexagon starter set can draw these.
PICTURES = {
    'city': (city, 0),
    'factory': (factory, 0),
    'hq': (hq, 0),
    'fortress': (fortress, 1),
}
