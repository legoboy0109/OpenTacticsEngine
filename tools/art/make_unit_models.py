#!/usr/bin/env python3
"""Writes the demo game's 3D models: games/demo-srpg/models/<unit id>.glb for the
seven unit types and fort.glb (a static stone tower).

usage: python3 tools/art/make_unit_models.py [REPO_ROOT]
       python3 tools/art/make_unit_models.py --check [REPO_ROOT]
       python3 tools/art/make_unit_models.py --pose CLIP:TIME OUT.glb UNIT [REPO_ROOT]

Original low-poly "block figures": every part is a box or a frustum (a box
whose top face is scaled), with flat colours taken from the 2D sprite palette
(tools/art/art_units.py). Faces point +Z, feet stand on Y = 0, centred in X/Z.

glTF layout of a unit:
  - one mesh with two primitives: material `body` (vertex colours carry the
    colour) and material `side` (white-ish vertex colours: the game tints it
    with the player's side colour); no textures;
  - one skin; rigid skinning (each vertex on exactly one joint, weight 1);
  - joint nodes are listed first (root first), then the mesh node, which
    references the skin. Rest pose = identity rotations, the vertex data is
    in the rest pose;
  - animations Idle, Move (loops), Attack, Die (one-shots): translation and
    rotation channels, LINEAR.
Rotation conventions used in this file: Rx(+) tips +Y towards +Z (a hanging
arm swings BACKWARDS, a standing weapon tilts FORWARD); a hanging arm swings
forward with negative x.

--check re-reads the written files and asserts their structure.
--pose renders a unit skeleton at CLIP:TIME as a static, unskinned .glb (same
maths as the engine's skinning) so it can be looked at in the 3D test scene.

Pure Python standard library; deterministic.
"""
import json
import math
import os
import struct
import sys

# ---------------------------------------------------------------- palette (art_units.py)
def rgb(h):
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255)


SKIN, SKIN_D = rgb(0xEBBE90), rgb(0xC99468)
STEEL, STEEL_D, STEEL_L = rgb(0xA9B3C4), rgb(0x77839A), rgb(0xDCE3EE)
LEATHER, LEATHER_D = rgb(0x92623A), rgb(0x5F3E24)
CLOTH, CLOTH_D = rgb(0xCFC7AE), rgb(0xA39B84)
GREY, GREY_D = rgb(0x8E919C), rgb(0x626571)
GOLD = rgb(0xE0B030)
WOOD = rgb(0xA67540)
HORSE, HORSE_D, HORSE_L = rgb(0xB28A5E), rgb(0x80603E), rgb(0xD2B088)
WHITE = rgb(0xF3F3F0)
OLIVE, OLIVE_D = rgb(0x7E8B67), rgb(0x5A6648)
EYE = rgb(0x30303A)
BLOND = rgb(0xD8B24A)
BROWN_HAIR = rgb(0x6B3F22)
RED_BAND = rgb(0xC8483A)
PEG_SHADE, PEG_WING = rgb(0xD6DDE6), rgb(0xE8EEF5)
# fort (art_tiles.py STONE: base, dark, light)
STONE, STONE_D, STONE_L = rgb(0xB9B3A2), rgb(0x8E8878), rgb(0xD8D3C4)
SIDEW = (255, 255, 255)      # vertex colour of side-material parts
SIDEG = (216, 216, 216)      # light grey trim on side parts

BODY, SIDE = 'body', 'side'
MATERIALS = [BODY, SIDE]

# ---------------------------------------------------------------- small maths
IDENT = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]


def mmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def mtrans(x, y, z):
    return [[1, 0, 0, x], [0, 1, 0, y], [0, 0, 1, z], [0, 0, 0, 1]]


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qaxis(axis, deg):
    h = math.radians(deg) / 2
    s = math.sin(h)
    v = [0, 0, 0]
    v[axis] = s
    return (v[0], v[1], v[2], math.cos(h))


def qeuler(x=0.0, y=0.0, z=0.0):
    """Degrees; the rotation is x first, then y, then z (in the fixed frame)."""
    return qmul(qaxis(2, z), qmul(qaxis(1, y), qaxis(0, x)))


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)


def qmat(q, t=(0, 0, 0)):
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), t[0]],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), t[1]],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), t[2]],
            [0, 0, 0, 1]]


def mpoint(m, p):
    return tuple(m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3))


def mdir(m, p):
    return tuple(m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] for i in range(3))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def cross(u, w):
    return (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])


def rot_about(pivot, x=0.0, y=0.0, z=0.0):
    return mmul(mtrans(*pivot), mmul(qmat(qeuler(x, y, z)), mtrans(-pivot[0], -pivot[1], -pivot[2])))


# ---------------------------------------------------------------- boxes
CORNERS = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
# Triangle corner indices, counter-clockwise seen from outside: +Z, -Z, +X, -X, +Y, -Y.
TRIS = [(4, 5, 6), (4, 6, 7), (1, 0, 3), (1, 3, 2), (5, 1, 2), (5, 2, 6),
        (0, 4, 7), (0, 7, 3), (7, 6, 2), (7, 2, 3), (0, 1, 5), (0, 5, 4)]


class Model:
    """Joints plus a triangle soup (flat normals, one colour per triangle)."""

    def __init__(self, name):
        self.name = name
        self.joints = []        # (name, parent index or -1, rest world position)
        self.tris = []          # (material, [3 x (pos, normal)], colour, joint index or -1)
        self.clips = {}         # name -> Clip

    def joint(self, name, parent, pos):
        self.joints.append((name, -1 if parent is None else self.jindex(parent), pos))

    def jindex(self, name):
        for i, j in enumerate(self.joints):
            if j[0] == name:
                return i
        raise KeyError(name)

    def box(self, joint, center, size, color, mat=BODY, top=(1, 1), shift=(0, 0), rot=None, pivot=None):
        """A box (or frustum: `top` scales the top face, `shift` slides it) with its centre at `center`
        (world rest position). `rot` = (x, y, z) degrees, about `pivot` (default: the centre)."""
        sx, sy, sz = size
        pts = []
        for cx, cy, cz in CORNERS:
            if cy > 0:
                pts.append((cx * sx / 2 * top[0] + shift[0], sy / 2, cz * sz / 2 * top[1] + shift[1]))
            else:
                pts.append((cx * sx / 2, -sy / 2, cz * sz / 2))
        pts = [add(p, center) for p in pts]
        if rot:
            m = rot_about(pivot if pivot else center, *rot)
            pts = [mpoint(m, p) for p in pts]
        if mat == SIDE and color not in (SIDEW, SIDEG):
            color = SIDEW
        j = -1 if joint is None else self.jindex(joint)
        for a, b, c in TRIS:
            p = (pts[a], pts[b], pts[c])
            n = cross(sub(p[1], p[0]), sub(p[2], p[0]))
            ln = math.sqrt(sum(x * x for x in n))
            if ln < 1e-9:
                continue            # a collapsed face of a pointed frustum
            n = tuple(x / ln for x in n)
            self.tris.append((mat, [(q, n) for q in p], color, j))


