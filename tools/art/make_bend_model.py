"""The skinned, animated glTF test model tests/fixtures/models/bend.glb (called by make_test_model.py).

An original, tiny model: a square column 3 units tall (9 rings of 4 corners plus a top and a bottom cap: 44
vertices) bent by a chain of three joints (root at the bottom, middle 1 up, tip 2 up). A node "base" above the
skeleton moves and turns everything (translation (1, 0, 2), a quarter turn about Y); the column's vertices are
stored in the model's own space (base transform already applied), as glTF skinning wants them.

Animations: "Bend" (rotation of middle and tip, LINEAR, 3 keys over 1 s, plus a channel for a node that is not in
the scene), "Hop" (root translation LINEAR, a STEP scale track on the mesh node), "Spline" (a CUBICSPLINE
rotation of the middle joint, 2 s).

Pure Python standard library; deterministic.
"""
import json
import math
import struct

COLUMN_HALF = 0.2
HEIGHT = 3.0
RINGS = 9
BASE_T = (1.0, 0.0, 2.0)
BASE_ANGLE = math.pi / 2  # about Y


def mul(a, b):  # 4 x 4, column-major lists
    return [sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4)) for c in range(4) for r in range(4)]


def translate(x, y, z):
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1]


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]


def rigid_inverse(m):
    r = [[m[c * 4 + row] for c in range(3)] for row in range(3)]  # r[row][col]
    t = [m[12], m[13], m[14]]
    out = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    for row in range(3):
        for col in range(3):
            out[col * 4 + row] = r[col][row]  # transpose
        out[12 + row] = -sum(r[col][row] * t[col] for col in range(3))
    return out


def point(m, p):
    return [m[0 + i] * p[0] + m[4 + i] * p[1] + m[8 + i] * p[2] + m[12 + i] for i in range(3)]


def direction(m, d):
    return [m[0 + i] * d[0] + m[4 + i] * d[1] + m[8 + i] * d[2] for i in range(3)]


def quat_z(angle):
    return [0.0, 0.0, math.sin(angle / 2), math.cos(angle / 2)]


def pad4(b, fill=b'\x00'):
    return b + fill * (-len(b) % 4)


def weights_at(y):
    """(joint a, joint b, weight a, weight b): smooth between neighbouring joints at y = 0, 1, 2."""
    if y >= 2:
        return 2, 2, 1.0, 0.0
    j = int(y)
    f = y - j
    return j, j + 1, 1 - f, f


def column():
    base = mul(translate(*BASE_T), rot_y(BASE_ANGLE))
    corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    verts = []  # (position, normal, joints, weights) in model space
    for k in range(RINGS):
        y = HEIGHT * k / (RINGS - 1)
        ja, jb, wa, wb = weights_at(y)
        for cx, cz in corners:
            n = (cx * math.sqrt(0.5), 0.0, cz * math.sqrt(0.5))
            verts.append((point(base, (cx * COLUMN_HALF, y, cz * COLUMN_HALF)), direction(base, n), (ja, jb, 0, 0), (wa, wb, 0, 0)))
    for y, ny, j in ((0.0, -1.0, 0), (HEIGHT, 1.0, 2)):  # bottom and top caps
        for cx, cz in corners:
            verts.append((point(base, (cx * COLUMN_HALF, y, cz * COLUMN_HALF)), direction(base, (0, ny, 0)), (j, 0, 0, 0),
                          (1.0, 0.0, 0.0, 0.0)))
    quads = []
    for k in range(RINGS - 1):
        for f in range(4):
            a, b = k * 4 + f, k * 4 + (f + 1) % 4
            quads.append((a, b, b + 4, a + 4))
    cap0, cap1 = RINGS * 4, RINGS * 4 + 4
    quads.append((cap0, cap0 + 1, cap0 + 2, cap0 + 3))
    quads.append((cap1, cap1 + 1, cap1 + 2, cap1 + 3))
    idx = []
    for q in quads:
        for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
            p = [verts[i][0] for i in tri]
            u = [p[1][m] - p[0][m] for m in range(3)]
            w = [p[2][m] - p[0][m] for m in range(3)]
            g = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
            if sum(g[m] * sum(verts[i][1][m] for i in tri) for m in range(3)) < 0:
                tri = (tri[0], tri[2], tri[1])  # front face outward
            idx.extend(tri)
    return base, verts, idx


