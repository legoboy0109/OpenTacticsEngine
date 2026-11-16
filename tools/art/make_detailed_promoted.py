"""The "Detailed 3D" models of the seven promoted / summoned types, built from the same CC0 packs as the others.

Called by make_detailed_style.py. Same pieces as make_detailed_units.py, cut and dressed differently (glb_gear.py):

  captain      Knight: closed helm with a generated plume, generated gold pauldrons, cape, badge shield, sword; bigger
  lancer       Knight on the Quaternius horse: helm, tower shield, the Quaternius spear as a long couched lance; the horse
               armoured with generated barding (breastplate, hanging cloth, face plate) and tinted steel-grey
  outrider     Rogue_Hooded on the horse tinted palomino: knife as a sword, a generated scarf, no armour, smaller
  marksman     Rogue_Hooded with the Quaternius bow hung at twice the archer's size, a generated quiver and
               bandolier, a feather in the hood; darker
  brute        Barbarian: no hat, generated horns, the two-handed axe 1.5 times its size, cape; 22 % bigger, ruddy
  houndmaster  Barbarian: the Knight's sword in the right hand, cape, a generated fur collar, whistle and leash; brown
  hound        the Quaternius "Husky" (CC0, https://poly.pizza/m/wcWiuEqwzq): a generated collar, tinted tan;
               its own Idle / Gallop / Attack / Death

A unit that is a known character with things added keeps that character's scale (see make_detailed_units.build_all);
SCALE makes a kind bigger or smaller than that. The Husky is not a KayKit character: its height is chosen outright.
"""
import os

import glb_attach as ga
import glb_gear as gear
import make_detailed_units as mdu
import trim_glb

# Colours of generated parts (0..1)
GOLD, CRIMSON, STEEL = (0.80, 0.55, 0.10), (0.50, 0.08, 0.08), (0.45, 0.48, 0.55)
STEEL_D = (0.25, 0.27, 0.32)
BONE, FUR, LEATHER = (0.78, 0.72, 0.58), (0.78, 0.74, 0.64), (0.26, 0.14, 0.07)
WHITE = (0.90, 0.90, 0.86)

SCALE = {'captain': 1.08, 'lancer': 1.08, 'outrider': 0.96, 'marksman': 1.02, 'brute': 1.22, 'houndmaster': 1.0}
HOUND_HEIGHT = 0.78          # squares: the rest pose's top (the ears) stands this high
HUSKY_CLIPS = [('Idle', 'Idle'), ('Move', 'Gallop'), ('Attack', 'Attack'), ('Die', 'Death')]
SHIELDS = ['Badge_Shield', 'Rectangle_Shield', 'Round_Shield', 'Spike_Shield']


def _kk(kaykit, char):
    return os.path.join(kaykit, char + '.glb')


def _only(drop_all, *keep):
    return [d for d in drop_all if d not in keep]


KNIGHT_PARTS = ['1H_Sword_Offhand'] + SHIELDS + ['1H_Sword', '2H_Sword', 'Knight_Helmet', 'Knight_Cape']
BARB_PARTS = ['1H_Axe_Offhand', 'Barbarian_Round_Shield', '1H_Axe', '2H_Axe', 'Mug', 'Barbarian_Hat', 'Barbarian_Cape']
ROGUE_PARTS = ['Knife_Offhand', '1H_Crossbow', '2H_Crossbow', 'Knife', 'Throwable', 'Rogue_Cape']


# ---------------------------------------------------------------------------- on foot (the head is 1.24..2.4 high)
def dress_captain(g, ctx):
    gear.add_blocks(g, 'head', [((0, 2.55, 0.0), (0.2, 0.12, 0.6), GOLD),                              # crest bar and a swept plume
                                ((0, 2.74, -0.08), (0.16, 0.34, 0.4), CRIMSON, (12, 0, 0)),
                                ((0, 2.94, -0.3), (0.16, 0.3, 0.42), CRIMSON, (34, 0, 0)),
                                ((0, 3.0, -0.58), (0.16, 0.26, 0.38), CRIMSON, (58, 0, 0))], 'plume')
    gear.add_blocks(g, 'chest', [((s * 0.43, 1.25, 0.0), (0.34, 0.14, 0.46), GOLD, (0, 0, -s * 12)) for s in (1, -1)], 'pauldrons')
    gear.add_blocks(g, 'chest', [((0, 0.78, -0.5), (0.98, 1.4, 0.08), CRIMSON), ((0, 0.12, -0.5), (1.02, 0.08, 0.12), GOLD),
                                 ((0, 1.5, -0.46), (0.98, 0.1, 0.16), GOLD)], 'cape')                    # a long cape to the boots