# ---------------------------------------------------------------- animation
class Clip:
    """Keys as offsets from the rest pose: rot (degrees, rest = identity), pos (added to the rest position)."""

    def __init__(self, model, duration, loop):
        self.model, self.duration, self.loop = model, duration, loop
        self.rots = {}      # joint -> {time: (x, y, z)}
        self.poss = {}
        self.xrot = {}      # (joint, time) -> x rotation, for wrist compensation

    def rot(self, joint, t, x=0.0, y=0.0, z=0.0):
        self.rots.setdefault(joint, {})[t] = (x, y, z)
        self.xrot[(joint, t)] = x

    def pos(self, joint, t, dx=0.0, dy=0.0, dz=0.0):
        self.poss.setdefault(joint, {})[t] = (dx, dy, dz)

    def wrist(self, joint, parent, t, world=None, alpha=0.0, spine='spine'):
        """Sets a held part's (weapon / bow) joint so that it points `world` degrees (about X, 0 = straight up,
        90 = forward) given the arm and spine rotations already keyed at time t. world=None: keeps its rest tilt."""
        carried = self.xrot.get((parent, t), 0.0) + self.xrot.get((spine, t), 0.0)
        w = alpha if world is None else world
        self.rot(joint, t, x=w - alpha - carried)

    def tracks(self):
        """-> {joint: {'translation': [(t, vec)], 'rotation': [(t, quat)]}} with absolute values, first key at 0,
        last at the duration (a loop's last key repeats its first)."""
        out = {}
        for joint, keys in self.rots.items():
            ks = dict(keys)
            ks = self._close(ks, (0.0, 0.0, 0.0))
            seq = [(t, qeuler(*ks[t])) for t in sorted(ks)]
            for i in range(1, len(seq)):         # shortest way round
                if sum(a * b for a, b in zip(seq[i - 1][1], seq[i][1])) < 0:
                    seq[i] = (seq[i][0], tuple(-c for c in seq[i][1]))
            out.setdefault(joint, {})['rotation'] = seq
        for joint, keys in self.poss.items():
            rest = self.model.joints[self.model.jindex(joint)]
            base = rest[2] if rest[1] < 0 else sub(rest[2], self.model.joints[rest[1]][2])
            ks = self._close(dict(keys), (0.0, 0.0, 0.0))
            out.setdefault(joint, {})['translation'] = [(t, add(base, ks[t])) for t in sorted(ks)]
        return out

    def _close(self, ks, rest):
        if 0.0 not in ks:
            ks[0.0] = rest
        if self.duration not in ks:
            ks[self.duration] = ks[0.0] if self.loop else ks[max(ks)]
        return ks


def sample(track, t, is_quat):
    if t <= track[0][0]:
        return track[0][1]
    if t >= track[-1][0]:
        return track[-1][1]
    for (t0, v0), (t1, v1) in zip(track, track[1:]):
        if t0 <= t <= t1:
            f = (t - t0) / (t1 - t0)
            v = tuple(a + (b - a) * f for a, b in zip(v0, v1))
            return qnorm(v) if is_quat else v


def skin_matrices(model, clip, t):
    """Joint world matrix x inverse bind matrix for every joint at time t of the clip (None: rest pose)."""
    tracks = clip.tracks() if clip else {}
    world = []
    for i, (name, parent, rpos) in enumerate(model.joints):
        tr = tracks.get(name, {})
        base = rpos if parent < 0 else sub(rpos, model.joints[parent][2])
        trans = sample(tr['translation'], t, False) if 'translation' in tr else base
        q = sample(tr['rotation'], t, True) if 'rotation' in tr else (0, 0, 0, 1)
        local = qmat(q, trans)
        world.append(local if parent < 0 else mmul(world[parent], local))
    return [mmul(world[i], mtrans(-j[2][0], -j[2][1], -j[2][2])) for i, j in enumerate(model.joints)]


def posed_tris(model, clip=None, t=0.0):
    mats = skin_matrices(model, clip, t)
    out = []
    for mat, verts, color, j in model.tris:
        m = mats[j] if j >= 0 else IDENT
        out.append((mat, [(mpoint(m, p), mdir(m, n)) for p, n in verts], color, j))
    return out


# ---------------------------------------------------------------- glTF writing
def pad4(b, fill=b'\x00'):
    return b + fill * (-len(b) % 4)


def build_gltf(model, tris=None, skinned=True):
    """-> (json doc, binary chunk). tris: the geometry to write (default: the rest pose)."""
    tris = tris if tris is not None else [(m, v, c, j) for m, v, c, j in model.tris]
    binary = bytearray()
    views, accessors = [], []

    def view(data, target=None):
        v = dict(buffer=0, byteOffset=len(binary), byteLength=len(data))
        if target:
            v['target'] = target
        views.append(v)
        binary.extend(pad4(data))
        return len(views) - 1

    def accessor(data, comp, count, typ, target=None, norm=False, mm=None):
        a = dict(bufferView=view(data, target), componentType=comp, count=count, type=typ)
        if norm:
            a['normalized'] = True
        if mm:
            a['min'], a['max'] = mm
        accessors.append(a)
        return len(accessors) - 1

    prims = []
    for mi, mname in enumerate(MATERIALS):
        sel = [t for t in tris if t[0] == mname]
        if not sel:
            continue
        pos, nor, col, joi = [], [], [], []
        for _, verts, color, j in sel:
            for p, n in verts:
                pos.append(p)
                nor.append(n)
                col.append(color)
                joi.append(max(j, 0))
        n = len(pos)
        attrs = {
            'POSITION': accessor(b''.join(struct.pack('<3f', *p) for p in pos), 5126, n, 'VEC3', 34962,
                                 mm=([min(p[k] for p in pos) for k in range(3)], [max(p[k] for p in pos) for k in range(3)])),
            'NORMAL': accessor(b''.join(struct.pack('<3f', *p) for p in nor), 5126, n, 'VEC3', 34962),
            'COLOR_0': accessor(b''.join(struct.pack('<4B', *c, 255) for c in col), 5121, n, 'VEC4', 34962, norm=True),
        }
        if skinned and model.joints:
            attrs['JOINTS_0'] = accessor(b''.join(struct.pack('<4B', j, 0, 0, 0) for j in joi), 5121, n, 'VEC4', 34962)
            attrs['WEIGHTS_0'] = accessor(b''.join(struct.pack('<4f', 1, 0, 0, 0) for _ in joi), 5126, n, 'VEC4', 34962)
        idx = accessor(struct.pack('<%dH' % n, *range(n)), 5123, n, 'SCALAR', 34963)
        prims.append({'attributes': attrs, 'indices': idx, 'material': mi, 'mode': 4})
    doc = {'asset': {'version': '2.0', 'generator': 'OpenTacticsEngine tools/art/make_unit_models.py'},
           'scene': 0, 'scenes': [{'nodes': [0]}]}
    materials = [{'name': m, 'pbrMetallicRoughness': {'baseColorFactor': [1.0, 1.0, 1.0, 1.0], 'metallicFactor': 0.0,
                                                      'roughnessFactor': 1.0}, 'doubleSided': False}
                 for m in MATERIALS]
    mesh = {'name': model.name, 'primitives': prims}
    if skinned and model.joints:
        nodes = []
        for i, (name, parent, rpos) in enumerate(model.joints):
            base = rpos if parent < 0 else sub(rpos, model.joints[parent][2])
            node = {'name': name, 'translation': [round(c, 6) for c in base]}
            kids = [k for k, j in enumerate(model.joints) if j[1] == i]
            if kids:
                node['children'] = kids
            nodes.append(node)
        nodes.append({'name': model.name, 'mesh': 0, 'skin': 0})
        doc['scenes'][0]['nodes'] = [0, len(nodes) - 1]
        ibm = b''.join(struct.pack('<16f', *[mtrans(-j[2][0], -j[2][1], -j[2][2])[r][c]
                                              for c in range(4) for r in range(4)]) for j in model.joints)
        ibm_acc = accessor(ibm, 5126, len(model.joints), 'MAT4')
        doc['skins'] = [{'name': model.name, 'skeleton': 0, 'joints': list(range(len(model.joints))),
                         'inverseBindMatrices': ibm_acc}]
        anims = []
        for cname, clip in model.clips.items():
            samplers, channels = [], []
            for jname, tr in sorted(clip.tracks().items(), key=lambda kv: model.jindex(kv[0])):
                for path in ('translation', 'rotation'):
                    if path not in tr:
                        continue
                    keys = tr[path]
                    times = [k[0] for k in keys]
                    tin = accessor(struct.pack('<%df' % len(times), *times), 5126, len(times), 'SCALAR',
                                   mm=([min(times)], [max(times)]))
                    flat = [c for k in keys for c in k[1]]
                    tout = accessor(struct.pack('<%df' % len(flat), *flat), 5126, len(keys),
                                    'VEC3' if path == 'translation' else 'VEC4')
                    samplers.append({'input': tin, 'output': tout, 'interpolation': 'LINEAR'})
                    channels.append({'sampler': len(samplers) - 1,
                                     'target': {'node': model.jindex(jname), 'path': path}})
            anims.append({'name': cname, 'samplers': samplers, 'channels': channels})
        doc['animations'] = anims
    else:
        nodes = [{'name': model.name, 'mesh': 0}]
    doc['nodes'] = nodes
    doc['meshes'] = [mesh]
    doc['materials'] = materials
    doc['accessors'] = accessors
    doc['bufferViews'] = views
    doc['buffers'] = [{'byteLength': len(binary)}]
    return doc, bytes(binary)


