"""Hexagon geometry of the hexagon starter art (docs/HEX.md "Top-down picture").

One `Geom` per layout. Everything here is in the pixels of one cell (the hexagon's box):
pointy cells are T wide and L tall, flat cells L wide and T tall (L = the multiple of 4 nearest
to T * 1.1547). The hexagon inside a box touches the box on all four sides; `inside(x, y)` tests a
pixel by its centre. The box corners outside the hexagon stay transparent (the engine does not clip).
No pixel centre can lie exactly on an edge (the parities never agree), so the hexagons of a map
partition the pixel grid exactly: no gaps and no overlaps between neighbours.
"""
import math

DIR_COUNT = 6


class Geom:
    def __init__(self, layout, T=24):
        assert layout in ('pointy', 'flat')
        self.layout, self.T = layout, T
        self.L = int(round(T * 1.1547 / 4.0)) * 4
        L = self.L
        self.pointy = layout == 'pointy'
        self.w, self.h = (T, L) if self.pointy else (L, T)
        self.cx, self.cy = self.w / 2.0, self.h / 2.0
        # Distance between neighbouring boxes: rows (pointy) or columns (flat) are 3L/4 apart.
        self.pitch = 3 * L // 4
        # Offsets from a hexagon's middle to its neighbours' middles, in the contract's direction order.
        if self.pointy:
            self.dirs = [(T / 2, -self.pitch), (T, 0), (T / 2, self.pitch),
                         (-T / 2, self.pitch), (-T, 0), (-T / 2, -self.pitch)]
            hw, q = T / 2.0, L / 4.0
            # corners clockwise from the top; edge i joins corner i and corner i + 1 and faces direction i
            self.corners = [(0, -L / 2.0), (hw, -q), (hw, q), (0, L / 2.0), (-hw, q), (-hw, -q)]
        else:
            self.dirs = [(0, -T), (self.pitch, -T / 2), (self.pitch, T / 2),
                         (0, T), (-self.pitch, T / 2), (-self.pitch, -T / 2)]
            hh, q = T / 2.0, L / 4.0
            # corners clockwise from the upper left; edge i faces direction i (0 = up)
            self.corners = [(-q, -hh), (q, -hh), (L / 2.0, 0), (q, hh), (-q, hh), (-L / 2.0, 0)]

    def inside(self, x, y):
        dx, dy = abs(x + 0.5 - self.cx), abs(y + 0.5 - self.cy)
        T, L = self.T, self.L
        if self.pointy:
            return dx <= T / 2.0 and dy + dx * L / (2.0 * T) <= L / 2.0
        return dy <= T / 2.0 and dx + dy * L / (2.0 * T) <= L / 2.0

    def mask_out(self, pix):
        """Makes every pixel outside the hexagon transparent (in place); returns pix."""
        for y in range(pix.h):
            for x in range(pix.w):
                if not self.inside(x, y):
                    pix.p[y][x] = None
        return pix

    def edge_depth(self, i, px, py):
        """How far (pixels) the point (px, py), in cell pixels, lies inside of edge i's line."""
        (ax, ay), (bx, by) = self.corners[i], self.corners[(i + 1) % 6]
        ex, ey = bx - ax, by - ay
        n = math.hypot(ex, ey)
        # outward normal of a clockwise polygon in y-down pixels is (ey, -ex)
        nx, ny = ey / n, -ex / n
        return -((px - self.cx - ax) * nx + (py - self.cy - ay) * ny)

    def pixel_dir_units(self):
        """Unit vectors (in pixels, y down) from a hexagon's middle toward each neighbour."""
        out = []
        for dx, dy in self.dirs:
            n = math.hypot(dx, dy)
            out.append((dx / n, dy / n))
        return out

    # ---- where things go on a map (used by the preview and the tests) ----

    def box_origin(self, x, y):
        """Top-left pixel of cell (x, y)'s box on the map."""
        if self.pointy:
            return x * self.T + (self.T // 2 if y % 2 else 0), y * self.pitch
        return x * self.pitch, y * self.T + (self.T // 2 if x % 2 else 0)

    def map_size(self, width, height):
        if self.pointy:
            return width * self.T + self.T // 2, (height - 1) * self.pitch + self.L
        return (width - 1) * self.pitch + self.L, height * self.T + self.T // 2

    def neighbor(self, x, y, d):
        """(x, y) of neighbour d in the contract's order (may lie off the map)."""
        if self.pointy:
            if y % 2 == 0:
                off = [(0, -1), (1, 0), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
            else:
                off = [(1, -1), (1, 0), (1, 1), (0, 1), (-1, 0), (0, -1)]
        else:
            if x % 2 == 0:
                off = [(0, -1), (1, -1), (1, 0), (0, 1), (-1, 0), (-1, -1)]
            else:
                off = [(0, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
        return x + off[d][0], y + off[d][1]

    # ---- the picture of a structure with rings: across the box of its seven hexagons ----

    def ring_size(self):
        """(width, height) of a ring structure's picture: 3T x 3L/2 + L pointy, turned for flat."""
        T, L = self.T, self.L
        return (3 * T, 3 * L // 2 + L) if self.pointy else (3 * L // 2 + L, 3 * T)

    def ring_centres(self):
        """Middles of the middle hexagon and of its six neighbours inside the ring picture."""
        w, h = self.ring_size()
        mx, my = w / 2.0, h / 2.0
        # the picture's bottom edge is on the lowest box's bottom; the middle hexagon's middle is the
        # picture's middle in both layouts (the union of the seven boxes is symmetric)
        return (mx, my), [(mx + dx, my + dy) for dx, dy in self.dirs]

    def ring_box_offset(self):
        """Offset of the middle hexagon's box from the ring picture's top-left corner."""
        w, h = self.ring_size()
        return (w - self.w) // 2, (h - self.h) // 2
