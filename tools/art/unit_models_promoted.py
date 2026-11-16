"""The 3D models of the seven promoted / summoned unit types of the story demo (generated low-poly block figures).

Built with the parts and animation recipes of make_unit_models.py (which registers them: see the end of that file).
Each reads as a bigger or lighter version of the type it grows out of, in the same palette:

  captain      lord grown grander: closed helm with a tall plume, long cape, gold pauldrons, a longer blade
  lancer       cavalier in plate on a barded horse (breastplate, face plate, hanging cloth), couching a long pennant lance
  outrider     cavalier stripped light: pale horse, no barding, hood and streaming scarf, a slim sword
  marksman     archer with a tall bow, a drawn arrow, a full quiver with fletching and a bandolier
  brute        fighter much bigger: horned cap, fur mantle, bare arms, a great axe with two blades
  houndmaster  soldier turned handler: fur collar, long coat, whistle, sword, a leash hanging from the fist
  hound        a four-legged dog (not a person, not a horse): its own skeleton (spine, head, tail, four legs)

`SIZE` is how big each stands next to the type it grows out of (REF; 1.0 = the same scale): the engine fits a model's
whole rest pose (long weapon included) into one standard height, so model_height() turns it into the "height" for
graphics.json. The hound is measured against a lord, as a dog next to a person.
"""
import make_unit_models as mu
from make_unit_models import (Model, add, rgb, foot_base, mount_base, sword, spear, foot_clips, mount_clips, settle, Clip,
                              SIDEW, SIDEG, SIDE, STEEL, STEEL_D, STEEL_L, LEATHER, LEATHER_D, GOLD, WOOD, SKIN, SKIN_D,
                              GREY, GREY_D, WHITE, OLIVE, OLIVE_D, CLOTH, CLOTH_D, BLOND, EYE, HORSE_D, HORSE)

FUR, FUR_D = rgb(0xD9D2C0), rgb(0xA89F8A)
BONE = rgb(0xE8E0C8)
COAT, COAT_D, COAT_L = rgb(0xA08560), rgb(0x6E5A3E), rgb(0xD0C0A0)     # a dog's coat; a handler's coat too
PALOMINO, PALOMINO_D = rgb(0xCDA46C), rgb(0x9C7A4C)
DARK = (0x2A, 0x2A, 0x33)

# how big each stands next to its reference type (see the docstring)
REF = {'captain': 'lord', 'lancer': 'cavalier', 'outrider': 'cavalier', 'marksman': 'archer', 'brute': 'fighter',
       'houndmaster': 'soldier', 'hound': 'lord'}
SIZE = {'captain': 1.08, 'lancer': 1.1, 'outrider': 0.96, 'marksman': 1.02, 'brute': 1.25, 'houndmaster': 1.0, 'hound': 0.9}


def _blade(M, joint, hand, alpha, length, width, guard):
    """A sword like mu.sword() but with the blade's length and width chosen (a captain's long blade)."""
    h = hand
    r = (alpha, 0, 0)
    M.box(joint, add(h, (0, 0.02, 0)), (0.05, 0.13, 0.05), LEATHER_D, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.09, 0)), (guard, 0.04, 0.07), GOLD, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.11 + length / 2, 0)), (width, length, 0.03), STEEL_L, rot=r, pivot=h)
    M.box(joint, add(h, (0, 0.11 + length + 0.07, 0)), (width, 0.14, 0.03), STEEL_L, top=(0.05, 1), rot=r, pivot=h)