def glb_bytes(doc, binary):
    js = pad4(json.dumps(doc, separators=(',', ':')).encode(), b' ')
    binary = pad4(binary)
    body = struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(binary), 0x004E4942) + binary
    return b'glTF' + struct.pack('<II', 2, 12 + len(body)) + body


# ---------------------------------------------------------------- foot figures
HIP = 0.72


def foot_base(M, torso=STEEL, torso_mat=BODY, trim=STEEL_L, legc=GREY_D, boot=LEATHER_D, sleeve=STEEL,
              glove=SKIN, belt=GOLD, hair=BLOND, tw=0.54, td=0.30, helmet=None, arm_w=0.14, leg_w=0.2):
    """Joints root, spine, head, arm_l, arm_r, leg_l, leg_r and the plain body parts. Returns the right hand position."""
    sx = tw / 2 + 0.07
    M.joint('root', None, (0, HIP, 0))
    M.joint('spine', 'root', (0, HIP, 0))
    M.joint('head', 'spine', (0, 1.2, 0))
    M.joint('arm_l', 'spine', (sx, 1.12, 0))
    M.joint('arm_r', 'spine', (-sx, 1.12, 0))
    M.joint('leg_l', 'root', (0.13, HIP, 0))
    M.joint('leg_r', 'root', (-0.13, HIP, 0))
    for s, lj in ((1, 'leg_l'), (-1, 'leg_r')):
        M.box(lj, (s * 0.13, 0.43, 0), (leg_w, 0.58, 0.22), legc)
        M.box(lj, (s * 0.13, 0.075, 0.05), (leg_w + 0.02, 0.15, 0.34), boot)
    sc = SIDEW if torso_mat == SIDE else torso
    M.box('spine', (0, 0.95, 0), (tw, 0.46, td), sc, torso_mat)
    if trim is not None and torso_mat == BODY:
        M.box('spine', (0, 1.175, 0), (tw, 0.05, td), trim)
    M.box('spine', (0, 0.78, 0), (tw + 0.02, 0.08, td + 0.02), belt)
    for s, aj in ((1, 'arm_l'), (-1, 'arm_r')):
        M.box(aj, (s * sx, 0.96, 0), (arm_w, 0.32, 0.16), sleeve)
        M.box(aj, (s * sx, 0.74, 0.01), (arm_w - 0.02, 0.14, 0.14), glove)
    # head, facing +Z
    M.box('head', (0, 1.2, 0), (0.12, 0.06, 0.12), SKIN_D)
    M.box('head', (0, 1.37, 0), (0.3, 0.3, 0.3), SKIN)
    for s in (1, -1):
        M.box('head', (s * 0.07, 1.4, 0.155), (0.05, 0.05, 0.02), EYE)
    M.box('head', (0, 1.34, 0.165), (0.05, 0.06, 0.05), SKIN_D)
    if helmet is None:
        M.box('head', (0, 1.55, 0), (0.32, 0.09, 0.32), hair)
        M.box('head', (0, 1.4, -0.15), (0.32, 0.24, 0.06), hair)
    return (-sx, 0.72, 0.02)


def sword(M, joint, hand, alpha=35):
    h = hand
    r = (alpha, 0, 0)
    M.box(joint, add(h, (0, 0.02, 0)), (0.05, 0.13, 0.05), LEATHER_D, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.09, 0)), (0.2, 0.04, 0.06), GOLD, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.38, 0)), (0.075, 0.54, 0.025), STEEL_L, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.72, 0)), (0.075, 0.14, 0.025), STEEL_L, top=(0.05, 1), rot=r, pivot=h)


def spear(M, joint, hand, alpha=15, length=0.85, butt=0.3, tip=STEEL_L):
    h = hand
    r = (alpha, 0, 0)
    M.box(joint, add(h, (0, (length - butt) / 2, 0)), (0.05, length + butt, 0.05), WOOD, rot=r, pivot=h)
    M.box(joint, add(h, (0, length + 0.1, 0)), (0.08, 0.2, 0.03), tip, top=(0.05, 1), rot=r, pivot=h)
    M.box(joint, add(h, (0, length - 0.02, 0)), (0.09, 0.04, 0.05), GREY_D, rot=r, pivot=h)


def axe(M, joint, hand, alpha=20):
    h = hand
    r = (alpha, 0, 0)
    M.box(joint, add(h, (0, 0.28, 0)), (0.05, 0.8, 0.05), WOOD, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.55, 0.1)), (0.06, 0.26, 0.16), STEEL_D, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.55, 0.22)), (0.05, 0.3, 0.1), STEEL_L, top=(1, 0.6), rot=(alpha, 0, 0), pivot=h)
    M.box(joint, add(h, (0, 0.58, -0.04)), (0.06, 0.1, 0.07), STEEL_D, rot=r, pivot=h)


def lord():
    M = Model('lord')
    hand = foot_base(M, STEEL, trim=STEEL_L, sleeve=STEEL, glove=LEATHER_D, hair=BLOND)
    M.joint('weapon', 'arm_r', hand)
    M.box('spine', (0, 0.96, 0.165), (0.3, 0.44, 0.03), SIDEW, SIDE)           # tabard
    M.box('spine', (0, 0.76, 0.165), (0.3, 0.12, 0.03), SIDEG, SIDE)
    M.box('spine', (0, 0.84, -0.185), (0.46, 0.72, 0.05), SIDEW, SIDE, top=(1.12, 1))   # cape
    M.box('spine', (0, 1.2, -0.02), (0.38, 0.05, 0.36), GOLD)                   # collar
    for s, aj in ((1, 'arm_l'), (-1, 'arm_r')):
        M.box(aj, (s * 0.34, 1.17, 0), (0.2, 0.1, 0.22), STEEL_L)
    M.box('head', (0, 1.55, 0), (0.33, 0.05, 0.33), GOLD)                      # circlet
    M.box('head', (0, 1.4, -0.07), (0.32, 0.25, 0.2), BLOND)
    sword(M, 'weapon', hand, alpha=35)
    M.alpha = 35
    return M


