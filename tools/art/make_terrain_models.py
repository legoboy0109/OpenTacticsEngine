#!/usr/bin/env python3
"""Writes demo-srpg's standing terrain models for its "Detailed 3D" style:
games/demo-srpg/models/detailed/forest_{a,b,c}.glb and mountain_{a,b}.glb, and puts them into
art/detailed.style.json as the "stand" of the forest and the mountain.

usage: python3 tools/art/make_terrain_models.py [REPO_ROOT]

Original low-poly models, generated here from cones, prisms and an icosahedron with flat shading and
vertex colours (no textures, nothing downloaded): clumps of pines and round trees, rocky peaks with
snow. Each stands on Y = 0, centred in X/Z, at most 0.45 from the middle (so it fits its square),
about 1 tall; the game scales them ("height") and the player turns each one a little differently
(`scatter`). The ground picture under them is the style's own flat ground texture.

Pure Python standard library; deterministic (fixed seeds), so re-running reproduces identical files.
make_detailed_style.py runs this too, so the style can be rebuilt in one go.
"""
import json
import math
import os
import random
import struct
import sys

GAME = 'games/demo-srpg'
OUT = 'models/detailed'
MODELS = {  # file stem -> (builder name, seed)
    'forest_a': ('pines', 1), 'forest_b': ('round', 2), 'forest_c': ('mixed', 3),
    'mountain_a': ('peaks_a', 4), 'mountain_b': ('peaks_b', 5),
}
FOREST_FILES = ['forest_a', 'forest_b', 'forest_c']
MOUNTAIN_FILES = ['mountain_a', 'mountain_b']


def rgb(h):
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255)


TRUNK = rgb(0x6B4526)
PINE = [rgb(0x2B6A3B), rgb(0x1F5530), rgb(0x357A45)]
LEAF = [rgb(0x3F8A3C), rgb(0x2F6B34), rgb(0x55A14A)]
ROCK = [rgb(0x8E8878), rgb(0x7A6E5C), rgb(0xA39785), rgb(0x6F6351)]
SNOW = rgb(0xF2F5F8)


class Mesh:
    """Flat-shaded triangles: every triangle has its own three vertices, a normal and a colour."""

    def __init__(self):
        self.tris = []   # (a, b, c, colour)

    def tri(self, a, b, c, colour, inside):
        """A triangle wound so that it faces away from the point `inside` (a point inside the solid)."""
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        mid = [(a[i] + b[i] + c[i]) / 3 - inside[i] for i in range(3)]
        if sum(n[i] * mid[i] for i in range(3)) < 0:
            b, c = c, b
        self.tris.append((a, b, c, colour))

    def quad(self, a, b, c, d, colour, inside):
        self.tri(a, b, c, colour, inside)
        self.tri(a, c, d, colour, inside)


def vary(palette, rnd):
    return palette[rnd.randrange(len(palette))]


def ring(cx, cz, r, y, n, rot=0.0, jitter=0.0, rnd=None):
    pts = []
    for i in range(n):
        a = rot + 2 * math.pi * i / n
        rr = r * (1 + (rnd.uniform(-jitter, jitter) if rnd else 0))
        pts.append((cx + rr * math.cos(a), y, cz + rr * math.sin(a)))
    return pts


def cone(m, cx, cz, r, y0, y1, n, palette, rnd, rot=0.0):
    base = ring(cx, cz, r, y0, n, rot)
    apex = (cx, y1, cz)
    inside = (cx, (y0 + y1) / 2, cz)
    for i in range(n):
        m.tri(base[i], base[(i + 1) % n], apex, vary(palette, rnd), inside)
    centre = (cx, y0, cz)
    for i in range(n):  # the underside, so a cone over a trunk has no hole
        m.tri(base[i], base[(i + 1) % n], centre, palette[1], inside)


def prism(m, cx, cz, r, y0, y1, n, colour):
    lo, hi = ring(cx, cz, r, y0, n), ring(cx, cz, r, y1, n)
    inside = (cx, (y0 + y1) / 2, cz)
    for i in range(n):
        j = (i + 1) % n
        m.quad(lo[i], lo[j], hi[j], hi[i], colour, inside)
    top = (cx, y1, cz)
    for i in range(n):
        m.tri(hi[i], hi[(i + 1) % n], top, colour, inside)


def pine(m, cx, cz, h, rnd):
    """A trunk and two stacked cones, about h tall."""
    prism(m, cx, cz, 0.035 * h + 0.01, 0.0, 0.25 * h, 4, TRUNK)
    cone(m, cx, cz, 0.20 * h, 0.16 * h, 0.66 * h, 6, PINE, rnd, rnd.uniform(0, 1))
    cone(m, cx, cz, 0.14 * h, 0.46 * h, h, 6, PINE, rnd, rnd.uniform(0, 1))