def captain():
    M = Model('captain')
    hand = foot_base(M, STEEL, trim=STEEL_L, sleeve=STEEL, glove=GOLD, hair=BLOND, belt=GOLD, tw=0.58, td=0.32,
                     helmet=True)
    M.joint('weapon', 'arm_r', hand)
    M.box('spine', (0, 0.96, 0.175), (0.32, 0.46, 0.03), SIDEW, SIDE)                  # tabard with a gold crest
    M.box('spine', (0, 1.0, 0.195), (0.07, 0.2, 0.012), GOLD)
    M.box('spine', (0, 1.04, 0.195), (0.2, 0.05, 0.012), GOLD)
    M.box('spine', (0, 0.7, -0.2), (0.52, 0.98, 0.05), SIDEW, SIDE, top=(1.2, 1))      # long cape, nearly to the boots
    M.box('spine', (0, 0.22 + 0.03, -0.2), (0.62, 0.05, 0.055), GOLD)                   # gold hem
    M.box('spine', (0, 1.2, -0.02), (0.42, 0.06, 0.38), GOLD)                           # collar
    for s, aj in ((1, 'arm_l'), (-1, 'arm_r')):
        M.box(aj, (s * 0.36, 1.18, 0), (0.24, 0.12, 0.26), GOLD)                       # pauldrons
        M.box(aj, (s * 0.36, 1.25, 0), (0.12, 0.04, 0.14), STEEL_L)
    M.box('head', (0, 1.38, 0), (0.35, 0.36, 0.35), STEEL)                              # closed helm, gold brow
    M.box('head', (0, 1.57, 0), (0.31, 0.04, 0.31), STEEL_L)
    M.box('head', (0, 1.47, 0.178), (0.33, 0.04, 0.02), GOLD)
    M.box('head', (0, 1.4, 0.178), (0.24, 0.04, 0.02), DARK)
    M.box('head', (0, 1.33, 0.178), (0.04, 0.12, 0.02), DARK)
    M.box('head', (0, 1.7, -0.03), (0.07, 0.22, 0.34), SIDEW, SIDE, top=(1, 0.6))        # tall plume
    M.box('head', (0, 1.6, 0), (0.05, 0.06, 0.34), GOLD)                                # crest bar under it
    _blade(M, 'weapon', hand, 35, 0.72, 0.09, 0.26)
    M.alpha = 35
    return M


def lancer():
    M = Model('lancer')
    hand = mount_base(M, HORSE, HORSE_D, LEATHER_D, HORSE_D, armour=STEEL)
    M.joint('weapon', 'arm_r', hand)
    pv = (0, 1.08, 0.38)
    M.box('root', (0, 1.0, 0.46), (0.4, 0.34, 0.07), STEEL_D)                             # breastplate
    M.box('root', (0, 1.04, 0.5), (0.2, 0.2, 0.03), GOLD)
    for s in (1, -1):
        M.box('root', (s * 0.27, 0.92, 0.0), (0.05, 0.3, 0.7), STEEL_D)                 # flank plates
        M.box('root', (s * 0.29, 0.7, 0.0), (0.04, 0.34, 0.84), SIDEW, SIDE, top=(1, 1.04))  # cloth hanging down each side
        M.box('root', (s * 0.3, 0.55, 0.0), (0.045, 0.05, 0.84), GOLD)
    M.box('root', (0, 1.18, 0.0), (0.54, 0.05, 0.62), SIDEW, SIDE)
    M.box('neck', (0, 1.34, 0.46), (0.25, 0.4, 0.28), STEEL_D, rot=(28, 0, 0), pivot=pv)    # neck armour
    M.box('neck', (0, 1.52, 0.8), (0.2, 0.2, 0.27), STEEL, rot=(20, 0, 0), pivot=pv)       # face plate
    M.box('neck', (0, 1.66, 0.76), (0.04, 0.12, 0.05), STEEL_L, rot=(20, 0, 0), pivot=pv)  # spike
    M.box('spine', (0, 1.42, 0.17), (0.36, 0.38, 0.03), SIDEW, SIDE)                    # surcoat
    M.box('head', (0, 1.8, 0.152), (0.29, 0.26, 0.04), STEEL)                           # visor closes the face
    M.box('head', (0, 1.83, 0.176), (0.22, 0.035, 0.01), DARK)
    M.box('head', (0, 2.06, -0.04), (0.07, 0.17, 0.3), SIDEW, SIDE, top=(1, 0.6))        # plume
    h = hand
    length = 1.2
    spear(M, 'weapon', hand, alpha=60, length=length, butt=0.55, tip=STEEL_L)
    M.box('weapon', add(h, (0, length - 0.22, -0.13)), (0.02, 0.2, 0.26), SIDEW, SIDE, top=(1, 0.5), rot=(60, 0, 0), pivot=h)  # pennant
    M.alpha = 60
    return M