def archer():
    M = Model('archer')
    hand = foot_base(M, OLIVE, trim=None, legc=OLIVE_D, boot=LEATHER_D, sleeve=OLIVE, glove=LEATHER, belt=LEATHER,
                     hair=OLIVE_D, helmet=True)
    M.joint('bow', 'arm_l', (0.34 + 0.07 - 0.0, 0.76, 0.06))
    M.box('head', (0, 1.55, 0.0), (0.36, 0.14, 0.36), SIDEW, SIDE)             # hood
    M.box('head', (0, 1.38, -0.17), (0.36, 0.3, 0.08), SIDEW, SIDE)
    for s in (1, -1):
        M.box('head', (s * 0.175, 1.42, 0.0), (0.04, 0.22, 0.34), SIDEG, SIDE)
    M.box('spine', (0, 1.14, 0), (0.66, 0.1, 0.4), SIDEW, SIDE, top=(0.8, 0.85))   # mantle
    M.box('spine', (0.1, 1.0, -0.2), (0.12, 0.42, 0.1), LEATHER_D, rot=(0, 0, -12))   # quiver
    for k in (-0.025, 0.025):
        M.box('spine', (0.145 + k * 0.4, 1.24, -0.2), (0.025, 0.1, 0.025), WHITE, rot=(0, 0, -12))
    # bow: held in the left hand, upright; limbs lean back (-Z) towards the archer
    h = (0.41, 0.76, 0.06)
    M.box('bow', add(h, (0, 0.0, 0.03)), (0.05, 0.22, 0.06), WOOD)
    M.box('bow', add(h, (0, 0.2, -0.02)), (0.07, 0.26, 0.07), WOOD, rot=(-22, 0, 0))
    M.box('bow', add(h, (0, -0.2, -0.02)), (0.07, 0.26, 0.07), WOOD, rot=(22, 0, 0))
    M.box('bow', add(h, (0, 0.0, -0.1)), (0.02, 0.74, 0.02), WHITE)           # string
    M.box('bow', add(h, (0, 0.05, 0.2)), (0.02, 0.02, 0.6), LEATHER_D)          # arrow
    M.box('bow', add(h, (0, 0.05, 0.53)), (0.04, 0.04, 0.08), STEEL_L, top=(0.05, 1), rot=(90, 0, 0))
    M.alpha = 0
    return M


def fighter():
    M = Model('fighter')
    hand = foot_base(M, LEATHER, trim=None, legc=LEATHER, boot=LEATHER_D, sleeve=SKIN, glove=SKIN, belt=LEATHER_D,
                     hair=BROWN_HAIR, tw=0.58)
    M.joint('weapon', 'arm_r', hand)
    M.box('spine', (0, 0.7, 0.0), (0.6, 0.2, 0.34), SIDEW, SIDE)                # sash / waistcloth
    M.box('spine', (0, 0.58, 0.17), (0.28, 0.34, 0.03), SIDEG, SIDE)
    M.box('head', (0, 1.46, 0), (0.33, 0.06, 0.33), SIDEW, SIDE)               # headband
    M.box('head', (0, 1.43, -0.18), (0.08, 0.06, 0.1), SIDEG, SIDE, rot=(25, 0, 0))   # knot
    for s in (1, -1):
        M.box('spine', (s * 0.21, 0.96, 0.15), (0.14, 0.44, 0.03), LEATHER_D)   # vest straps
    axe(M, 'weapon', hand, alpha=20)
    M.alpha = 20
    return M


def knight():
    M = Model('knight')
    hand = foot_base(M, STEEL, trim=STEEL_L, legc=STEEL_D, boot=GREY_D, sleeve=STEEL_D, glove=GREY_D, belt=GOLD,
                     tw=0.6, td=0.34, helmet=True, arm_w=0.17, leg_w=0.22)
    M.joint('weapon', 'arm_r', hand)
    M.box('head', (0, 1.38, 0), (0.35, 0.36, 0.35), STEEL)                    # great helm
    M.box('head', (0, 1.57, 0), (0.31, 0.04, 0.31), STEEL_L)
    M.box('head', (0, 1.42, 0.178), (0.26, 0.04, 0.02), (0x2A, 0x2A, 0x33))    # visor slit
    M.box('head', (0, 1.33, 0.178), (0.04, 0.12, 0.02), (0x2A, 0x2A, 0x33))
    M.box('head', (0, 1.6, -0.03), (0.07, 0.1, 0.26), SIDEW, SIDE, top=(1, 0.7))    # plume
    M.box('spine', (0, 0.9, 0.19), (0.34, 0.54, 0.03), SIDEW, SIDE)           # surcoat
    M.box('spine', (0, 0.84, -0.19), (0.34, 0.64, 0.03), SIDEG, SIDE)
    for s, aj in ((1, 'arm_l'), (-1, 'arm_r')):
        M.box(aj, (s * 0.37, 1.17, 0), (0.25, 0.13, 0.27), STEEL_L)           # pauldrons
    # tower shield on the left arm: side-coloured face with a gold cross
    sxc, syc, szc = 0.36, 0.9, 0.2
    M.box('arm_l', (sxc, syc, szc - 0.03), (0.4, 0.62, 0.04), GREY_D)
    M.box('arm_l', (sxc, syc, szc), (0.34, 0.56, 0.04), SIDEW, SIDE, )
    M.box('arm_l', (sxc, syc, szc + 0.025), (0.06, 0.46, 0.012), GOLD)
    M.box('arm_l', (sxc, syc + 0.08, szc + 0.025), (0.26, 0.06, 0.012), GOLD)
    spear(M, 'weapon', hand, alpha=12, length=0.75, tip=STEEL_L)
    M.alpha = 12
    return M


def soldier():
    M = Model('soldier')
    hand = foot_base(M, CLOTH, torso_mat=SIDE, legc=GREY_D, boot=LEATHER_D, sleeve=CLOTH_D, glove=LEATHER, belt=LEATHER,
                     hair=LEATHER_D, helmet=True)
    M.joint('weapon', 'arm_r', hand)
    M.box('head', (0, 1.55, 0), (0.34, 0.1, 0.34), STEEL)                      # helmet
    M.box('head', (0, 1.62, 0), (0.12, 0.06, 0.3), STEEL_L)
    M.box('head', (0, 1.45, -0.04), (0.33, 0.12, 0.28), STEEL_D)
    M.box('head', (0, 1.4, -0.15), (0.32, 0.22, 0.06), STEEL_D)
    M.box('spine', (0, 0.9, 0.0), (0.58, 0.06, 0.34), SIDEG, SIDE)           # trim hem
    for s in (1, -1):
        M.box('spine', (s * 0.17, 1.0, 0.16), (0.04, 0.34, 0.03), LEATHER_D, rot=(0, 0, s * 20))   # straps
    # round shield (a hexagon of three stacked blocks)
    cx, cy, cz = 0.36, 0.86, 0.2
    M.box('arm_l', (cx, cy, cz), (0.42, 0.26, 0.05), WOOD)
    for s in (1, -1):
        M.box('arm_l', (cx, cy + s * 0.15, cz), (0.42, 0.06, 0.05), WOOD, top=(0.55, 1), rot=(0, 0, 0 if s > 0 else 180),
              pivot=(cx, cy + s * 0.15, cz))
    M.box('arm_l', (cx, cy, cz + 0.03), (0.12, 0.12, 0.04), GOLD)
    M.box('arm_l', (cx, cy, cz - 0.04), (0.46, 0.4, 0.03), LEATHER_D)
    spear(M, 'weapon', hand, alpha=15, length=0.72)
    M.alpha = 15
    return M


