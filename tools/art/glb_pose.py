"""Reading a .glb's numbers: accessors, node matrices, animation sampling, mesh bounds.

Helper for glb_merge.py (combining models). Pure Python + numpy; matrices are 4x4, column vectors
(M @ v), the glTF convention. Nothing here writes files.
"""
import struct

import numpy as np

_COMP = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
_NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT2': 4, 'MAT3': 9, 'MAT4': 16}


def read_accessor(doc, blob, index):
    """The accessor's data as a numpy array (count, components); integers stay integers."""
    a = doc['accessors'][index]
    v = doc['bufferViews'][a['bufferView']]
    code, size = _COMP[a['componentType']]
    n = _NCOMP[a['type']]
    stride = v.get('byteStride') or size * n
    base = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    out = np.empty((a['count'], n), dtype=np.dtype('<' + code))
    for i in range(a['count']):
        out[i] = struct.unpack_from('<' + code * n, blob, base + i * stride)
    return out


def trs_matrix(node):
    """The node's local matrix (from `matrix`, or translation / rotation / scale)."""
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=float).reshape(4, 4).T
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    s = node.get('scale', [1, 1, 1])
    m = np.eye(4)
    m[:3, :3] = r * np.array(s)[None, :]
    m[:3, 3] = node.get('translation', [0, 0, 0])
    return m


def parents(doc):
    p = {}
    for i, n in enumerate(doc['nodes']):
        for c in n.get('children', []):
            p[c] = i
    return p


def world_matrices(doc, local=None):
    """World matrix of every node. `local` optionally maps node -> local matrix (a posed value)."""
    par = parents(doc)
    out = {}

    def get(i):
        if i not in out:
            m = local[i] if local and i in local else trs_matrix(doc['nodes'][i])
            out[i] = get(par[i]) @ m if i in par else m
        return out[i]
    for i in range(len(doc['nodes'])):
        get(i)
    return out


def node_index(doc, name):
    for i, n in enumerate(doc['nodes']):
        if n.get('name') == name:
            return i
    raise KeyError(name)


def _slerp(a, b, t):
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        q = a + t * (b - a)
    else:
        th = np.arccos(d)
        q = (np.sin((1 - t) * th) * a + np.sin(t * th) * b) / np.sin(th)
    return q / np.linalg.norm(q)


def sample_clip(doc, blob, clip, time):
    """{node: (translation, rotation, scale)} for the nodes the clip animates, at `time` (holds at the ends)."""
    res = {}
    for ch in clip['channels']:
        t = ch['target']
        if 'node' not in t or t['path'] not in ('translation', 'rotation', 'scale'):
            continue
        s = clip['samplers'][ch['sampler']]
        ts = read_accessor(doc, blob, s['input'])[:, 0]
        vs = read_accessor(doc, blob, s['output']).astype(float)
        if s.get('interpolation') == 'CUBICSPLINE':
            vs = vs[1::3]
        if time <= ts[0]:
            v = vs[0]
        elif time >= ts[-1]:
            v = vs[-1]
        else:
            k = int(np.searchsorted(ts, time) - 1)
            f = (time - ts[k]) / (ts[k + 1] - ts[k])
            if s.get('interpolation') == 'STEP':
                v = vs[k]
            elif t['path'] == 'rotation':
                v = _slerp(vs[k], vs[k + 1], f)
            else:
                v = vs[k] + f * (vs[k + 1] - vs[k])
        res.setdefault(t['node'], {})[t['path']] = v
    return res


def posed_locals(doc, blob, clip, time):
    """Local matrices of all nodes at `time` of `clip` (None = rest pose)."""
    local = {}
    posed = sample_clip(doc, blob, clip, time) if clip else {}
    for i, n in enumerate(doc['nodes']):
        node = dict(n)
        node.pop('matrix', None)
        base = trs_matrix(n) if 'matrix' in n else None
        for path, v in posed.get(i, {}).items():
            node[path] = [float(x) for x in v]
        local[i] = trs_matrix(node) if base is None or i in posed else base
    return local


def clip_by_name(doc, name):
    for a in doc.get('animations', []):
        if a.get('name') == name:
            return a
    raise KeyError(name)


def clip_length(doc, blob, clip):
    return max(float(read_accessor(doc, blob, s['input'])[-1, 0]) for s in clip['samplers'])


def skinned_points(doc, blob, node_i, local=None):
    """Vertex positions (n, 3) of the mesh on node `node_i` in model space.

    A skinned node's vertices are already in model space (the loader ignores the node's matrix);
    a rigid node's are moved by its world matrix. `local` poses the nodes; the skin is applied for real.
    """
    n = doc['nodes'][node_i]
    world = world_matrices(doc, local)
    pts = []
    for p in doc['meshes'][n['mesh']]['primitives']:
        pos = read_accessor(doc, blob, p['attributes']['POSITION']).astype(float)
        if 'skin' in n and 'JOINTS_0' in p['attributes']:
            skin = doc['skins'][n['skin']]
            ibm = read_accessor(doc, blob, skin['inverseBindMatrices']).astype(float).reshape(-1, 4, 4).transpose(0, 2, 1)
            jm = np.array([world[j] @ ibm[k] for k, j in enumerate(skin['joints'])])
            joints = read_accessor(doc, blob, p['attributes']['JOINTS_0'])
            weights = read_accessor(doc, blob, p['attributes']['WEIGHTS_0']).astype(float)
            if weights.dtype.kind == 'f' and weights.max() > 1.5:
                weights = weights / 255.0
            ph = np.hstack([pos, np.ones((len(pos), 1))])
            out = np.zeros((len(pos), 3))
            for c in range(4):
                m = jm[joints[:, c]]
                out += weights[:, c:c + 1] * np.einsum('nij,nj->ni', m, ph)[:, :3]
            pts.append(out)
        else:
            pts.append((np.hstack([pos, np.ones((len(pos), 1))]) @ world[node_i].T)[:, :3])
    return np.vstack(pts)


def model_points(doc, blob, local=None):
    """All mesh vertices of the default scene in model space."""
    pts = [skinned_points(doc, blob, i, local) for i, n in enumerate(doc['nodes']) if 'mesh' in n]
    return np.vstack(pts)