# The 20 faces of an icosahedron.
PHI = (1 + math.sqrt(5)) / 2
ICO = [(-1, PHI, 0), (1, PHI, 0), (-1, -PHI, 0), (1, -PHI, 0), (0, -1, PHI), (0, 1, PHI), (0, -1, -PHI), (0, 1, -PHI),
       (PHI, 0, -1), (PHI, 0, 1), (-PHI, 0, -1), (-PHI, 0, 1)]
ICO_F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]


def blob(m, cx, cy, cz, r, palette, rnd):
    """A low-poly round crown: an icosahedron a little squashed and bumpy."""
    pts = []
    for x, y, z in ICO:
        k = r / math.sqrt(1 + PHI * PHI) * (1 + rnd.uniform(-0.1, 0.1))
        pts.append((cx + x * k, cy + y * k * 0.9, cz + z * k))
    for a, b, c in ICO_F:
        m.tri(pts[a], pts[b], pts[c], vary(palette, rnd), (cx, cy, cz))


def round_tree(m, cx, cz, h, rnd):
    r = 0.30 * h
    prism(m, cx, cz, 0.04 * h + 0.01, 0.0, 0.5 * h, 4, TRUNK)
    blob(m, cx, h - r, cz, r, LEAF, rnd)


def forest(kind, rnd):
    m = Mesh()
    if kind == 'pines':
        for cx, cz, h in ((-0.28, 0.22, 0.8), (0.27, 0.2, 0.74), (0.0, 0.05, 0.9), (-0.22, -0.26, 0.95), (0.25, -0.24, 1.0)):
            pine(m, cx, cz, h, rnd)
    elif kind == 'round':
        for cx, cz, h in ((-0.27, 0.2, 0.72), (0.27, 0.24, 0.66), (0.02, 0.06, 0.8), (-0.24, -0.25, 0.9), (0.26, -0.22, 0.95)):
            round_tree(m, cx, cz, h, rnd)
    else:
        pine(m, -0.24, -0.2, 1.0, rnd)
        round_tree(m, 0.25, -0.22, 0.85, rnd)
        round_tree(m, -0.05, 0.08, 0.7, rnd)
        pine(m, 0.28, 0.24, 0.6, rnd)
        round_tree(m, -0.3, 0.26, 0.6, rnd)
    return m


def peak(m, cx, cz, r, h, rnd, rot, snow_from=0.62, sides=8):
    """A rocky peak: three jittered rings and an apex; the top part is snow."""
    levels = [(r, 0.0), (r * 0.66, h * 0.36), (r * 0.36, h * 0.68)]
    rings = [ring(cx, cz, rr, y, sides, rot + 0.3 * i, 0.16, rnd) for i, (rr, y) in enumerate(levels)]
    # The upper rings lean a little, so the peak is not a perfect cone.
    lean = (rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05))
    rings = [[(x + lean[0] * i, y, z + lean[1] * i) for x, y, z in rg] for i, rg in enumerate(rings)]
    apex = (cx + lean[0] * 3, h, cz + lean[1] * 3)
    inside = (cx, h * 0.3, cz)

    def colour(ys):
        y = sum(ys) / len(ys)
        if y > h * snow_from:
            return SNOW
        return vary(ROCK, rnd) if y > h * 0.1 else ROCK[1]
    for k in range(len(rings) - 1):
        lo, hi = rings[k], rings[k + 1]
        for i in range(sides):
            j = (i + 1) % sides
            m.tri(lo[i], lo[j], hi[j], colour([lo[i][1], lo[j][1], hi[j][1]]), inside)
            m.tri(lo[i], hi[j], hi[i], colour([lo[i][1], hi[j][1], hi[i][1]]), inside)
    for i in range(sides):
        a, b = rings[-1][i], rings[-1][(i + 1) % sides]
        m.tri(a, b, apex, SNOW if h * 0.9 > h * snow_from else vary(ROCK, rnd), inside)
    centre = (cx, 0.0, cz)
    for i in range(sides):
        m.tri(rings[0][i], rings[0][(i + 1) % sides], centre, ROCK[3], inside)


def mountain(kind, rnd):
    m = Mesh()
    if kind == 'peaks_a':    # a tall peak with a lower one beside it
        peak(m, 0.08, -0.05, 0.38, 1.0, rnd, 0.2)
        peak(m, -0.26, 0.2, 0.22, 0.52, rnd, 0.9, snow_from=0.85, sides=7)
    else:                    # two peaks side by side and a rock
        peak(m, -0.17, 0.0, 0.28, 0.85, rnd, 0.5)
        peak(m, 0.2, -0.04, 0.26, 0.72, rnd, 1.4)
        peak(m, 0.0, 0.3, 0.14, 0.26, rnd, 0.1, snow_from=2.0, sides=6)
    return m


