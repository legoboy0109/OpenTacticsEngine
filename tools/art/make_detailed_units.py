"""The unit models of the "Detailed 3D" style: KayKit characters, cut down, some with weapons or a mount.

Called by make_detailed_style.py. Plain units are a character with parts left out (trim_glb). Combined
units add pieces from other packs with glb_attach / glb_merge:

  archer    Rogue_Hooded + Quaternius "Wooden Bow" hung on the left hand joint
  soldier   Barbarian + Quaternius "Spear" hung on the right hand joint
  cavalier  Knight seated on the Quaternius "Horse" (the rider's skeleton is a child of the horse's Body joint)
  sky_rider Mage seated on the same horse, with generated wings (glb_wings.py) that flap

One clip animates both skeletons of a rider model: the mount's own clip, plus (for the attack) the rider's
arm and chest tracks. Idle and move leave the rider in its seated pose and let the horse's body carry it.
"""
import os

import numpy as np

import glb_attach as ga
import glb_pose as gp
import glb_wings
import trim_glb

BASE_HEIGHT = 1.15      # squares tall, a plain character
RIDE_FIT = 0.85         # mounted units are scaled down so a horse's length fits a square

# Parts of the KayKit characters that every unit leaves out (they all carry every weapon, hat and shield).
_SWORDS = ['1H_Sword_Offhand', 'Badge_Shield', 'Rectangle_Shield', 'Round_Shield', 'Spike_Shield']