def outrider():
    M = Model('outrider')
    hand = mount_base(M, PALOMINO, PALOMINO_D, LEATHER_D, rgb(0x6B4A2E), armour=OLIVE)
    M.joint('weapon', 'arm_r', hand)
    M.box('spine', (0, 1.42, 0.145), (0.34, 0.38, 0.03), OLIVE_D)                       # leather harness
    M.box('spine', (0, 1.55, -0.17), (0.34, 0.5, 0.04), SIDEW, SIDE, top=(1.0, 1), rot=(-12, 0, 0))   # cloak streaming back
    M.box('spine', (0, 1.48, -0.4), (0.2, 0.08, 0.34), SIDEW, SIDE, top=(1, 0.8), rot=(8, 0, 0))     # scarf tails
    M.box('head', (0, 1.55, -0.16), (0.3, 0.3, 0.08), OLIVE_D)                         # hood
    M.box('root', (0, 1.17, 0.0), (0.46, 0.04, 0.5), LEATHER_D)                          # plain saddle
    sword(M, 'weapon', hand, alpha=40)
    M.alpha = 40
    return M


def marksman():
    M = Model('marksman')
    hand = foot_base(M, OLIVE, trim=None, legc=OLIVE_D, boot=LEATHER_D, sleeve=OLIVE, glove=LEATHER, belt=LEATHER,
                     hair=OLIVE_D, helmet=True)
    M.joint('bow', 'arm_l', (0.41, 0.76, 0.06))
    M.box('head', (0, 1.55, 0.0), (0.36, 0.14, 0.36), SIDEW, SIDE)                      # hood
    M.box('head', (0, 1.38, -0.17), (0.36, 0.3, 0.08), SIDEW, SIDE)
    for s in (1, -1):
        M.box('head', (s * 0.175, 1.42, 0.0), (0.04, 0.22, 0.34), SIDEG, SIDE)
    M.box('head', (0.1, 1.7, -0.02), (0.04, 0.18, 0.1), WHITE, rot=(0, 0, -25))         # cap feather
    M.box('spine', (0, 1.14, 0), (0.66, 0.1, 0.4), SIDEW, SIDE, top=(0.8, 0.85))        # mantle
    M.box('spine', (0, 1.0, 0.0), (0.06, 0.62, 0.42), LEATHER_D, rot=(0, 0, 38))        # bandolier across the chest
    M.box('spine', (0.1, 0.96, -0.2), (0.17, 0.54, 0.12), LEATHER_D, rot=(0, 0, -12))   # big quiver
    for k in (-0.04, 0.0, 0.04):
        M.box('spine', (0.16 + k * 0.4, 1.3, -0.2), (0.025, 0.14, 0.025), WHITE, rot=(0, 0, -12))   # fletching
    # a tall bow, a drawn arrow
    h = (0.41, 0.76, 0.06)
    M.box('bow', add(h, (0, 0.0, 0.03)), (0.05, 0.24, 0.06), WOOD)
    M.box('bow', add(h, (0, 0.28, -0.03)), (0.07, 0.36, 0.07), WOOD, rot=(-22, 0, 0))
    M.box('bow', add(h, (0, -0.28, -0.03)), (0.07, 0.36, 0.07), WOOD, rot=(22, 0, 0))
    M.box('bow', add(h, (0, 0.52, -0.12)), (0.06, 0.12, 0.06), WOOD, rot=(-50, 0, 0))
    M.box('bow', add(h, (0, -0.52, -0.12)), (0.06, 0.12, 0.06), WOOD, rot=(50, 0, 0))
    M.box('bow', add(h, (0, 0.0, -0.17)), (0.02, 1.04, 0.02), WHITE)                    # string
    M.box('bow', add(h, (0, 0.05, 0.27)), (0.02, 0.02, 0.8), LEATHER_D)                 # arrow
    M.box('bow', add(h, (0, 0.05, 0.71)), (0.05, 0.05, 0.1), STEEL_L, top=(0.05, 1), rot=(90, 0, 0))
    M.box('bow', add(h, (0, 0.05, -0.1)), (0.05, 0.05, 0.08), WHITE)
    M.alpha = 0
    return M


