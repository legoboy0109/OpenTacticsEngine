"""Geometry of hexagons in the 2D isometric view (docs/HEX.md "2D isometric"), the Python twin of
engine/core/view/hex_iso.cpp + hex_geo.cpp (hexagon row spans). The art generator, its tests and the player test use this
one copy; the player test also checks it against the engine's real pixels.

A top-down hexagon box is T x L (pointy) or L x T (flat), L = the multiple of 4 nearest to T * 1.1547. Seen from the
fixed angle every top-down position (x, y) goes to (x * sx, y * sy), sy = sx / 2, sx = tileWidth / T. The squashed
hexagons are a hexagon lattice of their own whose box is
  pointy: tileWidth wide, the multiple of 4 nearest to L * sy tall;
  flat:   the multiple of 4 nearest to L * sx wide, tileWidth / 2 (rounded up to even) tall;
a long side that would leave a one-pixel seam between neighbours moves to the nearer multiple of 4 that does not.
"""
import math

DIR_COUNT = 6


def long_side(tile):
    """Top-down L for a tile size (24 -> 28)."""
    return max(4, int(math.floor(tile * 1.1547005 / 4.0 + 0.5)) * 4)


def _round_div(num, den):
    return (2 * num + den) // (2 * den)


def _nearest4(num, den):
    return max(4, 4 * _round_div(num, 4 * den))


def _tiling_long_side(long_side_, exact, pointy, short):
    def ok(l):
        rows, rise = (l // 4, short // 2) if pointy else (short // 2, l // 4)
        return rows % 2 == 0 or rise % 2 == 0
    if ok(long_side_):
        return long_side_
    lo, hi = max(4, long_side_ - 4), long_side_ + 4
    if not ok(lo):
        return hi
    if not ok(hi):
        return lo
    return lo if abs(lo - exact) <= abs(hi - exact) else hi


def _ramp(j, h, q):
    """The engine's ramp: q integer steps from about 0 to h, ramp(j) + ramp(q - 1 - j) == h."""
    if q <= 0:
        return h
    if 2 * j + 1 > q:
        return h - _ramp(q - 1 - j, h, q)
    if 2 * j + 1 == q:
        return h // 2
    return (h * (2 * j + 1) + q) // (2 * q)


class IsoGeom:
    """One layout's squashed hexagons. `w` x `h` is the top face's box; `rise` rows above it and `step` rows below
    it (the side faces) make the terrain art cell, `cell` = (width, height)."""

    def __init__(self, layout, tile=24, tile_width=None, step=None, rise=20):
        assert layout in ('pointy', 'flat')
        self.layout, self.tile = layout, tile
        self.pointy = layout == 'pointy'
        self.tile_width = tile_width or tile * 2
        self.step = self.tile_width // 4 if step is None else step
        self.rise = rise
        L = long_side(tile)
        W = self.tile_width
        if self.pointy:
            self.T = W
            self.L = _tiling_long_side(_nearest4(L * W, 2 * tile), L * W / (2.0 * tile), True, W)
            self.w, self.h = self.T, self.L
        else:
            self.T = W // 2 + (W // 2) % 2
            self.L = _tiling_long_side(_nearest4(L * W, tile), L * W / float(tile), False, self.T)
            self.w, self.h = self.L, self.T
        self.q = self.L // 4
        self.pitch = 3 * self.q

    @property
    def cell(self):
        """Terrain art cell: the top face's box with `rise` rows above it and `step` rows of side faces below."""
        return self.w, self.rise + self.h + self.step

    def row_span(self, row):
        """Pixels [l, r) of row `row` (0 .. h-1) that belong to the hexagon, relative to the box's left edge."""
        T, L = self.T, self.L
        if self.pointy:
            q, h = L // 4, T // 2
            if row < q:
                hw = _ramp(row, h, q)
            elif row < 3 * q:
                hw = h
            else:
                hw = _ramp(q - 1 - min(row - 3 * q, q - 1), h, q)
            return T // 2 - hw, T // 2 + hw
        half, h = T // 2, L // 4
        d = half - 1 - row if row < half else min(row - half, half - 1)
        hw = L // 2 - _ramp(d, h, half)
        return L // 2 - hw, L // 2 + hw

    def inside(self, x, y):
        if not (0 <= y < self.h):
            return False
        l, r = self.row_span(y)
        return l <= x < r

    def last_row(self, x):
        """The lowest row of the top face in column x (-1 if the column has none)."""
        for y in range(self.h - 1, -1, -1):
            if self.inside(x, y):
                return y
        return -1

    def face_dir(self, x):
        """Which lower edge's side face lies under column x: 3 = lower left, 2 = lower right, 4 = (flat) the left
        quarter, 3 = (flat) the flat bottom; the shade picks lit / front / dark from it."""
        if self.pointy:
            return 3 if x < self.w // 2 else 2
        return 4 if x < self.q else (3 if x < self.w - self.q else 2)

    # ---- where things go on a map ----

    def box_origin(self, x, y):
        if self.pointy:
            return x * self.T + (self.T // 2 if y % 2 else 0), y * self.pitch
        return x * self.pitch, y * self.T + (self.T // 2 if x % 2 else 0)

    def map_size(self, cols, rows):
        if self.pointy:
            return cols * self.T + self.T // 2, (rows - 1) * self.pitch + self.L
        return (cols - 1) * self.pitch + self.L, rows * self.T + self.T // 2

    def neighbor(self, x, y, d):
        if self.pointy:
            off = ([(0, -1), (1, 0), (0, 1), (-1, 1), (-1, 0), (-1, -1)] if y % 2 == 0
                   else [(1, -1), (1, 0), (1, 1), (0, 1), (-1, 0), (0, -1)])
        else:
            off = ([(0, -1), (1, -1), (1, 0), (0, 1), (-1, 0), (-1, -1)] if x % 2 == 0
                   else [(0, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)])
        return x + off[d][0], y + off[d][1]

    def middle_offsets(self):
        """Offsets from a hexagon's middle to its six neighbours' middles (contract order)."""
        T, p = self.T, self.pitch
        if self.pointy:
            return [(T // 2, -p), (T, 0), (T // 2, p), (-T // 2, p), (-T, 0), (-T // 2, -p)]
        return [(0, -T), (p, -T // 2), (p, T // 2), (0, T), (-p, T // 2), (-p, -T // 2)]

    # ---- structures ----

    def single_size(self, extra=15):
        """(width, height) of a one-hexagon structure's picture: the box wide, `extra` rows above the top face."""
        return self.w, self.h + 3 + extra

    def ring_box_height(self):
        """Height of the box of the seven hexagons (no towers): pointy 3L/2 + L, flat 3T."""
        return 3 * self.L // 2 + self.L if self.pointy else 3 * self.T

    def ring_size(self, extra=26):
        """(width, height) of a ring structure's picture: across the box of the seven hexagons (pointy 3T wide,
        3L/2 + L tall; flat turned), plus `extra` rows for towers standing above it."""
        if self.pointy:
            return 3 * self.T, self.ring_box_height() + extra
        return 3 * self.L // 2 + self.L, self.ring_box_height() + extra