# ---------------------------------------------------------------- mounted figures
def mount_base(M, horse, shade, hoof, mane, armour=STEEL, white_wings=False):
    """The horse (joints root, neck, four legs) plus the rider (spine, head, arms). Returns the right hand position."""
    M.joint('root', None, (0, 0.95, 0))
    M.joint('spine', 'root', (0, 1.22, 0))
    M.joint('head', 'spine', (0, 1.62, 0))
    M.joint('arm_l', 'spine', (0.31, 1.56, 0))
    M.joint('arm_r', 'spine', (-0.31, 1.56, 0))
    M.joint('neck', 'root', (0, 1.08, 0.38))
    legs = (('leg_fl', 0.17, 0.4), ('leg_fr', -0.17, 0.4), ('leg_bl', 0.17, -0.38), ('leg_br', -0.17, -0.38))
    for n, x, z in legs:
        M.joint(n, 'root', (x, 0.74, z))
    # horse
    M.box('root', (0, 0.95, 0.0), (0.5, 0.42, 0.95), horse)
    M.box('root', (0, 0.95, -0.36), (0.52, 0.46, 0.3), horse)
    M.box('root', (0, 0.74, 0.0), (0.46, 0.1, 0.85), shade)
    M.box('root', (0, 0.99, -0.58), (0.1, 0.4, 0.1), mane, rot=(-22, 0, 0))   # tail
    for n, x, z in legs:
        M.box(n, (x, 0.4, z), (0.14, 0.64, 0.15), horse if z > 0 else shade)
        M.box(n, (x, 0.05, z + 0.01), (0.15, 0.1, 0.17), hoof)
    # neck and head (the head leans forward and down)
    pv = (0, 1.08, 0.38)
    M.box('neck', (0, 1.3, 0.47), (0.2, 0.46, 0.24), horse, rot=(28, 0, 0), pivot=pv, top=(0.85, 1))
    M.box('neck', (0, 1.32, 0.4), (0.07, 0.46, 0.1), mane, rot=(28, 0, 0), pivot=pv)
    hc = (0, 1.55, 0.73)
    M.box('neck', hc, (0.2, 0.22, 0.38), horse, rot=(20, 0, 0), pivot=pv)
    M.box('neck', (0, 1.46, 0.93), (0.15, 0.13, 0.12), horse_light(horse), rot=(20, 0, 0), pivot=pv)
    for s in (1, -1):
        M.box('neck', (s * 0.11, 1.57, 0.78), (0.04, 0.05, 0.05), EYE, rot=(20, 0, 0), pivot=pv)
        M.box('neck', (s * 0.07, 1.7, 0.61), (0.05, 0.12, 0.05), mane, rot=(20, 0, 0), pivot=pv, top=(0.4, 0.6))
    # rider: legs hang down the horse's flanks
    for s in (1, -1):
        M.box('root', (s * 0.31, 1.05, 0.02), (0.13, 0.46, 0.2), STEEL_D if armour == STEEL else armour)
        M.box('root', (s * 0.31, 0.8, 0.05), (0.14, 0.16, 0.3), LEATHER_D)
    M.box('spine', (0, 1.42, 0), (0.46, 0.4, 0.28), armour)
    M.box('spine', (0, 1.24, 0), (0.48, 0.08, 0.3), GOLD)
    for s, aj in ((1, 'arm_l'), (-1, 'arm_r')):
        M.box(aj, (s * 0.31, 1.41, 0), (0.13, 0.3, 0.15), armour)
        M.box(aj, (s * 0.31, 1.2, 0.03), (0.11, 0.13, 0.13), LEATHER_D)
        M.box(aj, (s * 0.31, 1.58, 0), (0.18, 0.09, 0.2), STEEL_L)
    M.box('head', (0, 1.64, 0), (0.1, 0.05, 0.1), SKIN_D)
    M.box('head', (0, 1.77, 0), (0.26, 0.26, 0.26), SKIN)
    for s in (1, -1):
        M.box('head', (s * 0.06, 1.8, 0.135), (0.045, 0.045, 0.02), EYE)
    M.box('head', (0, 1.92, 0), (0.29, 0.1, 0.29), armour)                     # helmet
    M.box('head', (0, 1.8, -0.13), (0.29, 0.22, 0.06), armour)
    return (-0.31, 1.2, 0.07)


def horse_light(c):
    return tuple(min(255, int(v * 1.12 + 6)) for v in c)


def cavalier():
    M = Model('cavalier')
    hand = mount_base(M, HORSE, HORSE_D, LEATHER_D, HORSE_D)
    M.joint('weapon', 'arm_r', hand)
    M.box('root', (0, 1.18, 0.0), (0.54, 0.05, 0.62), SIDEW, SIDE)              # saddle blanket
    M.box('root', (0.27, 1.0, 0.0), (0.04, 0.3, 0.6), SIDEG, SIDE)
    M.box('root', (-0.27, 1.0, 0.0), (0.04, 0.3, 0.6), SIDEG, SIDE)
    M.box('spine', (0, 1.42, 0.145), (0.34, 0.38, 0.03), SIDEW, SIDE)           # tabard
    M.box('spine', (0, 1.38, -0.17), (0.4, 0.5, 0.04), SIDEW, SIDE, top=(1.1, 1))    # short cape
    M.box('head', (0, 1.98, 0), (0.06, 0.07, 0.3), SIDEW, SIDE)                   # crest
    spear(M, 'weapon', hand, alpha=40, length=0.8, butt=0.4)
    M.alpha = 40
    return M


def sky_rider():
    M = Model('sky_rider')
    hand = mount_base(M, WHITE, PEG_SHADE, GREY, PEG_SHADE)
    M.joint('weapon', 'arm_r', hand)
    M.joint('wing_l', 'root', (0.25, 1.2, 0.1))
    M.joint('wing_r', 'root', (-0.25, 1.2, 0.1))
    for s, wj in ((1, 'wing_l'), (-1, 'wing_r')):
        # a wing built along +Y from its shoulder, then laid out to the side and raised 25 degrees
        up = 60 * s
        piv = (s * 0.25, 1.2, 0.1)
        for k, (cy, size, top, col) in enumerate(((0.25, (0.1, 0.5, 0.4), (1, 0.8), PEG_WING),
                                                   (0.72, (0.1, 0.55, 0.32), (1, 0.45), WHITE))):
            M.box(wj, add(piv, (0, cy, 0.0)), (size[0] * 0.5, size[1], size[2]), col, top=top,
                  rot=(0, 0, -up), pivot=piv)
    M.box('root', (0, 1.18, 0.0), (0.54, 0.05, 0.62), SIDEW, SIDE)
    M.box('spine', (0, 1.42, 0.145), (0.34, 0.38, 0.03), SIDEW, SIDE)
    M.box('spine', (0, 1.38, -0.17), (0.4, 0.5, 0.04), SIDEW, SIDE, top=(1.1, 1))
    M.box('head', (0, 1.98, 0), (0.06, 0.07, 0.3), SIDEW, SIDE)
    spear(M, 'weapon', hand, alpha=40, length=0.8, butt=0.4)
    M.alpha = 40
    return M