def brute():
    M = Model('brute')
    hand = foot_base(M, LEATHER, trim=None, legc=LEATHER, boot=LEATHER_D, sleeve=SKIN, glove=SKIN, belt=LEATHER_D,
                     hair=rgb(0x4A2A18), tw=0.74, td=0.42, arm_w=0.22, leg_w=0.26, helmet=True)
    M.joint('weapon', 'arm_r', hand)
    M.box('head', (0, 1.52, 0), (0.34, 0.09, 0.34), STEEL_D)                            # iron cap with horns
    M.box('head', (0, 1.42, -0.17), (0.32, 0.22, 0.06), rgb(0x4A2A18))
    M.box('head', (0, 1.37, 0.0), (0.33, 0.1, 0.32), rgb(0x4A2A18), top=(1, 0.2))       # beard
    for s in (1, -1):
        M.box('head', (s * 0.2, 1.6, 0), (0.07, 0.2, 0.07), BONE, rot=(0, 0, -s * 35))
        M.box('head', (s * 0.3, 1.74, 0), (0.06, 0.16, 0.06), BONE, rot=(0, 0, -s * 70))
    M.box('spine', (0, 1.17, 0), (0.94, 0.15, 0.56), FUR, top=(0.8, 0.9))                # fur mantle
    M.box('spine', (0, 1.1, 0.02), (0.94, 0.06, 0.58), FUR_D)
    M.box('spine', (0, 0.72, 0.0), (0.76, 0.28, 0.46), SIDEW, SIDE)                      # side-coloured hide kilt
    M.box('spine', (0, 0.6, 0.23), (0.34, 0.34, 0.03), SIDEG, SIDE)
    M.box('spine', (0, 0.95, 0.215), (0.4, 0.14, 0.015), SKIN_D)                        # chest shadow
    # great axe: long haft, two big blades along Z
    h = hand
    r = (15, 0, 0)
    M.box('weapon', add(h, (0, 0.4, 0)), (0.06, 1.3, 0.06), WOOD, rot=r, pivot=h)
    for s in (1, -1):
        M.box('weapon', add(h, (0, 0.88, s * 0.19)), (0.05, 0.4, 0.28), STEEL, top=(1, 0.55) if s > 0 else (1, 0.55),
              rot=r, pivot=h)
        M.box('weapon', add(h, (0, 0.88, s * 0.34)), (0.04, 0.46, 0.05), STEEL_L, rot=r, pivot=h)
    M.box('weapon', add(h, (0, 0.88, 0)), (0.09, 0.28, 0.12), STEEL_D, rot=r, pivot=h)
    M.box('weapon', add(h, (0, 1.12, 0)), (0.05, 0.14, 0.05), STEEL_L, top=(0.1, 0.1), rot=r, pivot=h)
    M.alpha = 15
    return M


def houndmaster():
    M = Model('houndmaster')
    hand = foot_base(M, COAT, trim=None, legc=COAT_D, boot=LEATHER_D, sleeve=COAT, glove=LEATHER, belt=LEATHER,
                     hair=LEATHER_D, helmet=True)
    M.joint('weapon', 'arm_r', hand)
    M.box('spine', (0, 0.62, 0.0), (0.58, 0.36, 0.34), COAT)                            # long coat skirts
    M.box('spine', (0, 0.5, 0.0), (0.6, 0.1, 0.36), COAT_D)
    M.box('spine', (0, 0.7, 0.171), (0.08, 0.5, 0.012), LEATHER_D)                      # coat opening
    M.box('spine', (0, 0.95, 0.0), (0.5, 0.06, 0.3), SIDEG, SIDE)                       # sash
    M.box('spine', (0, 1.2, 0.0), (0.56, 0.16, 0.44), FUR, top=(0.82, 0.82))            # big fur collar
    M.box('spine', (0, 1.14, 0.0), (0.58, 0.05, 0.46), FUR_D)
    M.box('head', (0, 1.56, 0), (0.33, 0.1, 0.33), SIDEW, SIDE)                         # cap
    M.box('head', (0, 1.45, -0.04), (0.33, 0.1, 0.27), SIDEW, SIDE)
    M.box('head', (0, 1.4, -0.15), (0.32, 0.22, 0.06), LEATHER_D)
    M.box('head', (0, 1.31, 0.17), (0.06, 0.04, 0.06), GOLD)                            # whistle at the lips
    M.box('spine', (0, 1.16, 0.19), (0.012, 0.16, 0.012), GOLD)                         # its chain
    sword(M, 'weapon', hand, alpha=30)
    # leash from the left fist: hangs down, then trails along the ground in a loop
    p = (0.41, 0.72, 0.03)
    M.box('arm_l', add(p, (0, -0.16, 0.0)), (0.03, 0.3, 0.03), LEATHER)
    M.box('arm_l', add(p, (0, -0.3, 0.08)), (0.03, 0.03, 0.2), LEATHER)
    M.box('arm_l', add(p, (0.06, -0.3, 0.2)), (0.14, 0.03, 0.03), LEATHER)
    M.box('arm_l', add(p, (0.12, -0.3, 0.12)), (0.03, 0.03, 0.18), LEATHER)
    M.box('spine', (-0.28, 0.75, 0.1), (0.1, 0.1, 0.1), LEATHER_D)                       # spare leash coiled at the hip
    M.alpha = 30
    return M


