#!/usr/bin/env python3
"""Writes the engine's glTF test fixtures (tests/fixtures/models/) and
assets/models/figure.glb (shown by the player's 3D test scene).

usage: python3 tools/art/make_test_model.py [REPO_ROOT]

An original, tiny model: a pawn-like figure (a revolved profile, interleaved
vertex data, an 8 x 8 checker texture) with a child node "hat" (a box with no
normals, a see-through unlit colour). Written twice with identical content:
figure.glb (texture embedded) and figure.gltf + figure.bin + figure_tex.png.
Also grey200.jpg, an 8 x 8 flat grey JPEG for the image-decoder test (a
hand-built single-block baseline JPEG: no JPEG library needed).

Pure Python standard library; no input files. Re-running reproduces identical
files.
"""
import json
import math
import os
import struct
import sys
import zlib

SEGMENTS = 16
# (radius, height) from the bottom up: base disk, body, collar, head.
PROFILE = [(0.0, 0.0), (0.45, 0.0), (0.45, 0.12), (0.3, 0.25), (0.18, 0.6), (0.18, 0.85),
           (0.3, 0.9), (0.3, 0.95), (0.2, 1.0), (0.25, 1.15), (0.25, 1.35), (0.15, 1.5), (0.0, 1.55)]
TOP = PROFILE[-1][1]


def texel(x, y):
    if (x, y) == (7, 7):
        return (0, 0, 255, 255)
    if (x // 2 + y // 2) % 2 == 0:
        return (200, 40, 40, 255)
    return (240, 220, 60, 255)


def make_png():
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)
    rows = b''.join(b'\x00' + bytes(c for x in range(8) for c in texel(x, y)) for y in range(8))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 8, 8, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def make_jpeg():
    """8 x 8 grey (200) baseline JPEG: one block, DC only. Quantiser 1, so DC = (200 - 128) * 8 = 576."""
    def seg(marker, payload):
        return b'\xff' + bytes([marker]) + struct.pack('>H', len(payload) + 2) + payload
    dqt = seg(0xDB, b'\x00' + bytes([1] * 64))
    sof = seg(0xC0, struct.pack('>BHHB', 8, 8, 8, 1) + bytes([1, 0x11, 0]))
    # DC table: 12 symbols (categories 0..11) with 4-bit codes 0..11. AC table: just end-of-block, code "0".
    dht_dc = seg(0xC4, b'\x00' + bytes([0, 0, 0, 12] + [0] * 12) + bytes(range(12)))
    dht_ac = seg(0xC4, b'\x10' + bytes([1] + [0] * 15) + b'\x00')
    sos = seg(0xDA, bytes([1, 1, 0x00, 0, 63, 0]))
    bits = format(10, '04b') + format(576, '010b') + '0'  # category 10, the 10 value bits, end of block
    bits += '1' * (-len(bits) % 8)
    scan = bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))
    scan = scan.replace(b'\xff', b'\xff\x00')
    return b'\xff\xd8' + dqt + sof + dht_dc + dht_ac + sos + scan + b'\xff\xd9'


def pawn():
    """Interleaved vertices (position, normal, uv: 8 floats each) and 16-bit triangle indices."""
    seg_normals = []
    for (r0, y0), (r1, y1) in zip(PROFILE, PROFILE[1:]):
        dr, dy = r1 - r0, y1 - y0
        n = math.hypot(dr, dy)
        seg_normals.append((dy / n, -dr / n))  # (radial, up)
    ring_normals = []
    for i in range(len(PROFILE)):
        near = seg_normals[max(i - 1, 0):i + 1]
        rad = sum(n[0] for n in near) / len(near)
        up = sum(n[1] for n in near) / len(near)
        n = math.hypot(rad, up)
        ring_normals.append((rad / n, up / n))
    verts = []
    for i, (r, y) in enumerate(PROFILE):
        for j in range(SEGMENTS + 1):
            a = 2 * math.pi * j / SEGMENTS
            c, s = math.cos(a), -math.sin(a)
            rad, up = ring_normals[i]
            verts.append((r * c, y, r * s, rad * c, up, rad * s, 2.0 * j / SEGMENTS, 1.0 - y / TOP))
    idx = []
    for i in range(len(PROFILE) - 1):
        for j in range(SEGMENTS):
            a, b = i * (SEGMENTS + 1) + j, i * (SEGMENTS + 1) + j + 1
            c, d = b + SEGMENTS + 1, a + SEGMENTS + 1
            for tri in ((a, b, c), (a, c, d)):
                p = [verts[k] for k in tri]
                u = [p[1][m] - p[0][m] for m in range(3)]
                w = [p[2][m] - p[0][m] for m in range(3)]
                g = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
                if max(abs(x) for x in g) < 1e-9:
                    continue  # a sliver at a pole
                if sum(g[m] * sum(v[3 + m] for v in p) for m in range(3)) < 0:
                    tri = (tri[0], tri[2], tri[1])  # make the front face point outward
                idx.extend(tri)
    return verts, idx


# A box, 8 shared corners, 12 triangles (counter-clockwise from outside).
BOX_POS = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
BOX_IDX = [4, 5, 6, 4, 6, 7, 1, 0, 3, 1, 3, 2, 5, 1, 2, 5, 2, 6, 0, 4, 7, 0, 7, 3, 7, 6, 2, 7, 2, 3, 0, 1, 5, 0, 5, 4]


