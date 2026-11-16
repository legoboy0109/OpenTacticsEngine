"""Generated wings for a mount (no download: this part of the model is original to the project, GPL-3.0).

A wing is a pivot node (the animated flap) under the mount's joint, carrying a small mesh: an arm bone and
a fan of tapered feathers. add_wings() adds two and returns a function that makes the flap tracks of a clip.
"""
import numpy as np

import glb_merge as gm
import glb_pose as gp

# Where the wings start on the mount (model space, left side; the right is mirrored) and how they rest.
ROOT = (0.42, 1.50, 0.02)
REST_DEG = 40.0          # raised: 0 = straight out sideways, 90 = straight up
FEATHERS = 9
SPAN = 1.9
WHITE = [0.96, 0.96, 1.0, 1.0]
SHADE = [0.80, 0.84, 0.94, 1.0]


def _slab(base, direction, side, length, width, thick, taper=0.45):
    """A tapered flat slab from `base` along `direction` (feather or bone): returns (positions, normals, indices)."""
    d = np.array(direction, float)
    d /= np.linalg.norm(d)
    s = np.array(side, float)
    s -= d * np.dot(s, d)
    s /= np.linalg.norm(s)
    n = np.cross(d, s)
    b = np.array(base, float)
    tip = b + d * length
    # eight corners: base (wide) then tip (narrower)
    def ring(c, w):
        return [c - s * w / 2 - n * thick / 2, c + s * w / 2 - n * thick / 2, c + s * w / 2 + n * thick / 2, c - s * w / 2 + n * thick / 2]
    corners = ring(b, width) + ring(tip, width * taper)
    faces = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    pos, nor, idx = [], [], []
    for f in faces:
        q = [corners[i] for i in f]
        fn = np.cross(q[1] - q[0], q[2] - q[0])
        fn /= np.linalg.norm(fn)
        if np.dot(fn, np.mean(q, axis=0) - (b + tip) / 2) < 0:      # make the face normal point outwards
            q = q[::-1]
            fn = -fn
        k = len(pos)
        pos += q
        nor += [fn] * 4
        idx += [k, k + 1, k + 2, k, k + 2, k + 3]
    return pos, nor, idx


def wing_mesh(mirror):
    """Left wing (extends along +x; chord along z; thin along y). mirror = the right wing."""
    parts = []
    # the arm: out along +x, slightly forward, then the feathers fan out from along it
    arm = np.array([1.0, 0.0, 0.10])
    parts.append((_slab((0, 0, 0.05), arm, (0, 0, 1), SPAN * 0.55, 0.16, 0.10, 0.7), 0))
    for i in range(FEATHERS):
        t = i / (FEATHERS - 1)
        yaw = np.radians(-2 - t * 38)                         # outer feathers point out, inner ones sweep back
        d = (np.cos(yaw), 0.0, np.sin(yaw))
        start = np.array([0.18 + 0.52 * (1 - t), 0.0, 0.02 - 0.10 * t])
        length = SPAN * (1.0 - 0.34 * t)
        parts.append((_slab(start + (0, 0.012 * (i % 2) - 0.006, 0), d, (0, 1, 0), length, 0.46 - 0.08 * t, 0.035, 0.8), 1 + i % 2))
    pos, nor, idx, col = [], [], [], []
    for (p, n, ix), shade in parts:
        k = len(pos)
        pos += p
        nor += n
        idx += [k + j for j in ix]
        col += [WHITE if shade != 2 else SHADE] * len(p)
    pos, nor = np.array(pos), np.array(nor)
    idx = np.array(idx).reshape(-1, 3)
    if mirror:
        pos[:, 0] *= -1
        nor[:, 0] *= -1
        idx = idx[:, ::-1]
    return pos, nor, idx.reshape(-1)


def add_wings(g, joint):
    """Add two wings under `joint` of Glb `g`; returns tracks(state, duration) -> track list for new_clip."""
    d = g.doc
    d.setdefault('materials', []).append({'name': 'wing', 'doubleSided': True,
                                          'pbrMetallicRoughness': {'baseColorFactor': WHITE, 'metallicFactor': 0, 'roughnessFactor': 0.8}})
    mat = len(d['materials']) - 1
    j = g.node(joint)
    bw = gp.world_matrices(d)[j]
    pivots = []
    for mirror in (False, True):
        pos, nor, idx = wing_mesh(mirror)
        prim = {'attributes': {'POSITION': g.add_accessor(pos), 'NORMAL': g.add_accessor(nor)},
                'indices': g.add_accessor(idx.reshape(-1, 1), component=gm.USHORT, target=34963), 'material': mat}
        d['meshes'].append({'name': 'wing', 'primitives': [prim]})
        sign = -1.0 if mirror else 1.0
        root = np.array([ROOT[0] * sign, ROOT[1], ROOT[2]])

        def local(deg, sign=sign, root=root):
            return np.linalg.inv(bw) @ gm.translate(*root) @ gm.rotation((0, 0, 1), sign * deg)
        pivot = g.add_node('wing.' + ('r' if mirror else 'l'), parent=j, local=local(REST_DEG))
        g.add_node('wing_mesh.' + ('r' if mirror else 'l'), parent=pivot, mesh=len(d['meshes']) - 1)
        pivots.append((pivot, local))

    def tracks(state, dur):
        n = max(8, int(round(dur * 24)))
        ts = np.linspace(0.0, dur, n + 1)
        if state == 'Idle':
            ang = REST_DEG + 9 * np.sin(2 * np.pi * ts / (dur / 3))
        elif state == 'Move':
            ang = REST_DEG - 8 + 38 * np.sin(2 * np.pi * ts / (dur / 2) + 0.6)
        elif state == 'Attack':
            ang = REST_DEG + 40 * np.sin(2 * np.pi * ts / dur) - 25 * np.sin(4 * np.pi * ts / dur)
        else:                                                           # Die: the wings droop and spread
            ang = REST_DEG - (REST_DEG + 55) * np.clip(ts / dur, 0, 1) ** 0.7
        out = []
        for pivot, local in pivots:
            q = np.array([gm.decompose(local(a))[1] for a in ang])
            for k in range(1, len(q)):                                  # keep neighbouring keys on the same side
                if np.dot(q[k], q[k - 1]) < 0:
                    q[k] = -q[k]
            out.append((pivot, 'rotation', ts, q))
        return out
    return tracks