def hound():
    """A dog on four legs, long and low, facing +Z. Joints: root, spine, head, tail, leg_fl / fr / bl / br."""
    M = Model('hound')
    M.joint('root', None, (0, 0.5, 0))
    M.joint('spine', 'root', (0, 0.5, 0.0))
    M.joint('head', 'spine', (0, 0.58, 0.3))
    M.joint('tail', 'root', (0, 0.58, -0.32))
    legs = (('leg_fl', 0.09, 0.22), ('leg_fr', -0.09, 0.22), ('leg_bl', 0.09, -0.24), ('leg_br', -0.09, -0.24))
    for n, x, z in legs:
        M.joint(n, 'root', (x, 0.36, z))
    for n, x, z in legs:
        front = z > 0
        M.box(n, (x, 0.2, z), (0.1, 0.34, 0.12), COAT if x > 0 else COAT_D)
        M.box(n, (x, 0.04, z + 0.02), (0.11, 0.08, 0.17), COAT_L)                       # paws
    M.box('spine', (0, 0.5, 0.0), (0.26, 0.24, 0.5), COAT)                              # body
    M.box('spine', (0, 0.52, 0.2), (0.3, 0.3, 0.22), COAT)                              # deep chest
    M.box('spine', (0, 0.52, -0.2), (0.28, 0.26, 0.22), COAT)                           # haunches
    M.box('spine', (0, 0.38, 0.0), (0.2, 0.06, 0.5), COAT_L)                            # pale belly
    M.box('spine', (0, 0.64, -0.02), (0.1, 0.05, 0.5), COAT_D)                          # dark stripe along the back
    M.box('spine', (0, 0.58, 0.0), (0.3, 0.06, 0.26), SIDEW, SIDE)                      # side-coloured saddle cloth
    M.box('spine', (0, 0.6, 0.31), (0.24, 0.12, 0.1), SIDEW, SIDE)                      # collar
    M.box('head', (0, 0.6, 0.38), (0.12, 0.2, 0.16), COAT, rot=(-20, 0, 0), pivot=(0, 0.58, 0.3))      # neck
    M.box('head', (0, 0.68, 0.46), (0.2, 0.2, 0.2), COAT)                               # skull
    M.box('head', (0, 0.63, 0.62), (0.11, 0.1, 0.2), COAT_L)                            # long snout
    M.box('head', (0, 0.58, 0.58), (0.09, 0.03, 0.14), COAT_D)                          # lower jaw
    M.box('head', (0, 0.66, 0.725), (0.07, 0.05, 0.03), DARK)                           # nose
    for s in (1, -1):
        M.box('head', (s * 0.105, 0.7, 0.52), (0.03, 0.05, 0.05), EYE)
        M.box('head', (s * 0.08, 0.83, 0.42), (0.06, 0.16, 0.06), COAT_D, top=(0.25, 0.4), rot=(0, 0, s * 8))   # pricked ears
    M.box('head', (0.0, 0.595, 0.69), (0.05, 0.03, 0.02), WHITE)                        # a fang
    M.box('tail', (0, 0.72, -0.4), (0.07, 0.3, 0.07), COAT_D, rot=(-35, 0, 0), pivot=(0, 0.58, -0.32), top=(0.6, 0.6))
    return M