BUILDERS = {'pines': lambda r: forest('pines', r), 'round': lambda r: forest('round', r), 'mixed': lambda r: forest('mixed', r),
            'peaks_a': lambda r: mountain('peaks_a', r), 'peaks_b': lambda r: mountain('peaks_b', r)}


# ---------------------------------------------------------------- glTF writing
def pad4(b, fill=b'\x00'):
    return b + fill * (-len(b) % 4)


def glb(mesh, name):
    pos, nor, col = [], [], []
    for a, b, c, colour in mesh.tris:
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        ln = math.sqrt(sum(x * x for x in n)) or 1.0
        n = tuple(x / ln for x in n)
        for p in (a, b, c):
            pos.append(p)
            nor.append(n)
            col.append(colour)
    count = len(pos)
    binary = bytearray()
    views, accessors = [], []

    def accessor(data, comp, typ, target, norm=False, mm=None):
        views.append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(data), 'target': target})
        binary.extend(pad4(data))
        acc = {'bufferView': len(views) - 1, 'componentType': comp, 'count': count, 'type': typ}
        if norm:
            acc['normalized'] = True
        if mm:
            acc['min'], acc['max'] = mm
        accessors.append(acc)
        return len(accessors) - 1

    attrs = {
        'POSITION': accessor(b''.join(struct.pack('<3f', *p) for p in pos), 5126, 'VEC3', 34962,
                             mm=([min(p[k] for p in pos) for k in range(3)], [max(p[k] for p in pos) for k in range(3)])),
        'NORMAL': accessor(b''.join(struct.pack('<3f', *p) for p in nor), 5126, 'VEC3', 34962),
        'COLOR_0': accessor(b''.join(struct.pack('<4B', *c, 255) for c in col), 5121, 'VEC4', 34962, norm=True),
    }
    index = accessor(struct.pack('<%dH' % count, *range(count)), 5123, 'SCALAR', 34963)
    doc = {
        'asset': {'version': '2.0', 'generator': 'OpenTacticsEngine tools/art/make_terrain_models.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'name': name, 'mesh': 0}],
        'meshes': [{'name': name, 'primitives': [{'attributes': attrs, 'indices': index, 'material': 0, 'mode': 4}]}],
        'materials': [{'name': 'body', 'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0], 'metallicFactor': 0.0,
                                                               'roughnessFactor': 1.0}, 'doubleSided': False}],
        'accessors': accessors, 'bufferViews': views, 'buffers': [{'byteLength': len(binary)}],
    }
    js = pad4(json.dumps(doc, separators=(',', ':')).encode(), b' ')
    body = struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(binary), 0x004E4942) + bytes(binary)
    return b'glTF' + struct.pack('<II', 2, 12 + len(body)) + body


def stand_looks():
    """The "stand" of the forest and the mountain in the detailed style (heights in squares)."""
    return {
        'forest': {'variants': ['%s/%s.glb' % (OUT, f) for f in FOREST_FILES], 'height': 1.0, 'scatter': True},
        'mountain': {'variants': ['%s/%s.glb' % (OUT, f) for f in MOUNTAIN_FILES], 'height': 1.3, 'scatter': True},
    }


def write_all(repo):
    game = os.path.join(repo, GAME)
    out = os.path.join(game, OUT)
    os.makedirs(out, exist_ok=True)
    for stem, (kind, seed) in MODELS.items():
        mesh = BUILDERS[kind](random.Random(seed))
        with open(os.path.join(out, stem + '.glb'), 'wb') as f:
            f.write(glb(mesh, stem))
        print('  %s: %d triangles' % (stem, len(mesh.tris)))
    # Into the style's own piece of graphics.json (the ground pictures there stay as they are).
    path = os.path.join(game, 'art', 'detailed.style.json')
    with open(path, encoding='utf-8') as f:
        lines = f.read().splitlines(True)
    head = ''.join(l for l in lines if l.lstrip().startswith('//'))
    piece = json.loads(''.join(l for l in lines if not l.lstrip().startswith('//')))
    terrain = piece['styles']['detailed-3d']['terrain']
    for tid, stand in stand_looks().items():
        terrain[tid]['stand'] = stand
    with open(path, 'w', encoding='utf-8') as f:
        f.write(head + json.dumps(piece, indent=2) + '\n')


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    write_all(repo)


if __name__ == '__main__':
    sys.dont_write_bytecode = True
    main()