# ---------------------------------------------------------------- the fort
def fort():
    M = Model('fort')
    M.box(None, (0, 0.05, 0), (1.0, 0.1, 1.0), rgb(0x6C6A70))                         # plinth
    M.box(None, (0, 0.6, 0), (0.66, 1.0, 0.66), STONE_D)                          # keep
    M.box(None, (0, 1.12, 0), (0.7, 0.06, 0.7), STONE)                        # parapet floor
    for s in (-1, 1):
        for t in (-0.2, 0.0, 0.2):                                              # battlements
            M.box(None, (s * 0.3, 1.2, t), (0.1, 0.1, 0.1), STONE)
            M.box(None, (t, 1.2, s * 0.3), (0.1, 0.1, 0.1), STONE)
    for sx in (-1, 1):                                                          # corner turrets
        for sz in (-1, 1):
            M.box(None, (sx * 0.38, 0.55, sz * 0.38), (0.24, 0.9, 0.24), STONE, top=(0.9, 0.9))
            M.box(None, (sx * 0.38, 1.04, sz * 0.38), (0.28, 0.08, 0.28), STONE_L)
            for ex, ez in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
                M.box(None, (sx * 0.38 + ex * 0.1, 1.1, sz * 0.38 + ez * 0.1), (0.08, 0.08, 0.08), STONE_D)
    M.box(None, (0, 0.28, 0.345), (0.24, 0.36, 0.04), rgb(0x3A2F28))            # gate
    M.box(None, (0, 0.5, 0.345), (0.24, 0.08, 0.04), rgb(0x3A2F28), top=(0.6, 1))
    M.box(None, (0, 0.15, 0.4), (0.4, 0.1, 0.2), STONE)                       # step
    for sx in (-1, 1):
        M.box(None, (sx * 0.12, 0.8, 0.335), (0.05, 0.2, 0.02), rgb(0x35465E))  # arrow slits
    M.box(None, (0, 1.37, 0), (0.04, 0.5, 0.04), rgb(0x4A4038))                 # flag pole... drawn up to 1.3 below
    M.box(None, (0.14, 1.5, 0), (0.24, 0.16, 0.025), SIDEW, SIDE)               # banner
    M.box(None, (0.29, 1.5, 0), (0.06, 0.16, 0.025), SIDEG, SIDE, top=(0.2, 1))
    # squash to about 1.3 tall (the parts above were laid out at full height)
    M.tris = [(m, [((p[0], p[1] * 0.82, p[2]), n) for p, n in vs], c, j) for m, vs, c, j in M.tris]
    return M


# ---------------------------------------------------------------- animations
def foot_clips(M, weapon_joint='weapon', style='sword', alpha=0.0, shield=False):
    """Idle / Move / Attack / Die for a standing figure."""
    wj = weapon_joint
    bow = style == 'bow'
    held = 'arm_l' if bow else 'arm_r'
    alpha = alpha

    def hold(c, t):
        c.wrist(wj, held, t, world=None, alpha=alpha)

    # Idle: breathing, arms sway
    c = Clip(M, 1.2, True)
    for t, b in ((0.0, 0), (0.6, 1), (1.2, 0)):
        c.pos('spine', t, dy=0.012 * b)
        c.rot('spine', t, x=1.5 * b)
        c.rot('head', t, x=-1.5 * b)
        c.rot('arm_l', t, x=3 * b, z=2 * b)
        c.rot('arm_r', t, x=-3 * b, z=-2 * b)
        hold(c, t)
    M.clips['Idle'] = c

    # Move: legs and arms swing; the weapon arm swings less and the weapon is held steady
    c = Clip(M, 0.6, True)
    for t, p, d in ((0.0, 25, -0.065), (0.15, 0, 0.0), (0.3, -25, -0.065), (0.45, 0, 0.0), (0.6, 25, -0.065)):
        c.pos('root', t, dy=d)
        c.rot('leg_l', t, x=-p)
        c.rot('leg_r', t, x=p)
        c.rot('arm_l', t, x=p * 0.9 * (0.5 if bow else 1))
        c.rot('arm_r', t, x=-p * 0.5)
        c.rot('spine', t, y=p * 0.12, x=3)
        c.rot('head', t, y=-p * 0.1)
        hold(c, t)
    M.clips['Move'] = c

    # Attack (one-shot, 0.5 s)
    c = Clip(M, 0.5, False)
    if bow:
        # raise the bow, draw, loose, lower
        for t, la, ra, dz in ((0.0, 0, 0, 0), (0.12, -85, -75, -0.0), (0.28, -88, -60, -0.12), (0.34, -88, -78, 0.0),
                              (0.5, 0, 0, 0)):
            c.rot('spine', t, x=-3 if 0.1 < t < 0.4 else 0)
            c.rot('arm_l', t, x=la)
            c.rot('arm_r', t, x=ra, z=-8 if t in (0.28,) else 0)
            c.pos('arm_r', t, dz=dz)
            c.wrist('bow', 'arm_l', t, world=None, alpha=0)
    else:
        wind, strike = ATTACK[style]
        keys = [(0.0, (0, alpha, 0, 0.0, 0.0)), (0.15, wind), (0.26, strike), (0.34, strike), (0.5, (0, alpha, 0, 0.0, 0.0))]
        for t, (arm, w, sp, dz, rz) in keys:
            c.rot('spine', t, x=sp)
            c.rot('arm_r', t, x=arm)
            c.pos('arm_r', t, dz=dz * 0.6)
            c.pos('root', t, dz=dz)
            c.rot('arm_l', t, x=(-35 if sp > 5 and not shield else (-15 if shield and sp > 5 else 0)) if t > 0 else 0)
            c.wrist(wj, 'arm_r', t, world=w, alpha=alpha)
        # the weapon stays at the end of the strike
    M.clips['Attack'] = c

    # Die: topple backwards, arms flung out; the root height is adjusted so the body rests on the ground
    c = Clip(M, 0.7, False)
    times = (0.0, 0.1, 0.2, 0.3, 0.42, 0.55, 0.7)
    angles = (0, -12, -32, -58, -84, -93, -90)
    for t, a in zip(times, angles):
        f = min(1.0, -a / 90)
        c.rot('root', t, x=a)
        c.pos('root', t, dz=-0.25 * f)
        c.rot('spine', t, x=-6 * f)
        c.rot('head', t, x=-12 * f)
        c.rot('arm_l', t, x=-20 * f, z=55 * f)
        c.rot('arm_r', t, x=-20 * f, z=-55 * f)
        c.rot('leg_l', t, x=8 * f, z=6 * f)
        c.rot('leg_r', t, x=-6 * f, z=-6 * f)
        c.wrist(wj, held, t, world=None, alpha=alpha)
    M.clips['Die'] = c
    settle(M, c, times)


# attack keys per style: (wind-up, strike) each = (arm x, world weapon angle, spine lean, root dz, unused)
ATTACK = {
    'sword': ((-100, -15, -6, -0.05, 0), (-75, 90, 14, 0.14, 0)),
    'axe': ((-150, 10, -6, -0.05, 0), (-55, 105, 16, 0.14, 0)),
    'spear': ((8, 85, -4, -0.12, 0), (-82, 88, 8, 0.18, 0)),
}


def settle(M, clip, times):
    """Adds a root height to every key of a Die clip so the lowest vertex just touches Y = 0."""
    for t in times:
        miny = min(p[1] for _, vs, _, _ in posed_tris(M, clip, t) for p, _ in vs)
        dx, _, dz = clip.poss.get('root', {}).get(t, (0.0, 0.0, 0.0))
        clip.pos('root', t, dx=dx, dy=-miny, dz=dz)