def hound_clips(M):
    """Idle / Move / Attack / Die for the four-legged skeleton (negative x swings a hanging leg forward)."""
    legs = ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br')
    c = Clip(M, 1.2, True)                              # idle: breathes, the tail wags, the head turns a little
    for t, b, w in ((0.0, 0, 12), (0.3, 0.5, -12), (0.6, 1, 12), (0.9, 0.5, -12), (1.2, 0, 12)):
        c.pos('spine', t, dy=0.012 * b)
        c.rot('head', t, x=-3 * b, y=6 * (b - 0.5))
        c.rot('tail', t, y=w, x=-5 * b)
    M.clips['Idle'] = c

    c = Clip(M, 0.6, True)                              # move: a bounding trot, diagonal pairs
    for t, p, d in ((0.0, 30, -0.04), (0.15, 0, 0.02), (0.3, -30, -0.04), (0.45, 0, 0.02), (0.6, 30, -0.04)):
        c.pos('root', t, dy=d)
        c.rot('leg_fl', t, x=p)
        c.rot('leg_br', t, x=p)
        c.rot('leg_fr', t, x=-p)
        c.rot('leg_bl', t, x=-p)
        c.rot('spine', t, x=-p * 0.08)
        c.rot('head', t, x=p * 0.1 - 4)
        c.rot('tail', t, y=p * 0.5)
    M.clips['Move'] = c

    c = Clip(M, 0.5, False)                             # attack: crouch, spring forward, bite, settle back
    keys = ((0.0, 0.0, 0, 0, 0), (0.12, -0.08, 8, -10, 0), (0.24, 0.28, -10, 38, -30), (0.34, 0.28, -10, 38, -30),
            (0.5, 0.0, 0, 0, 0))                          # (t, root dz, spine x, head x, legs x)
    for t, dz, sp, hd, lg in keys:
        c.pos('root', t, dz=dz)
        c.rot('spine', t, x=sp)
        c.rot('head', t, x=hd)
        c.rot('leg_fl', t, x=lg)
        c.rot('leg_fr', t, x=lg)
        c.rot('leg_bl', t, x=-lg * 0.6)
        c.rot('leg_br', t, x=-lg * 0.6)
        c.rot('tail', t, x=-15 if 0.2 < t < 0.4 else 0)
    M.clips['Attack'] = c

    c = Clip(M, 0.7, False)                             # die: rolls onto its side, legs stiffen out
    times = (0.0, 0.1, 0.2, 0.3, 0.42, 0.55, 0.7)
    angles = (0, 12, 32, 58, 84, 93, 90)
    for t, a in zip(times, angles):
        f = min(1.0, a / 90)
        c.rot('root', t, z=a)
        c.rot('head', t, x=-20 * f, z=10 * f)
        c.rot('tail', t, y=25 * f)
        for i, leg in enumerate(legs):
            c.rot(leg, t, x=(-35 if i % 2 else 35) * f)
    M.clips['Die'] = c
    settle(M, c, times)


BUILDERS = {
    'captain': (captain, lambda M: foot_clips(M, style='sword', alpha=35)),
    'lancer': (lancer, lambda M: mount_clips(M, 60)),
    'outrider': (outrider, lambda M: mount_clips(M, 40)),
    'marksman': (marksman, lambda M: foot_clips(M, 'bow', style='bow', alpha=0)),
    'brute': (brute, lambda M: foot_clips(M, style='axe', alpha=15)),
    'houndmaster': (houndmaster, lambda M: foot_clips(M, style='sword', alpha=30)),
    'hound': (hound, hound_clips),
}
# Unit types on four legs (no arms, no weapon joint): make_unit_models --check expects another set of joints of them.
QUADRUPEDS = {'hound'}


def model_height(uid):
    """The "height" (squares) graphics.json gives the model so that it stands SIZE[uid] times its reference type's size."""
    top = lambda u: max(p[1] for _, vs, _, _ in mu.make_unit(u).tris for p, _ in vs)
    return round(SIZE[uid] * top(uid) / top(REF[uid]), 3)