def dress_brute(g, ctx):
    parts = [((0, 2.0, 0.0), (1.14, 0.12, 1.14), STEEL_D), ((0, 2.04, 0.58), (0.3, 0.2, 0.06), GOLD)]     # an iron band with horns
    for s in (1, -1):
        parts += [((s * 0.62, 2.18, 0.0), (0.24, 0.44, 0.24), BONE, (0, 0, -s * 25)),
                  ((s * 0.84, 2.52, 0.0), (0.18, 0.42, 0.18), BONE, (0, 0, -s * 55))]
    gear.add_blocks(g, 'head', parts, 'horns')


def dress_houndmaster(g, ctx):
    fur2 = (0.62, 0.58, 0.50)
    cap = (0.40, 0.30, 0.20)
    collar = [((0, 1.3, -0.3), (1.0, 0.3, 0.3), FUR), ((0, 1.3, 0.32), (1.0, 0.3, 0.3), FUR),
              ((0.46, 1.3, 0.0), (0.3, 0.3, 0.7), FUR), ((-0.46, 1.3, 0.0), (0.3, 0.3, 0.7), FUR),
              ((0, 1.18, 0.36), (0.9, 0.16, 0.24), fur2), ((0, 1.18, -0.34), (0.9, 0.16, 0.24), fur2),
              ((0.5, 1.18, 0.0), (0.22, 0.16, 0.6), fur2), ((-0.5, 1.18, 0.0), (0.22, 0.16, 0.6), fur2)]
    gear.add_blocks(g, 'chest', collar + [((0.1, 1.2, 0.5), (0.1, 0.1, 0.1), GOLD),                    # whistle on a cord
                                           ((0.1, 1.05, 0.44), (0.03, 0.34, 0.03), LEATHER)], 'collar')
    gear.add_blocks(g, 'head', [((0, 2.22, 0.0), (1.1, 0.28, 1.1), cap), ((0, 2.08, 0.0), (1.16, 0.12, 1.14), (0.52, 0.42, 0.30)),    # fur cap with flaps
                                ((0.6, 1.8, 0.0), (0.12, 0.55, 0.5), cap), ((-0.6, 1.8, 0.0), (0.12, 0.55, 0.5), cap)], 'cap')
    # the leash: from the left fist (the hand's long axis is its Y) hangs down, then trails along the ground
    gear.add_blocks(g, 'handslot.l', [((0, -0.4, 0.0), (0.07, 0.8, 0.07), LEATHER), ((0, -0.8, 0.14), (0.07, 0.07, 0.34), LEATHER),
                                      ((0.16, -0.8, 0.3), (0.34, 0.07, 0.07), LEATHER)], 'leash', local=True)


def dress_marksman(g, ctx):
    gear.add_blocks(g, 'chest', [((0.3, 1.15, -0.42), (0.24, 0.8, 0.24), LEATHER, (0, 0, -14)),       # quiver
                                 ((0.36, 1.62, -0.42), (0.12, 0.2, 0.12), WHITE, (0, 0, -14)),
                                 ((0, 1.02, 0.0), (0.1, 0.9, 0.7), LEATHER, (0, 0, 38))], 'quiver')    # bandolier
    gear.add_blocks(g, 'head', [((0.36, 2.3, -0.12), (0.1, 0.5, 0.12), WHITE, (0, 0, -22))], 'feather')


FOOT = {
    'captain': dict(char='Knight', drop=_only(KNIGHT_PARTS, 'Badge_Shield', '1H_Sword', 'Knight_Helmet', 'Knight_Cape'),
                    clips={'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Slice_Diagonal', 'Die': 'Death_A'},
                    dress=dress_captain, tint={'knight_texture': (1.0, 0.97, 0.9)}),
    'brute': dict(char='Barbarian', drop=_only(BARB_PARTS, '2H_Axe', 'Barbarian_Cape'),
                  clips={'Idle': '2H_Melee_Idle', 'Move': 'Walking_A', 'Attack': '2H_Melee_Attack_Chop', 'Die': 'Death_B'},
                  node_scale={'2H_Axe': 1.5}, dress=dress_brute, tint={'barbarian_texture': (1.04, 0.92, 0.88)}),
    'houndmaster': dict(char='Barbarian', drop=_only(BARB_PARTS, 'Barbarian_Cape'),
                        parts=[('Knight', '1H_Sword', 'handslot.r', 1.0)],
                        clips={'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Slice_Diagonal', 'Die': 'Death_A'},
                        dress=dress_houndmaster, tint={'barbarian_texture': (0.95, 0.85, 0.72)}),
}