def mount_clips(M, alpha=40, wings=False):
    legs = ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br')

    def hold(c, t):
        c.wrist('weapon', 'arm_r', t, world=None, alpha=alpha)

    def flap(c, t, a):
        if wings:
            c.rot('wing_l', t, z=a)
            c.rot('wing_r', t, z=-a)

    c = Clip(M, 1.2, True)
    flaps = {0.0: 0, 0.3: 18, 0.6: 0, 0.9: -22, 1.2: 0} if wings else {0.0: 0, 0.6: 0, 1.2: 0}
    for t in sorted(flaps):
        b = 1 if t == 0.6 else 0
        c.pos('root', t, dy=0.012 * b)
        c.rot('spine', t, x=1.5 * b)
        c.rot('neck', t, x=-4 * b)
        c.rot('arm_l', t, x=3 * b)
        c.rot('arm_r', t, x=-3 * b)
        hold(c, t)
        flap(c, t, flaps[t])
    M.clips['Idle'] = c

    c = Clip(M, 0.6, True)
    wflap = {0.0: 20, 0.15: 0, 0.3: -25, 0.45: 0, 0.6: 20}
    for t, p, d in ((0.0, 22, -0.05), (0.15, 0, 0.0), (0.3, -22, -0.05), (0.45, 0, 0.0), (0.6, 22, -0.05)):
        c.pos('root', t, dy=d)
        c.rot('leg_fl', t, x=p)
        c.rot('leg_br', t, x=p)
        c.rot('leg_fr', t, x=-p)
        c.rot('leg_bl', t, x=-p)
        c.rot('neck', t, x=-p * 0.18)
        c.rot('spine', t, x=-p * 0.06 + 2)
        c.rot('arm_l', t, x=p * 0.2)
        c.rot('arm_r', t, x=-p * 0.2)
        hold(c, t)
        flap(c, t, wflap[t])
    M.clips['Move'] = c

    c = Clip(M, 0.5, False)
    wind, strike = ATTACK['spear']
    wind = (10, 80, -5, -0.06, 0)
    strike = (-82, 88, 9, 0.12, 0)
    for t, (arm, w, sp, dz, _) in ((0.0, (0, alpha, 0, 0, 0)), (0.15, wind), (0.26, strike), (0.34, strike),
                                   (0.5, (0, alpha, 0, 0, 0))):
        c.rot('spine', t, x=sp)
        c.rot('arm_r', t, x=arm)
        c.pos('arm_r', t, dz=dz * 1.4)
        c.pos('root', t, dz=dz * 0.6)
        c.rot('neck', t, x=-8 if t in (0.26, 0.34) else 0)
        c.rot('arm_l', t, x=-25 if t in (0.26, 0.34) else 0)
        c.wrist('weapon', 'arm_r', t, world=w, alpha=alpha)
        flap(c, t, 22 if t in (0.15, 0.26) else (10 if t == 0.34 else 0))
    M.clips['Attack'] = c

    # Die: the whole animal rolls onto its side; legs flail, rider slumps
    c = Clip(M, 0.7, False)
    times = (0.0, 0.1, 0.2, 0.3, 0.42, 0.55, 0.7)
    angles = (0, 12, 32, 58, 84, 93, 90)
    for t, a in zip(times, angles):
        f = min(1.0, a / 90)
        c.rot('root', t, z=a, x=-4 * f)
        c.rot('spine', t, x=-10 * f, z=-15 * f)
        c.rot('head', t, x=-10 * f)
        c.rot('neck', t, x=-25 * f)
        c.rot('arm_l', t, x=-30 * f, z=30 * f)
        c.rot('arm_r', t, x=-30 * f, z=-30 * f)
        for i, leg in enumerate(legs):
            c.rot(leg, t, x=(-25 if i % 2 else 25) * f)
        c.wrist('weapon', 'arm_r', t, world=None, alpha=alpha)
        flap(c, t, -30 * f)
    M.clips['Die'] = c
    settle(M, c, times)


UNIT_BUILDERS = {
    'lord': (lord, lambda M: foot_clips(M, style='sword', alpha=35)),
    'cavalier': (cavalier, lambda M: mount_clips(M, 40)),
    'archer': (archer, lambda M: foot_clips(M, 'bow', style='bow', alpha=0)),
    'fighter': (fighter, lambda M: foot_clips(M, style='axe', alpha=20)),
    'knight': (knight, lambda M: foot_clips(M, style='spear', alpha=12, shield=True)),
    'soldier': (soldier, lambda M: foot_clips(M, style='spear', alpha=15, shield=True)),
    'sky_rider': (sky_rider, lambda M: mount_clips(M, 40, wings=True)),
}
# The promoted and summoned types live in unit_models_promoted.py, which builds on everything above. When this
# file runs as a script it is "__main__": register it under its own name so that module's import finds this one.
sys.modules.setdefault('make_unit_models', sys.modules[__name__])
import unit_models_promoted as promoted  # noqa: E402
UNIT_BUILDERS.update(promoted.BUILDERS)
UNIT_ORDER = list(UNIT_BUILDERS)


def centre(M):
    """Shifts the whole model so the body (not arms, weapon, bow, shield) is centred in X and Z."""
    keep = {i for i, j in enumerate(M.joints) if j[0] not in ('weapon', 'bow', 'arm_l', 'arm_r')}
    pts = [p for _, vs, _, j in M.tris if j in keep for p, _ in vs]
    dx = -(min(p[0] for p in pts) + max(p[0] for p in pts)) / 2
    dz = -(min(p[2] for p in pts) + max(p[2] for p in pts)) / 2
    M.tris = [(m, [(add(p, (dx, 0, dz)), n) for p, n in vs], c, j) for m, vs, c, j in M.tris]
    M.joints = [(n, par, add(p, (dx, 0, dz))) for n, par, p in M.joints]


def make_unit(uid):
    build, clips = UNIT_BUILDERS[uid]
    M = build()
    centre(M)
    clips(M)
    return M


def unit_bytes(uid):
    M = make_unit(uid)
    return glb_bytes(*build_gltf(M))


def fort_bytes():
    return glb_bytes(*build_gltf(fort(), skinned=False))


def out_dir(root):
    return os.path.join(root, 'games', 'demo-srpg', 'models')


def write_all(root):
    d = out_dir(root)
    os.makedirs(d, exist_ok=True)
    for uid in UNIT_ORDER:
        with open(os.path.join(d, uid + '.glb'), 'wb') as f:
            f.write(unit_bytes(uid))
    with open(os.path.join(d, 'fort.glb'), 'wb') as f:
        f.write(fort_bytes())
    print('wrote', d)


def write_pose(clip_time, out, uid):
    cname, t = clip_time.split(':')
    M = make_unit(uid)
    tris = posed_tris(M, M.clips[cname], float(t))
    with open(out, 'wb') as f:
        f.write(glb_bytes(*build_gltf(M, tris=tris, skinned=False)))


# ---------------------------------------------------------------- self-check
def read_glb(path):
    data = open(path, 'rb').read()
    magic, ver, length = struct.unpack_from('<4sII', data, 0)
    assert magic == b'glTF' and ver == 2 and length == len(data), 'bad header'
    jl, jt = struct.unpack_from('<II', data, 12)
    assert jt == 0x4E4F534A
    doc = json.loads(data[20:20 + jl])
    bl, bt = struct.unpack_from('<II', data, 20 + jl)
    assert bt == 0x004E4942
    binary = data[28 + jl:28 + jl + bl]
    assert len(binary) == bl and bl >= doc['buffers'][0]['byteLength']
    return doc, binary


COMP = {5126: ('f', 4), 5121: ('B', 1), 5123: ('H', 2)}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def read_acc(doc, binary, i):
    a = doc['accessors'][i]
    v = doc['bufferViews'][a['bufferView']]
    fmt, size = COMP[a['componentType']]
    n = NCOMP[a['type']]
    off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    assert off + a['count'] * n * size <= v['byteOffset'] + v['byteLength'] + 0, 'accessor overruns its view'
    vals = struct.unpack_from('<%d%s' % (a['count'] * n, fmt), binary, off)
    return [vals[k * n:(k + 1) * n] for k in range(a['count'])]


