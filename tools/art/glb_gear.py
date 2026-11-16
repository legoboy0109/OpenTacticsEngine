"""More ways to dress a character model: a part cut from another pack, generated blocks, a colour tint.

Used by make_detailed_promoted.py. All of it works on a Glb (glb_merge) in model space (+Y up, +Z forward).

  load_part(path, node)         one rigid part (a sword, a shield, a helmet) of a pack, as a model of its own
  attach_part(g, part, joint)   put it under a joint of `g` exactly where the pack had it (the KayKit characters
                                share one skeleton, so a part of the Knight fits the Barbarian's hand), optionally bigger
  add_blocks(g, joint, blocks)  generated boxes (a plume, a collar, a quiver, barding) that follow `joint`
  scale_node(g, name, f)        make a part bigger about its own origin
  tint(g, material, rgb)        multiply a material's colour (the packs colour a model through one texture)
"""
import numpy as np

import glb_merge as gm
import glb_pose as gp
import trim_glb


def load_part(path, node_name):
    """A Glb holding only the rigid mesh node `node_name` of the file (no skeleton, no clips, no parent)."""
    doc, blob = trim_glb.read_glb(path)
    idx = gp.node_index(doc, node_name)
    n = {k: v for k, v in doc['nodes'][idx].items() if k not in ('children', 'skin')}
    if 'mesh' not in n:
        raise ValueError('%s: node %s has no mesh' % (path, node_name))
    doc['nodes'] = [n]
    doc['scenes'] = [{'nodes': [0]}]
    doc['scene'] = 0
    doc.pop('skins', None)
    doc.pop('animations', None)
    doc, blob = trim_glb.trim(doc, blob, [], set())
    return gm.Glb(doc, blob)


def attach_part(g, part, joint, scale=1.0, lo=0, hi=None):
    """Append `part` and hang it under the node called `joint`, keeping the placement its pack gave it
    (the part's own local transform, which is relative to the same joint in the pack), made `scale` times bigger."""
    _, roots, _ = g.append(part)
    j = g.node(joint, lo, hi)
    for r in roots:
        local = gp.trs_matrix(g.doc['nodes'][r])
        g.reparent(r, j, local @ gm.scaling(scale) if scale != 1.0 else local)
    return roots


def _box(center, size, rot=(0, 0, 0)):
    """Triangles of a box: (positions (36, 3), normals (36, 3)), turned by `rot` degrees (x, then y, then z) about its centre."""
    hx, hy, hz = (s / 2 for s in size)
    r = gm.rotation((0, 0, 1), rot[2])[:3, :3] @ gm.rotation((0, 1, 0), rot[1])[:3, :3] @ gm.rotation((1, 0, 0), rot[0])[:3, :3]
    pos, nor = [], []
    faces = [((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0)), ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
             ((-1, 0, 0), (0, 0, 1), (0, 1, 0)), ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1))]
    ext = np.array([hx, hy, hz])
    for n, u, v in faces:
        n, u, v = (np.array(a, float) for a in (n, u, v))
        c = n * ext
        quad = [c - u * ext - v * ext, c + u * ext - v * ext, c + u * ext + v * ext, c - u * ext + v * ext]
        for tri in ((0, 1, 2), (0, 2, 3)):
            for k in tri:
                pos.append(r @ quad[k] + center)
                nor.append(r @ n)
    return np.array(pos), np.array(nor)


def add_blocks(g, joint, blocks, name='gear', local=False, lo=0, hi=None):
    """Add generated boxes under the node `joint` (searched in nodes lo..hi). `blocks`: list of (center, size, (r, g, b) 0..1
    [, rot degrees]); the centres are in model space at the rest pose, or, with `local`, in the joint's own frame
    (origin at the joint; use it where the joint is scaled or turned, like a seated rider's or the horse's, whose joints
    are 100 times scaled). They move with the joint and with whatever it hangs from."""
    d = g.doc
    j = g.node(joint, lo, hi)
    inv = np.eye(4) if local else np.linalg.inv(gp.world_matrices(d)[j])
    by_colour = {}
    for b in blocks:
        center, size, colour = b[:3]
        pos, nor = _box(np.array(center, float), size, b[3] if len(b) > 3 else (0, 0, 0))
        pos = (inv[:3, :3] @ pos.T).T + inv[:3, 3]
        nor = (inv[:3, :3] @ nor.T).T
        nor /= np.linalg.norm(nor, axis=1, keepdims=True)
        by_colour.setdefault(tuple(colour), []).append((pos, nor))
    prims = []
    for colour, items in by_colour.items():
        d.setdefault('materials', []).append({'name': '%s.%02x%02x%02x' % ((name,) + tuple(int(c * 255) for c in colour)),
                                              'pbrMetallicRoughness': {'baseColorFactor': list(colour) + [1.0],
                                                                       'metallicFactor': 0.0, 'roughnessFactor': 0.8}})
        pos = np.vstack([p for p, _ in items])
        nor = np.vstack([n for _, n in items])
        idx = np.arange(len(pos), dtype=np.uint32).reshape(-1, 1)
        prims.append({'attributes': {'POSITION': g.add_accessor(pos, target=34962), 'NORMAL': g.add_accessor(nor, target=34962)},
                      'indices': g.add_accessor(idx, component=gm.UINT, target=34963), 'material': len(d['materials']) - 1})
    d['meshes'].append({'name': name, 'primitives': prims})
    return g.add_node(name, parent=j, mesh=len(d['meshes']) - 1)


def tint(g, material, rgb, lo=0, hi=None):
    """Multiply the base colour of the material(s) named `material` (only those at index lo..hi, so a rider's materials can be told from the mount's) by `rgb`."""
    for m in g.doc['materials'][lo:hi]:
        if m.get('name') == material:
            pbr = m.setdefault('pbrMetallicRoughness', {})
            old = pbr.get('baseColorFactor', [1, 1, 1, 1])
            pbr['baseColorFactor'] = [float(o * c) for o, c in zip(old, list(rgb) + [1.0])]


def scale_node(g, node_name, factor):
    """Make the rigid part called `node_name` `factor` times bigger about its own origin (a grip, for a weapon)."""
    n = g.doc['nodes'][g.node(node_name)]
    n['scale'] = [float(v) * factor for v in n.get('scale', [1, 1, 1])]