# Bow and lance carry / aim directions (model space: long direction, belly direction)
MARKSMAN = dict(char='Rogue_Hooded', drop=['Knife_Offhand', '1H_Crossbow', '2H_Crossbow', 'Knife', 'Throwable'],
                weapon=('Bow.glb', 'handslot.l'), hang=dict(length=3.0, grip=0.5, long_axis=1),
                dirs={'Idle': mdu._CARRY, 'Move': mdu._CARRY, 'Attack': mdu._AIM, 'Die': mdu._CARRY},
                clips={'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '2H_Ranged_Shoot', 'Die': 'Death_A'},
                dress=dress_marksman, tint={'rogue_texture': (0.6, 0.66, 0.68)})


def build_foot(kaykit, weapons, uid):
    r = FOOT[uid]
    g = ga.load_trimmed(_kk(kaykit, r['char']), r['drop'], list(r['clips'].items()))
    for file, node, joint, scale in r.get('parts', []):
        gear.attach_part(g, gear.load_part(_kk(kaykit, file), node), joint, scale)
    for node, scale in r.get('node_scale', {}).items():
        gear.scale_node(g, node, scale)
    r['dress'](g, None)
    for material, rgb in r.get('tint', {}).items():
        gear.tint(g, material, rgb)
    return mdu.finish(g, list(r['clips']))


def build_marksman(kaykit, weapons, uid='marksman'):
    r = MARKSMAN
    g = ga.load_trimmed(_kk(kaykit, r['char']), r['drop'], list(r['clips'].items()))
    wfile, joint = r['weapon']
    pivot = ga.hang_weapon(g, ga.load_trimmed(os.path.join(weapons, wfile)), joint, **r['hang'])
    ga.steer_weapon(g, pivot, joint, r['dirs'])
    r['dress'](g, None)
    for material, rgb in r['tint'].items():
        gear.tint(g, material, rgb)
    return mdu.finish(g, list(r['clips']))


# ---------------------------------------------------------------------------- mounted
def _lance_dirs():
    carry = ([0, 0.5, 0.86], [1, 0, 0])
    aim = ([0, 0.12, 1.0], [1, 0, 0])
    return {'Idle': mdu._blend(carry, carry, [(0, 0)]), 'Move': mdu._blend(carry, carry, [(0, 0)]),
            'Attack': mdu._blend(carry, aim, [(0, 0), (0.25, 0.4), (0.4, 1), (0.9, 1), (1.3, 0), (9, 0)]),
            'Die': mdu._blend(carry, carry, [(0, 0)])}


def lancer_recipe(kaykit, weapons):
    held = {}

    def extra(g, off, hi):
        spear = ga.load_trimmed(os.path.join(weapons, 'Spear.glb'))
        held['pivot'] = ga.hang_weapon(g, spear, 'handslot.r', length=4.4, grip=0.3, long_axis=1)
        gear.add_blocks(g, 'Body', [((0, 1.78, -0.4), (1.1, 0.1, 1.3), CRIMSON),                                    # barding
                                    ((0.58, 1.42, -0.4), (0.07, 0.8, 1.2), CRIMSON), ((-0.58, 1.42, -0.4), (0.07, 0.8, 1.2), CRIMSON),
                                    ((0.6, 1.0, -0.4), (0.08, 0.08, 1.2), GOLD), ((-0.6, 1.0, -0.4), (0.08, 0.08, 1.2), GOLD),
                                    ((0, 1.45, 0.85), (0.9, 0.95, 0.18), STEEL), ((0, 1.5, 0.96), (0.3, 0.3, 0.06), GOLD),
                                    ((0, 1.72, -1.1), (0.86, 0.5, 0.3), STEEL)], 'barding')
        gear.add_blocks(g, 'Head', [((0, 2.2, 1.7), (0.5, 0.18, 1.0), STEEL), ((0, 2.34, 1.0), (0.16, 0.16, 0.3), GOLD)], 'faceplate')
        gear.add_blocks(g, 'Neck', [((0, 1.75, 0.62), (0.58, 1.1, 0.5), STEEL, (-10, 0, 0))], 'crinet')
        gear.add_blocks(g, 'hold.handslot.r', [((0, 3.0, 0.0), (0.04, 0.34, 0.56), CRIMSON)], 'pennant', local=True)
        gear.tint(g, 'AtlasMaterial', (0.8, 0.84, 0.95))

    def late(g):
        ga.steer_weapon(g, held['pivot'], 'handslot.r', _lance_dirs())

    keep = ['Rectangle_Shield', 'Knight_Helmet', 'Knight_Cape']
    return dict(char='Knight', drop=_only(KNIGHT_PARTS, *keep), move='Run', hold='2H_Melee_Idle',
                attack='2H_Melee_Attack_Stab', seat=(0.0, 1.72, -0.25), size=1.1, wings=False, extra=extra, late=late)


def outrider_recipe(kaykit, weapons):
    def extra(g, off, hi):
        gear.attach_part(g, gear.load_part(_kk(kaykit, 'Knight'), '1H_Sword'), 'handslot.r')              # a real sword, not the knife
        gear.tint(g, 'AtlasMaterial', (1.3, 1.12, 0.85))

    drop = ['Knife_Offhand', '1H_Crossbow', '2H_Crossbow', 'Throwable', 'Knife']
    return dict(char='Rogue_Hooded', drop=drop, move='Run', hold='2H_Melee_Idle', attack='1H_Melee_Attack_Slice_Horizontal',
                seat=(0.0, 1.72, -0.25), size=1.0, wings=False, extra=extra)


# ---------------------------------------------------------------------------- the hound
def build_hound(hound_dir):
    g = ga.load_trimmed(os.path.join(hound_dir, 'Husky.glb'), keep=HUSKY_CLIPS)
    gear.add_blocks(g, 'Neck2', [((0.01, 2.2, 1.52), (0.66, 0.24, 0.66), CRIMSON, (-35, 0, 0)),
                                 ((0.01, 2.0, 1.82), (0.1, 0.16, 0.06), GOLD, (-35, 0, 0))], 'collar')
    gear.tint(g, 'Material.001', (1.7, 1.15, 0.7))        # the grey fur turns tan, the white parts cream
    gear.tint(g, 'Material.003', (1.0, 0.92, 0.78))
    return mdu.finish(g, [k for k, _ in HUSKY_CLIPS])


# ---------------------------------------------------------------------------- build everything
def _write(out_dir, uid, doc, blob, base_char, base_drop, kaykit, scale, fit=1.0):
    base = trim_glb.trim(*trim_glb.read_glb(_kk(kaykit, base_char)), [], set(base_drop))
    trim_glb.write_glb(os.path.join(out_dir, uid + '.glb'), doc, blob)
    return round(mdu.BASE_HEIGHT * mdu._top(doc, blob) / mdu._top(*base) * fit * scale, 3)


def build_all(kaykit, animals, weapons, hound, out_dir):
    """Write the seven .glb files into out_dir; returns {unit: height in squares}."""
    heights = {}
    for uid, r in FOOT.items():
        heights[uid] = _write(out_dir, uid, *build_foot(kaykit, weapons, uid), r['char'], r['drop'], kaykit, SCALE[uid])
    heights['marksman'] = _write(out_dir, 'marksman', *build_marksman(kaykit, weapons), MARKSMAN['char'], MARKSMAN['drop'],
                                 kaykit, SCALE['marksman'])
    for uid, recipe in (('lancer', lancer_recipe(kaykit, weapons)), ('outrider', outrider_recipe(kaykit, weapons))):
        heights[uid] = _write(out_dir, uid, *mdu.build_rider(kaykit, animals, uid, recipe), recipe['char'], recipe['drop'],
                              kaykit, SCALE[uid], mdu.RIDE_FIT)
    doc, blob = build_hound(hound)
    trim_glb.write_glb(os.path.join(out_dir, 'hound.glb'), doc, blob)
    heights['hound'] = round(HOUND_HEIGHT, 3)
    return heights