def check_file(path, uid):
    doc, binary = read_glb(path)
    is_fort = uid == 'fort'
    mats = {m['name']: m for m in doc['materials']}
    assert set(mats) == {'body', 'side'}
    for m in mats.values():
        assert m['pbrMetallicRoughness']['baseColorFactor'] == [1, 1, 1, 1]
    assert 'textures' not in doc and 'images' not in doc
    verts = []       # (pos, joint, colour, material)
    tris = 0
    for prim in doc['meshes'][0]['primitives']:
        a = prim['attributes']
        pos = read_acc(doc, binary, a['POSITION'])
        nor = read_acc(doc, binary, a['NORMAL'])
        col = read_acc(doc, binary, a['COLOR_0'])
        idx = [i[0] for i in read_acc(doc, binary, prim['indices'])]
        assert len(pos) == len(nor) == len(col) and len(idx) % 3 == 0 and max(idx) < len(pos)
        tris += len(idx) // 3
        acc = doc['accessors'][a['POSITION']]
        for k in range(3):
            assert abs(min(p[k] for p in pos) - acc['min'][k]) < 1e-5 and abs(max(p[k] for p in pos) - acc['max'][k]) < 1e-5
        for n in nor:
            assert abs(math.sqrt(sum(c * c for c in n)) - 1) < 1e-4
        # front faces are counter-clockwise: the geometric normal agrees with the stored one
        for t in range(0, len(idx), 3):
            p = [pos[idx[t + k]] for k in range(3)]
            g = cross(sub(p[1], p[0]), sub(p[2], p[0]))
            assert sum(x * y for x, y in zip(g, nor[idx[t]])) > 0, 'face winding disagrees with its normal'
        if is_fort:
            assert 'JOINTS_0' not in a
            joints = [0] * len(pos)
        else:
            jn = read_acc(doc, binary, a['JOINTS_0'])
            wt = read_acc(doc, binary, a['WEIGHTS_0'])
            nj = len(doc['skins'][0]['joints'])
            for j, w in zip(jn, wt):
                assert j[0] < nj and abs(sum(w) - 1) < 1e-5 and w[0] == 1
            joints = [j[0] for j in jn]
        mat = doc['materials'][prim['material']]['name']
        for p, j, c in zip(pos, joints, col):
            verts.append((p, j, tuple(c[:3]), mat))
    if is_fort:
        assert 'skins' not in doc and 'animations' not in doc and len(doc['nodes']) == 1
        assert 60 <= tris <= 600, tris
    else:
        assert 150 <= tris <= 800, 'triangle count %d' % tris
        assert len(doc['skins']) == 1
        names = [n['name'] for n in doc['nodes']]
        skin = doc['skins'][0]
        ibm = read_acc(doc, binary, skin['inverseBindMatrices'])
        assert len(ibm) == len(skin['joints'])
        assert doc['nodes'][-1].get('skin') == 0 and 'mesh' in doc['nodes'][-1]
        quad = uid in promoted.QUADRUPEDS
        for need in (('root', 'spine', 'head', 'tail', 'leg_fl', 'leg_fr', 'leg_bl', 'leg_br') if quad else
                     ('root', 'spine', 'head', 'arm_l', 'arm_r')):
            assert need in names
        clips = {a['name']: a for a in doc['animations']}
        assert set(clips) == {'Idle', 'Move', 'Attack', 'Die'}, set(clips)
        dur = {}
        for cname, an in clips.items():
            end = 0
            for ch in an['channels']:
                s = an['samplers'][ch['sampler']]
                assert s['interpolation'] == 'LINEAR'
                times = [t[0] for t in read_acc(doc, binary, s['input'])]
                vals = read_acc(doc, binary, s['output'])
                assert times[0] == 0 and times == sorted(times) and len(set(times)) == len(times)
                assert len(times) == len(vals) and ch['target']['node'] < len(skin['joints'])
                end = max(end, times[-1])
                if ch['target']['path'] == 'rotation':
                    for q in vals:
                        assert abs(math.sqrt(sum(c * c for c in q)) - 1) < 1e-4, 'quaternion not normalised'
                if cname in ('Idle', 'Move'):
                    assert all(abs(a - b) < 1e-5 for a, b in zip(vals[0], vals[-1])), '%s loop does not close' % cname
            dur[cname] = end
        assert abs(dur['Idle'] - 1.2) < 0.05 and abs(dur['Move'] - 0.6) < 0.05
        assert abs(dur['Attack'] - 0.5) < 0.05 and abs(dur['Die'] - 0.7) < 0.05, dur
    ys = [v[0][1] for v in verts]
    assert abs(min(ys)) < 0.02, 'feet not on Y = 0: %f' % min(ys)
    body = verts
    if not is_fort:
        keep = {skin['joints'][i] for i, n in enumerate(names[:-1]) if n not in ('weapon', 'bow', 'arm_l', 'arm_r')}
        body = [v for v in verts if v[1] in keep]
    cx = (min(v[0][0] for v in body) + max(v[0][0] for v in body)) / 2
    cz = (min(v[0][2] for v in body) + max(v[0][2] for v in body)) / 2
    assert abs(cx) < 0.06 and abs(cz) < 0.12, 'not centred: %f %f' % (cx, cz)
    height = max(ys)
    wid = max(v[0][0] for v in verts) - min(v[0][0] for v in verts)
    dep = max(v[0][2] for v in verts) - min(v[0][2] for v in verts)
    side = sum(1 for v in verts if v[3] == 'side')
    assert 0.03 < side / len(verts) < 0.9, 'side share %f' % (side / len(verts))
    if is_fort:
        assert 1.2 < height < 1.4 and wid < 1.1 and dep < 1.1
        return dict(tris=tris, height=height, joints=[], clips={}, width=wid, depth=dep, side=side / len(verts))
    # facing +Z: the eyes (dark spots on the head joint) lie in front of the head's centre
    hj = skin['joints'][names.index('head')]
    eyes = [v for v in verts if v[1] == hj and v[2] == EYE]
    hz = [v[0][2] for v in verts if v[1] == hj]
    if quad:      # a dog's head is long: its eyes sit in the front half of the animal instead
        assert eyes and sum(v[0][2] for v in eyes) / len(eyes) > 0.3, 'face does not look to +Z'
        assert height < 0.95 and wid < 0.7 and 0.9 < dep < 1.4, (height, wid, dep)
        return dict(tris=tris, height=height, width=wid, depth=dep, joints=names[:-1], clips=dur, side=side / len(verts))
    assert eyes and sum(v[0][2] for v in eyes) / len(eyes) > sum(hz) / len(hz), 'face does not look to +Z'
    if 'neck' in names:       # mounted: the horse's head is in front of its body
        nj = names.index('neck')
        nz = [v[0][2] for v in verts if v[1] == nj]
        assert sum(nz) / len(nz) > 0.2
        assert height < 2.2 and (max(v[0][2] for v in body) - min(v[0][2] for v in body)) < 1.85
    else:
        assert height < 1.9 and wid < 1.3 and dep < 1.2, (height, wid, dep)
    wj = names.index('weapon') if 'weapon' in names else names.index('bow')
    wz = [v[0][2] for v in verts if v[1] == wj]
    # the held thing is in front of the torso's back
    assert max(wz) > 0.0
    return dict(tris=tris, height=height, width=wid, depth=dep, joints=names[:-1], clips=dur, side=side / len(verts))


def check(root):
    d = out_dir(root)
    for uid in UNIT_ORDER + ['fort']:
        path = os.path.join(d, uid + '.glb')
        info = check_file(path, uid)
        fresh = fort_bytes() if uid == 'fort' else unit_bytes(uid)
        assert open(path, 'rb').read() == fresh, uid + ': file differs from a fresh run (not deterministic / stale)'
        print('%-9s ok  tris=%d height=%.2f w=%.2f d=%.2f side=%.0f%% joints=%s clips=%s' % (
            uid, info['tris'], info['height'], info['width'], info['depth'], info['side'] * 100,
            ','.join(info['joints']), {k: round(v, 2) for k, v in info['clips'].items()}))
    print('all checks passed')


def main():
    args = sys.argv[1:]
    default_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    if args and args[0] == '--pose':
        write_pose(args[1], args[2], args[3])
        return
    if args and args[0] == '--check':
        check(args[1] if len(args) > 1 else default_root)
        return
    write_all(args[0] if args else default_root)


if __name__ == '__main__':
    sys.dont_write_bytecode = True
    main()