def pad4(b, fill=b'\x00'):
    return b + fill * (-len(b) % 4)


def build(embed_png, png):
    verts, idx = pawn()
    vb = b''.join(struct.pack('<8f', *v) for v in verts)
    ib = struct.pack('<%dH' % len(idx), *idx)
    box = [(x * 0.2, y * 0.1 + 0.1, z * 0.2) for x, y, z in BOX_POS]
    pb = b''.join(struct.pack('<3f', *p) for p in box)
    xb = bytes(BOX_IDX)
    binary = bytearray()
    views = []

    def add(data, **extra):
        views.append(dict(buffer=0, byteOffset=len(binary), byteLength=len(data), **extra))
        binary.extend(pad4(data))
        return len(views) - 1

    v_vert = add(vb, byteStride=32, target=34962)
    v_idx = add(ib, target=34963)
    v_pos = add(pb, target=34962)
    v_box = add(xb, target=34963)
    v_png = add(png) if embed_png else None

    lo = [min(v[m] for v in verts) for m in range(3)]
    hi = [max(v[m] for v in verts) for m in range(3)]
    accessors = [
        dict(bufferView=v_vert, byteOffset=0, componentType=5126, count=len(verts), type='VEC3', min=lo, max=hi),
        dict(bufferView=v_vert, byteOffset=12, componentType=5126, count=len(verts), type='VEC3'),
        dict(bufferView=v_vert, byteOffset=24, componentType=5126, count=len(verts), type='VEC2'),
        dict(bufferView=v_idx, componentType=5123, count=len(idx), type='SCALAR'),
        dict(bufferView=v_pos, componentType=5126, count=8, type='VEC3',
             min=[min(p[m] for p in box) for m in range(3)], max=[max(p[m] for p in box) for m in range(3)]),
        dict(bufferView=v_box, componentType=5121, count=len(BOX_IDX), type='SCALAR'),
    ]
    image = dict(mimeType='image/png', bufferView=v_png) if embed_png else dict(uri='figure_tex.png')
    doc = {
        'asset': {'version': '2.0', 'generator': 'OpenTacticsEngine tools/art/make_test_model.py'},
        'extensionsUsed': ['KHR_materials_unlit'],
        'scene': 0,
        'scenes': [{'nodes': [0]}],
        'nodes': [
            {'name': 'pawn', 'mesh': 0, 'children': [1]},
            {'name': 'hat', 'mesh': 1, 'translation': [0.0, 1.55, 0.25],
             'rotation': [0.0, math.sin(math.radians(15)), 0.0, math.cos(math.radians(15))]},
        ],
        'meshes': [
            {'name': 'pawn', 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2},
                                             'indices': 3, 'material': 0}]},
            {'name': 'hat', 'primitives': [{'attributes': {'POSITION': 4}, 'indices': 5, 'material': 1}]},
        ],
        'materials': [
            {'name': 'checker', 'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0],
                                                         'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0}},
            {'name': 'glass', 'alphaMode': 'BLEND', 'doubleSided': True,
             'pbrMetallicRoughness': {'baseColorFactor': [0.2, 0.6, 1.0, 0.5]},
             'extensions': {'KHR_materials_unlit': {}}},
        ],
        'textures': [{'sampler': 0, 'source': 0}],
        'samplers': [{'magFilter': 9728, 'minFilter': 9728, 'wrapS': 10497, 'wrapT': 33648}],
        'images': [image],
        'accessors': accessors,
        'bufferViews': views,
        'buffers': [{'byteLength': len(binary)}],
    }
    return doc, bytes(binary)


def write_glb(path, doc, binary):
    js = pad4(json.dumps(doc, separators=(',', ':')).encode(), b' ')
    body = struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(binary), 0x004E4942) + binary
    with open(path, 'wb') as f:
        f.write(b'glTF' + struct.pack('<II', 2, 12 + len(body)) + body)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    out = os.path.join(root, 'tests', 'fixtures', 'models')
    os.makedirs(out, exist_ok=True)
    png = make_png()

    doc, binary = build(True, png)
    write_glb(os.path.join(out, 'figure.glb'), doc, binary)
    # The same file for the player's 3D test scene (--test-3d).
    os.makedirs(os.path.join(root, 'assets', 'models'), exist_ok=True)
    write_glb(os.path.join(root, 'assets', 'models', 'figure.glb'), doc, binary)

    doc, binary = build(False, png)
    doc['buffers'][0]['uri'] = 'figure.bin'
    with open(os.path.join(out, 'figure.gltf'), 'w') as f:
        json.dump(doc, f, indent=1)
        f.write('\n')
    with open(os.path.join(out, 'figure.bin'), 'wb') as f:
        f.write(binary)
    with open(os.path.join(out, 'figure_tex.png'), 'wb') as f:
        f.write(png)
    with open(os.path.join(out, 'grey200.jpg'), 'wb') as f:
        f.write(make_jpeg())
    print('wrote', out)


if __name__ == '__main__':
    sys.dont_write_bytecode = True
    main()
