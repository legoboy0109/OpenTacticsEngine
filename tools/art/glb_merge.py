"""Combining .glb models: one model's nodes inside another's, new tracks, a changed rest pose.

Used by make_detailed_units.py to hang a weapon on a character's hand joint, to seat a rider on a
mount and to give a mount wings. The engine's loader poses every node of the scene on the CPU and
accepts several skins in one file, so the result is an ordinary model: two skeletons in one scene,
one clip animating both.

The class Glb keeps `doc` (the glTF JSON) and `blob` (the one binary buffer, a bytearray).
Add pieces with append(), move them with reparent(), change what they stand in with set_rest_pose()
and rebind_rest(), add animation with new_clip(); finish with trim_glb.trim() to drop what nothing uses.
"""
import copy

import numpy as np

import glb_pose as gp

FLOAT, USHORT, UINT = 5126, 5123, 5125
_TYPE = {1: 'SCALAR', 2: 'VEC2', 3: 'VEC3', 4: 'VEC4', 16: 'MAT4'}


class Glb:
    def __init__(self, doc, blob):
        self.doc = doc
        self.blob = bytearray(blob)

    # ---- data -------------------------------------------------------------------------------
    def add_view(self, data, target=None):
        self.blob += b'\0' * (-len(self.blob) % 4)
        v = {'buffer': 0, 'byteOffset': len(self.blob), 'byteLength': len(data)}
        if target:
            v['target'] = target
        self.blob += data
        self.doc['bufferViews'].append(v)
        return len(self.doc['bufferViews']) - 1

    def add_accessor(self, arr, component=FLOAT, normalized=False, target=None):
        """New accessor holding `arr` (count, n); min / max are written for POSITION-like float data."""
        arr = np.asarray(arr)
        n = arr.shape[1] if arr.ndim == 2 else 1
        dtype = {FLOAT: '<f4', USHORT: '<u2', UINT: '<u4'}[component]
        data = np.ascontiguousarray(arr.reshape(len(arr), n).astype(dtype)).tobytes()
        a = {'bufferView': self.add_view(data, target), 'componentType': component,
             'count': int(len(arr)), 'type': _TYPE[n]}
        if normalized:
            a['normalized'] = True
        if component == FLOAT and n <= 4:
            a['min'] = [float(x) for x in arr.min(axis=0)]
            a['max'] = [float(x) for x in arr.max(axis=0)]
        self.doc['accessors'].append(a)
        return len(self.doc['accessors']) - 1

    def read(self, accessor):
        return gp.read_accessor(self.doc, self.blob, accessor)

    # ---- joining two models --------------------------------------------------------------------
    def append(self, other):
        """Copy everything of `other` (a Glb) into this one. Returns (node offset, its root nodes, clip offset)."""
        d, o = self.doc, other.doc
        off = {k: len(d.get(k, [])) for k in
               ('bufferViews', 'accessors', 'images', 'samplers', 'textures', 'materials', 'meshes', 'skins', 'nodes',
                'animations')}
        for k in off:
            d.setdefault(k, [])
        base = len(self.blob) + (-len(self.blob) % 4)
        self.blob += b'\0' * (-len(self.blob) % 4)
        self.blob += other.blob
        for v in o['bufferViews']:
            v = dict(v)
            v['byteOffset'] = v.get('byteOffset', 0) + base
            d['bufferViews'].append(v)
        for a in o['accessors']:
            a = dict(a)
            if 'bufferView' in a:
                a['bufferView'] += off['bufferViews']
            d['accessors'].append(a)
        for i in o.get('images', []):
            i = dict(i)
            if 'bufferView' in i:
                i['bufferView'] += off['bufferViews']
            d['images'].append(i)
        d['samplers'] += [dict(s) for s in o.get('samplers', [])]
        for t in o.get('textures', []):
            t = dict(t)
            t['source'] += off['images']
            if 'sampler' in t:
                t['sampler'] += off['samplers']
            d['textures'].append(t)
        for m in o.get('materials', []):
            m = _shift_textures(m, off['textures'])
            d['materials'].append(m)
        for m in o['meshes']:
            m = {'name': m.get('name'), 'primitives': [dict(p) for p in m['primitives']]}
            for p in m['primitives']:
                p['attributes'] = {k: v + off['accessors'] for k, v in p['attributes'].items()}
                if 'indices' in p:
                    p['indices'] += off['accessors']
                if 'material' in p:
                    p['material'] += off['materials']
            d['meshes'].append(m)
        for s in o.get('skins', []):
            s = dict(s)
            s['joints'] = [j + off['nodes'] for j in s['joints']]
            s['inverseBindMatrices'] += off['accessors']
            if 'skeleton' in s:
                s['skeleton'] += off['nodes']
            d['skins'].append(s)
        for n in o['nodes']:
            n = dict(n)
            if 'children' in n:
                n['children'] = [c + off['nodes'] for c in n['children']]
            if 'mesh' in n:
                n['mesh'] += off['meshes']
            if 'skin' in n:
                n['skin'] += off['skins']
            d['nodes'].append(n)
        for a in o.get('animations', []):
            a = dict(a)
            a['channels'] = [dict(c, target=dict(c['target'], node=c['target']['node'] + off['nodes']),
                                  sampler=c['sampler']) for c in a['channels']]
            a['samplers'] = [dict(s, input=s['input'] + off['accessors'], output=s['output'] + off['accessors'])
                             for s in a['samplers']]
            d['animations'].append(a)
        roots = [r + off['nodes'] for r in o['scenes'][o.get('scene', 0)]['nodes']]
        return off['nodes'], roots, off['animations']

    def node(self, name, lo=0, hi=None):
        """Index of the node called `name` (searched in [lo, hi) so two models' same-named joints do not clash)."""
        for i in range(lo, len(self.doc['nodes']) if hi is None else hi):
            if self.doc['nodes'][i].get('name') == name:
                return i
        raise KeyError(name)

    def reparent(self, child, parent, local=None):
        """Make `child` a child of `parent` (taken out of the scene roots / its old parent); `local` = new local matrix."""
        d = self.doc
        for n in d['nodes']:
            if child in n.get('children', []):
                n['children'].remove(child)
        for s in d['scenes']:
            if child in s['nodes']:
                s['nodes'].remove(child)
        d['nodes'][parent].setdefault('children', []).append(child)
        if local is not None:
            self.set_local(child, local)

    def set_local(self, i, m):
        n = self.doc['nodes'][i]
        n.pop('matrix', None)
        t, q, s = decompose(m)
        n['translation'], n['rotation'], n['scale'] = [float(x) for x in t], [float(x) for x in q], [float(x) for x in s]

    def add_node(self, name, parent=None, mesh=None, local=None):
        d = self.doc
        n = {'name': name}
        if mesh is not None:
            n['mesh'] = mesh
        d['nodes'].append(n)
        i = len(d['nodes']) - 1
        if parent is None:
            d['scenes'][0]['nodes'].append(i)
        else:
            d['nodes'][parent].setdefault('children', []).append(i)
        if local is not None:
            self.set_local(i, local)
        return i

    # ---- poses ------------------------------------------------------------------------------------
    def set_rest_pose(self, clip, time=0.0, lo=0, hi=None):
        """Write the values `clip` has at `time` into the rest transforms of the nodes in [lo, hi)."""
        for i, v in gp.sample_clip(self.doc, self.blob, clip, time).items():
            if i < lo or (hi is not None and i >= hi):
                continue
            n = self.doc['nodes'][i]
            n.pop('matrix', None)
            for path, val in v.items():
                n[path] = [float(x) for x in val]

    def rebind_rest(self):
        """Make the current rest pose the bind pose of every skin.

        Vertices become what the skin gives now (positions and normals in model space) and each
        inverse bind matrix the inverse of its joint's rest world matrix, so rest pose = how the
        model looks now. Needed because the loader measures a model by its raw vertices (the unit's
        scale comes from that), and after a rest pose or a parent was changed they no longer match.
        """
        d = self.doc
        world = gp.world_matrices(d)
        done = {}
        for ni, n in enumerate(d['nodes']):
            if 'skin' not in n or 'mesh' not in n:
                continue
            sk = d['skins'][n['skin']]
            ibm = self.read(sk['inverseBindMatrices']).astype(float).reshape(-1, 4, 4).transpose(0, 2, 1)
            jm = np.array([world[j] @ ibm[k] for k, j in enumerate(sk['joints'])])
            for p in d['meshes'][n['mesh']]['primitives']:
                if id(p) in done:
                    continue
                done[id(p)] = 1
                at = p['attributes']
                pos = self.read(at['POSITION']).astype(float)
                nor = self.read(at['NORMAL']).astype(float) if 'NORMAL' in at else None
                joints = self.read(at['JOINTS_0'])
                w = self.read(at['WEIGHTS_0']).astype(float)
                acc = d['accessors'][at['WEIGHTS_0']]
                if acc['componentType'] != FLOAT:
                    w = w / float({5121: 255, 5123: 65535}[acc['componentType']])
                w = w / np.maximum(w.sum(axis=1, keepdims=True), 1e-9)
                ph = np.hstack([pos, np.ones((len(pos), 1))])
                npos = np.zeros_like(pos)
                nnor = np.zeros_like(pos)
                for c in range(4):
                    m = jm[joints[:, c]]
                    npos += w[:, c:c + 1] * np.einsum('nij,nj->ni', m, ph)[:, :3]
                    if nor is not None:
                        nnor += w[:, c:c + 1] * np.einsum('nij,nj->ni', m[:, :3, :3], nor)
                at['POSITION'] = self.add_accessor(npos)
                if nor is not None:
                    nnor /= np.maximum(np.linalg.norm(nnor, axis=1, keepdims=True), 1e-9)
                    at['NORMAL'] = self.add_accessor(nnor)
        for sk in d.get('skins', []):             # after all meshes: several meshes share a skin
            inv = np.array([np.linalg.inv(world[j]) for j in sk['joints']])
            sk['inverseBindMatrices'] = self.add_accessor(inv.transpose(0, 2, 1).reshape(-1, 16))

    # ---- animation --------------------------------------------------------------------------------
    def new_clip(self, name, pieces, duration):
        """Add a clip made of tracks. `pieces`: list of (source clip, node filter, speed) or
        (None, None, [(node, path, times, values), ...]) for tracks made by hand.

        A source track's times are divided by `speed`; if shorter than `duration` it is repeated (looped)
        up to it, if longer it is cut. Only nodes for which filter(node index) is true are taken.
        """
        d = self.doc
        channels, samplers = [], []
        for src, flt, extra in pieces:
            if src is None:
                tracks = extra
            else:
                tracks = []
                for ch in src['channels']:
                    node = ch['target']['node']
                    if not flt(node):
                        continue
                    s = src['samplers'][ch['sampler']]
                    tracks.append((node, ch['target']['path'], self.read(s['input'])[:, 0] / extra, self.read(s['output'])))
            for node, path, times, vals in tracks:
                times, vals = _fit_track(np.asarray(times, float), np.asarray(vals, float), duration)
                samplers.append({'input': self.add_accessor(times.reshape(-1, 1)),
                                 'output': self.add_accessor(vals), 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': node, 'path': path}})
        d.setdefault('animations', []).append({'name': name, 'channels': channels, 'samplers': samplers})
        return len(d['animations']) - 1

    def add_tracks(self, clip_index, tracks):
        """Add hand-made tracks [(node, path, times, values), ...] to an existing clip (times as given)."""
        clip = self.doc['animations'][clip_index]
        for node, path, times, vals in tracks:
            clip['samplers'].append({'input': self.add_accessor(np.asarray(times, float).reshape(-1, 1)),
                                     'output': self.add_accessor(np.asarray(vals, float)), 'interpolation': 'LINEAR'})
            clip['channels'].append({'sampler': len(clip['samplers']) - 1, 'target': {'node': node, 'path': path}})


def _fit_track(times, vals, duration):
    """Loop (or cut) a track so that it runs exactly 0..duration."""
    length = float(times[-1])
    if length <= 1e-6 or len(times) == 1:
        return np.array([0.0, duration]), np.vstack([vals[0], vals[0]])
    ts, vs, k = [], [], 0
    while k * length < duration - 1e-6:
        for t, v in zip(times, vals):
            if k > 0 and t == times[0]:
                continue                      # the first key repeats the last one of the round before
            ts.append(t + k * length)
            vs.append(v)
        k += 1
    ts, vs = np.array(ts), np.array(vs)
    if ts[-1] > duration + 1e-6:              # cut at `duration`, with a key exactly there
        j = int(np.searchsorted(ts, duration))
        f = (duration - ts[j - 1]) / (ts[j] - ts[j - 1])
        end = vs[j - 1] + f * (vs[j] - vs[j - 1])
        if vals.shape[1] == 4:
            end = end / np.linalg.norm(end)
        ts, vs = np.append(ts[:j], duration), np.vstack([vs[:j], end])
    return ts, vs


def _shift_textures(material, by):
    """A copy of a glTF material with its texture indices moved up by `by`."""
    m = copy.deepcopy(material)
    infos = [m.get('normalTexture'), m.get('occlusionTexture'), m.get('emissiveTexture')]
    pbr = m.get('pbrMetallicRoughness')
    if pbr:
        infos += [pbr.get('baseColorTexture'), pbr.get('metallicRoughnessTexture')]
    for t in infos:
        if t:
            t['index'] += by
    return m


def decompose(m):
    """(translation, quaternion xyzw, scale) of a matrix without shear."""
    t = m[:3, 3].copy()
    s = np.linalg.norm(m[:3, :3], axis=0)
    r = m[:3, :3] / s
    tr = np.trace(r)
    if tr > 0:
        k = 0.5 / np.sqrt(tr + 1.0)
        q = [(r[2, 1] - r[1, 2]) * k, (r[0, 2] - r[2, 0]) * k, (r[1, 0] - r[0, 1]) * k, 0.25 / k]
    elif r[0, 0] > r[1, 1] and r[0, 0] > r[2, 2]:
        k = 2.0 * np.sqrt(1.0 + r[0, 0] - r[1, 1] - r[2, 2])
        q = [0.25 * k, (r[0, 1] + r[1, 0]) / k, (r[0, 2] + r[2, 0]) / k, (r[2, 1] - r[1, 2]) / k]
    elif r[1, 1] > r[2, 2]:
        k = 2.0 * np.sqrt(1.0 + r[1, 1] - r[0, 0] - r[2, 2])
        q = [(r[0, 1] + r[1, 0]) / k, 0.25 * k, (r[1, 2] + r[2, 1]) / k, (r[0, 2] - r[2, 0]) / k]
    else:
        k = 2.0 * np.sqrt(1.0 + r[2, 2] - r[0, 0] - r[1, 1])
        q = [(r[0, 2] + r[2, 0]) / k, (r[1, 2] + r[2, 1]) / k, 0.25 * k, (r[1, 0] - r[0, 1]) / k]
    q = np.array(q)
    return t, q / np.linalg.norm(q), s


def translate(x, y, z):
    m = np.eye(4)
    m[:3, 3] = (x, y, z)
    return m


def scaling(s):
    m = np.eye(4)
    m[0, 0] = m[1, 1] = m[2, 2] = s
    return m


def rotation(axis, degrees):
    a = np.radians(degrees)
    x, y, z = np.array(axis, float) / np.linalg.norm(axis)
    c, s = np.cos(a), np.sin(a)
    m = np.eye(4)
    m[:3, :3] = [[c + x * x * (1 - c), x * y * (1 - c) - z * s, x * z * (1 - c) + y * s],
                 [y * x * (1 - c) + z * s, c + y * y * (1 - c), y * z * (1 - c) - x * s],
                 [z * x * (1 - c) - y * s, z * y * (1 - c) + x * s, c + z * z * (1 - c)]]
    return m
