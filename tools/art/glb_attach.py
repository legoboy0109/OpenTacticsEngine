"""Recipes on top of glb_merge: hold a weapon, seat a rider, spread a pose.

Everything works on a Glb (glb_merge) and in model space (+Y up, +Z forward, as the engine reads it).
"""
import numpy as np

import glb_merge as gm
import glb_pose as gp
import trim_glb

HORSE_PREFIX = 'AnimalArmature|AnimalArmature|AnimalArmature|'


def load_trimmed(path, drop=(), keep=()):
    """A Glb of a file with the parts named in `drop` removed and only the clips in `keep` (new=old pairs)."""
    doc, blob = trim_glb.read_glb(path)
    doc, blob = trim_glb.trim(doc, blob, list(keep), set(drop))
    return gm.Glb(doc, blob)


def extent(glb):
    """(min, max) corner of the rest pose of a rigid or skinned model, as the loader would measure it."""
    p = gp.model_points(glb.doc, glb.blob)
    return p.min(0), p.max(0)


def hang_weapon(glb, weapon, joint, length, grip, long_axis=1, belly_axis=0, shift=(0.0, 0.0, 0.0)):
    """Hang the model `weapon` (a Glb) on the node called `joint`, on a pivot node you can turn.

    The weapon is scaled so its long side (`long_axis`: 0 x, 1 y, 2 z of its own frame) is `length`
    (in the character's units) and put on the pivot with the point `grip` of the way along that side
    (0 = one end, 1 = the other) at the pivot, the long side along the pivot's +Y and `belly_axis` (its
    bulging side, e.g. a bow's back) along +X. The pivot sits at `shift` in the joint's frame.
    Returns the pivot node. See steer_weapon() for keeping it pointing where the picture needs it.
    """
    lo, hi = extent(weapon)
    s = length / (hi[long_axis] - lo[long_axis])
    c = (lo + hi) / 2
    c[long_axis] = lo[long_axis] + grip * (hi[long_axis] - lo[long_axis])
    # rows of the turn: own long axis -> +Y, own belly axis -> +X
    x = np.zeros(3)
    x[belly_axis] = 1.0
    y = np.zeros(3)
    y[long_axis] = 1.0
    to_frame = np.eye(4)
    to_frame[:3, :3] = np.array([x, y, np.cross(x, y)])
    m = gm.scaling(s) @ to_frame @ gm.translate(*(-c))
    pivot = glb.add_node('hold.' + joint, parent=glb.node(joint), local=gm.translate(*shift))
    off, roots, _ = glb.append(weapon)
    for r in roots:
        glb.reparent(r, pivot, m @ gp.trs_matrix(glb.doc['nodes'][r]))
    return pivot


def _frame(long_dir, belly_dir):
    """3x3 matrix whose X is the belly direction, Y the long direction (belly made square to it), Z the cross."""
    y = np.asarray(long_dir, float)
    y = y / np.linalg.norm(y)
    x = np.asarray(belly_dir, float)
    x = x - y * np.dot(x, y)
    x = x / np.linalg.norm(x)
    return np.array([x, y, np.cross(x, y)]).T


def steer_weapon(glb, pivot, joint, states, step=1 / 24.0):
    """Keep a hung weapon pointing the way a picture of the unit needs it, in every clip.

    `states` maps a clip name to a function f(time, duration) -> (long direction, belly direction) in model
    space (the way the weapon's long side and its bulging side should point). The pivot's rotation is keyed
    every `step` seconds so that, whatever the hand does in that clip, the weapon keeps that direction in
    the hand (a rigid weapon on a fist would otherwise point wherever the wrist does).
    """
    hand = glb.node(joint)
    for ci, clip in enumerate(glb.doc['animations']):
        f = states.get(clip.get('name'))
        if not f:
            continue
        dur = gp.clip_length(glb.doc, glb.blob, clip)
        times = np.arange(0.0, dur + 1e-6, step)
        if times[-1] < dur - 1e-6:
            times = np.append(times, dur)
        quats = []
        for t in times:
            pw = gp.world_matrices(glb.doc, gp.posed_locals(glb.doc, glb.blob, clip, float(t)))[hand]
            rp = pw[:3, :3] / np.linalg.norm(pw[:3, :3], axis=0)
            want = _frame(*f(float(t), dur))
            m = np.eye(4)
            m[:3, :3] = rp.T @ want
            quats.append(gm.decompose(m)[1])
        quats = np.array(quats)
        for k in range(1, len(quats)):
            if np.dot(quats[k], quats[k - 1]) < 0:
                quats[k] = -quats[k]
        glb.add_tracks(ci, [(pivot, 'rotation', times, quats)])
    f0 = next(iter(states.values()))
    pw = gp.world_matrices(glb.doc)[hand]
    rp = pw[:3, :3] / np.linalg.norm(pw[:3, :3], axis=0)
    m = np.eye(4)
    m[:3, :3] = rp.T @ _frame(*f0(0.0, 1.0))
    t, q, s = gm.decompose(m)
    glb.doc['nodes'][pivot]['rotation'] = [float(v) for v in q]


def rotate_joint(glb, joint, axis, degrees, lo=0, hi=None):
    """Turn the joint (and what hangs below it) about a model-space axis through the joint's own position."""
    i = glb.node(joint, lo, hi)
    world = gp.world_matrices(glb.doc)
    pos = world[i][:3, 3]
    r = gm.translate(*pos) @ gm.rotation(axis, degrees) @ gm.translate(*(-pos))
    parent = gp.parents(glb.doc).get(i)
    pw = world[parent] if parent is not None else np.eye(4)
    glb.set_local(i, np.linalg.inv(pw) @ r @ world[i])


def seat_rider(glb, rider_root, rider_lo, rider_hi, mount_joint, hips_at, scale=1.0):
    """Move the rider's whole skeleton so its hips are at `hips_at` (model space) and make it a child of the
    mount's joint, so that it follows the joint as the mount moves. `rider_lo/hi` is the node range of the rider."""
    hips = glb.node('hips', rider_lo, rider_hi)
    world = gp.world_matrices(glb.doc)
    hp = world[hips][:3, 3]
    place = gm.translate(*hips_at) @ gm.scaling(scale) @ gm.translate(*(-hp))
    mount = glb.node(mount_joint, 0, rider_lo)
    glb.reparent(rider_root, mount, np.linalg.inv(world[mount]) @ place @ world[rider_root])