# unit -> (character file, parts left out, clips state=clip). Plain units; the others are in COMBINED.
PLAIN = {
    'lord': ('Knight', ['1H_Sword_Offhand', 'Rectangle_Shield', 'Round_Shield', 'Spike_Shield', '2H_Sword', 'Knight_Helmet'],
             {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Chop', 'Die': 'Death_A'}),
    'knight': ('Knight', ['1H_Sword_Offhand', 'Badge_Shield', 'Round_Shield', 'Spike_Shield', '2H_Sword'],
               {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Stab', 'Die': 'Death_A'}),
    'fighter': ('Barbarian', ['1H_Axe_Offhand', 'Barbarian_Round_Shield', '1H_Axe', 'Mug'],
                {'Idle': '2H_Melee_Idle', 'Move': 'Walking_A', 'Attack': '2H_Melee_Attack_Chop', 'Die': 'Death_B'}),
}

# Joints a rider moves while it attacks (the legs stay seated); the rest pose of these comes from the `hold` clip.
UPPER = {'spine', 'chest', 'head', 'upperarm.l', 'upperarm.r', 'lowerarm.l', 'lowerarm.r', 'wrist.l', 'wrist.r',
         'hand.l', 'hand.r'}

# The horse's clips (the pack's own names) and the unit states that use them.
HORSE_CLIPS = {'Idle': 'Idle', 'Walk': 'Walk', 'Run': 'Run', 'Attack': 'Headbutt', 'Die': 'Death', 'Fly': 'Jump_Loop'}

RIDERS = {
    'cavalier': dict(char='Knight', drop=_SWORDS + ['1H_Sword', 'Knight_Cape'], move='Run', hold='2H_Melee_Idle',
                     attack='2H_Melee_Attack_Slice', seat=(0.0, 1.72, -0.25), size=1.05, wings=False),
    'sky_rider': dict(char='Mage', drop=['Spellbook', 'Spellbook_open', '1H_Wand'], move='Fly', hold='Idle',
                    attack='Spellcast_Shoot', seat=(0.0, 1.72, -0.25), size=1.05, wings=True),
}

def _blend(carry, aim, keys):
    """f(time, duration) -> directions that go from `carry` to `aim` and back along `keys` = [(time, 0..1)]."""
    ts, ws = zip(*keys)

    def f(t, dur):
        w = float(np.interp(t, ts, ws))
        return [(1 - w) * np.array(c) + w * np.array(a) for c, a in zip(carry, aim)]
    return f


_UP = _blend(([0, 0.97, 0.25], [1, 0, 0]), ([0, 0.97, 0.25], [1, 0, 0]), [(0, 0)])      # spear carried upright, leaning forward
_THRUST = _blend(([0, 0.97, 0.25], [1, 0, 0]), ([0, 0.30, 0.95], [1, 0, 0]),
                 [(0, 0), (0.3, 0.55), (0.45, 1), (0.9, 1), (1.5, 0), (9, 0)])
_BOW_CARRY = ([0, 0.87, 0.5], [1, 0, 0])
_BOW_AIM = ([0, 1, 0], [0, 0, 1])                                                        # upright, its back toward the target
_AIM = _blend(_BOW_CARRY, _BOW_AIM, [(0, 0), (0.25, 1), (0.8, 1), (1.07, 0.6), (9, 0.6)])
_CARRY = _blend(_BOW_CARRY, _BOW_CARRY, [(0, 0)])

ARMED = {
    'archer': dict(char='Rogue_Hooded', drop=['Knife_Offhand', '1H_Crossbow', '2H_Crossbow', 'Knife', 'Throwable'],
                   weapon=('Bow.glb', 'handslot.l'), hang=dict(length=1.5, grip=0.5, long_axis=1),
                   dirs={'Idle': _CARRY, 'Move': _CARRY, 'Attack': _AIM, 'Die': _CARRY},
                   clips={'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '2H_Ranged_Shoot', 'Die': 'Death_A'}),
    'soldier': dict(char='Barbarian', drop=['1H_Axe_Offhand', '2H_Axe', 'Mug', 'Barbarian_Hat', 'Barbarian_Cape', '1H_Axe'],
                    weapon=('Spear.glb', 'handslot.r'), hang=dict(length=2.7, grip=0.30, long_axis=1),
                    dirs={'Idle': _UP, 'Move': _UP, 'Attack': _THRUST, 'Die': _UP},
                    clips={'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Stab', 'Die': 'Death_A'}),
}


def build_plain(kaykit, uid):
    char, drop, clips = PLAIN[uid]
    doc, blob = trim_glb.read_glb(os.path.join(kaykit, char + '.glb'))
    return trim_glb.trim(doc, blob, list(clips.items()), set(drop))


def build_armed(kaykit, weapons, uid):
    r = ARMED[uid]
    g = ga.load_trimmed(os.path.join(kaykit, r['char'] + '.glb'), r['drop'], list(r['clips'].items()))
    wfile, joint = r['weapon']
    weapon = ga.load_trimmed(os.path.join(weapons, wfile))
    pivot = ga.hang_weapon(g, weapon, joint, **r['hang'])
    ga.steer_weapon(g, pivot, joint, r['dirs'])
    return finish(g, list(r['clips']))


def build_rider(kaykit, animals, uid, r=None):
    """A rider on the horse. `r` is the recipe (default: RIDERS[uid]); its optional `extra(g, rider_lo, rider_hi)` runs
    once the rider is seated (dress it: parts, blocks, tints) and `late(g)` once the clips exist (steer a hung weapon)."""
    r = r or RIDERS[uid]
    horse = ga.load_trimmed(os.path.join(animals, 'Horse.glb'), keep=[(k, ga.HORSE_PREFIX + v) for k, v in HORSE_CLIPS.items()])
    rider = ga.load_trimmed(os.path.join(kaykit, r['char'] + '.glb'), r['drop'],
                            [('Sit', 'Sit_Chair_Pose'), ('Hold', r['hold']), ('Attack', r['attack'])])
    clips = {c['name']: c for c in horse.doc['animations']}
    off, roots, coff = horse.append(rider)
    g = horse
    hi = len(g.doc['nodes'])
    rclip = {c['name']: c for c in g.doc['animations'][coff:]}
    # rest pose of the rider: seated, arms as in the `hold` clip, legs apart so they straddle the horse
    g.set_rest_pose(rclip['Sit'], 0.0, lo=off)
    names = {g.doc['nodes'][i].get('name'): i for i in range(off, hi)}
    hold = {i: v for i, v in gp.sample_clip(g.doc, g.blob, rclip['Hold'], 0.0).items()
            if g.doc['nodes'][i].get('name') in UPPER and i >= off}
    for i, v in hold.items():
        for path, val in v.items():
            g.doc['nodes'][i][path] = [float(x) for x in val]
    for side, sign in (('l', 1), ('r', -1)):
        ga.rotate_joint(g, 'upperleg.' + side, (0, 0, 1), sign * 38, off, hi)
    ga.seat_rider(g, roots[0], off, hi, 'Body', r['seat'], r['size'])
    if r.get('extra'):
        r['extra'](g, off, hi)
    # clips: horse's own, plus the rider's arms for the attack
    horse_nodes = lambda n: n < off
    rider_upper = lambda n: n >= off and g.doc['nodes'][n].get('name') in UPPER
    wing_tracks = glb_wings.add_wings(g, 'Body') if r['wings'] else None
    states = {'Idle': 'Idle', 'Move': r['move'], 'Attack': 'Attack', 'Die': 'Die'}
    base = len(g.doc['animations'])
    for state, hname in states.items():
        h = clips[hname]
        dur = gp.clip_length(g.doc, g.blob, h)
        pieces = [(h, horse_nodes, 1.0)]
        if state == 'Attack':
            a = rclip['Attack']
            pieces.append((a, rider_upper, gp.clip_length(g.doc, g.blob, a) / dur))
        if wing_tracks:
            pieces.append((None, None, wing_tracks(state, dur)))
        g.new_clip(state, pieces, dur)
    g.doc['animations'] = g.doc['animations'][base:]
    g.rebind_rest()
    if r.get('late'):
        r['late'](g)
    return finish(g, list(states))


def finish(g, states):
    """Drop what nothing uses any more (the clips not named in `states` too) and return (doc, blob) for write_glb."""
    return trim_glb.trim(g.doc, bytes(g.blob), [(s, s) for s in states], set())


def _top(doc, blob):
    return float(gp.model_points(doc, blob)[:, 1].max())


def build_all(kaykit, animals, weapons, out_dir):
    """Write every unit's .glb into out_dir. Returns {unit: height in squares}.

    The engine scales a model so its rest pose (measured by its raw vertices) is `height` squares tall.
    A unit that is the same character with something added keeps the character's scale: its height is
    scaled by how much taller the new rest pose is. A rider's is also made smaller (RIDE_FIT) so the mount fits a square.
    """
    heights = {}
    jobs = [(u, build_plain(kaykit, u), PLAIN[u][0], PLAIN[u][1], 1.0) for u in PLAIN]
    jobs += [(u, build_armed(kaykit, weapons, u), r['char'], r['drop'], 1.0) for u, r in ARMED.items()]
    jobs += [(u, build_rider(kaykit, animals, u), r['char'], r['drop'], RIDE_FIT) for u, r in RIDERS.items()]
    for uid, (doc, blob), char, drop, fit in jobs:
        base = trim_glb.trim(*trim_glb.read_glb(os.path.join(kaykit, char + '.glb')), [], set(drop))
        heights[uid] = round(BASE_HEIGHT * _top(doc, blob) / _top(*base) * fit, 3)
        trim_glb.write_glb(os.path.join(out_dir, uid + '.glb'), doc, blob)
    return heights
