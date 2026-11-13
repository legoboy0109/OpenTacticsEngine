"""Tiny pixel-art canvas used by make_starter_art.py (Pillow only for saving)."""
import random

from PIL import Image


def hexc(v):
    """0xRRGGBB (or '#rrggbb') -> (r, g, b, 255)."""
    if isinstance(v, str):
        v = int(v.lstrip('#'), 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255, 255)


class Pix:
    """A w x h canvas of RGBA pixels (None = transparent)."""

    def __init__(self, w, h, fill=None):
        self.w, self.h = w, h
        self.p = [[fill] * w for _ in range(h)]

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = hexc(c) if not isinstance(c, tuple) and c is not None else c

    def rect(self, x0, y0, x1, y1, c):
        """Filled rectangle, both corners inclusive."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def circle(self, cx, cy, r, c):
        for y in range(int(cy - r) - 1, int(cy + r) + 2):
            for x in range(int(cx - r) - 1, int(cx + r) + 2):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r + 0.3:
                    self.set(x, y, c)

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def outline(self, c):
        """1px outline around everything opaque (on transparent pixels only)."""
        c = hexc(c)
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is None and any(
                        self.get(x + dx, y + dy) is not None for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add:
            self.p[y][x] = c

    def paste(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                if other.p[y][x] is not None:
                    self.set(ox + x, oy + y, other.p[y][x])

    def image(self):
        im = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is not None:
                    im.putpixel((x, y), self.p[y][x])
        return im

    def mapped(self, fn):
        """A copy with fn(r, g, b) -> (r, g, b) applied to every opaque pixel."""
        out = Pix(self.w, self.h)
        for y in range(self.h):
            for x in range(self.w):
                c = self.p[y][x]
                if c is not None:
                    r, g, b = fn(c[0], c[1], c[2])
                    out.p[y][x] = (max(0, min(255, int(r))), max(0, min(255, int(g))), max(0, min(255, int(b))), c[3])
        return out


def rng(*seed):
    return random.Random(hash_seed(seed))


def hash_seed(seed):
    h = 2166136261
    for s in seed:
        for ch in str(s):
            h = ((h ^ ord(ch)) * 16777619) & 0xFFFFFFFF
    return h