def build():
    base, verts, idx = column()
    half = BASE_ANGLE / 2
    binary = bytearray()
    views, accessors = [], []

    def add(data, count, ctype, kind, norm=False, target=None, extra=None):
        view = dict(buffer=0, byteOffset=len(binary), byteLength=len(data))
        if target:
            view['target'] = target
        views.append(view)
        binary.extend(pad4(data))
        acc = dict(bufferView=len(views) - 1, componentType=ctype, count=count, type=kind)
        if norm:
            acc['normalized'] = True
        acc.update(extra or {})
        accessors.append(acc)
        return len(accessors) - 1

    n = len(verts)
    pos = add(b''.join(struct.pack('<3f', *v[0]) for v in verts), n, 5126, 'VEC3', target=34962,
              extra=dict(min=[min(v[0][m] for v in verts) for m in range(3)], max=[max(v[0][m] for v in verts) for m in range(3)]))
    nrm = add(b''.join(struct.pack('<3f', *v[1]) for v in verts), n, 5126, 'VEC3', target=34962)
    jnt = add(b''.join(struct.pack('<4B', *v[2]) for v in verts), n, 5121, 'VEC4', target=34962)
    # Normalised 16-bit weights: they only add up to about 1, the loader evens them out.
    wgt = add(b''.join(struct.pack('<4H', *[round(w * 65535) for w in v[3]]) for v in verts), n, 5123, 'VEC4', norm=True, target=34962)
    ind = add(struct.pack('<%dH' % len(idx), *idx), len(idx), 5123, 'SCALAR', target=34963)

    bind = [base, mul(base, translate(0, 1, 0)), mul(base, translate(0, 2, 0))]
    ibm = add(b''.join(struct.pack('<16f', *rigid_inverse(m)) for m in bind), 3, 5126, 'MAT4')

    def times(ts):
        return add(struct.pack('<%df' % len(ts), *ts), len(ts), 5126, 'SCALAR', extra=dict(min=[min(ts)], max=[max(ts)]))

    def vec(vs, kind):
        flat = [x for v in vs for x in v]
        return add(struct.pack('<%df' % len(flat), *flat), len(vs), 5126, kind)

    deg = math.radians
    t3 = [0.0, 0.5, 1.0]
    bend_mid = vec([quat_z(deg(a)) for a in (0, 45, 90)], 'VEC4')
    bend_tip = vec([quat_z(deg(a)) for a in (0, 30, 60)], 'VEC4')
    hop_t = vec([(0, 0, 0), (0, 0.5, 0), (0, 0, 0)], 'VEC3')
    step_in = times([0.0, 0.25, 0.75])
    step_s = vec([(1, 1, 1), (1, 2, 1), (1, 1, 3)], 'VEC3')
    orphan_in = times([0.0, 2.0])
    orphan_t = vec([(0, 0, 0), (5, 0, 0)], 'VEC3')
    spline_in = times([0.0, 1.0, 2.0])
    tang = (0.1, 0.2, 0.3, 0.4)  # arbitrary tangents: the loader must ignore them
    spline_out = vec([tang, quat_z(0), tang, tang, quat_z(deg(80)), tang, tang, quat_z(deg(-40)), tang], 'VEC4')
    bend_in = times(t3)

    anims = [
        {'name': 'Bend',
         'samplers': [{'input': bend_in, 'output': bend_mid, 'interpolation': 'LINEAR'},
                      {'input': bend_in, 'output': bend_tip},
                      {'input': orphan_in, 'output': orphan_t}],
         'channels': [{'sampler': 0, 'target': {'node': 2, 'path': 'rotation'}},
                      {'sampler': 1, 'target': {'node': 3, 'path': 'rotation'}},
                      {'sampler': 2, 'target': {'node': 5, 'path': 'translation'}}]},
        {'name': 'Hop',
         'samplers': [{'input': bend_in, 'output': hop_t, 'interpolation': 'LINEAR'},
                      {'input': step_in, 'output': step_s, 'interpolation': 'STEP'}],
         'channels': [{'sampler': 0, 'target': {'node': 1, 'path': 'translation'}},
                      {'sampler': 1, 'target': {'node': 4, 'path': 'scale'}}]},
        {'name': 'Spline',
         'samplers': [{'input': spline_in, 'output': spline_out, 'interpolation': 'CUBICSPLINE'}],
         'channels': [{'sampler': 0, 'target': {'node': 2, 'path': 'rotation'}}]},
    ]
    doc = {
        'asset': {'version': '2.0', 'generator': 'OpenTacticsEngine tools/art/make_test_model.py'},
        'scene': 0,
        'scenes': [{'nodes': [0]}],
        'nodes': [
            {'name': 'base', 'translation': list(BASE_T), 'rotation': [0.0, math.sin(half), 0.0, math.cos(half)], 'children': [1, 4]},
            {'name': 'root', 'children': [2]},
            {'name': 'middle', 'translation': [0.0, 1.0, 0.0], 'children': [3]},
            {'name': 'tip', 'translation': [0.0, 1.0, 0.0]},
            {'name': 'column', 'mesh': 0, 'skin': 0},
            {'name': 'unused'},  # not in the scene: its animation channel is skipped
        ],
        'skins': [{'name': 'chain', 'joints': [1, 2, 3], 'inverseBindMatrices': ibm, 'skeleton': 1}],
        'meshes': [{'name': 'column', 'primitives': [{'attributes': {'POSITION': pos, 'NORMAL': nrm, 'JOINTS_0': jnt, 'WEIGHTS_0': wgt},
                                                       'indices': ind, 'material': 0}]}],
        'materials': [{'name': 'clay', 'pbrMetallicRoughness': {'baseColorFactor': [0.9, 0.55, 0.3, 1.0], 'metallicFactor': 0.0}}],
        'animations': anims,
        'accessors': accessors,
        'bufferViews': views,
        'buffers': [{'byteLength': len(binary)}],
    }
    return doc, bytes(binary)
