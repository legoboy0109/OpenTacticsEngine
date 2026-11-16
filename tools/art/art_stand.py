"""Standing terrain art for the 3D kinds of view (docs/GRAPHICS.md "Standing terrain").

In the two 3D kinds a terrain can have something standing on its ground: an upright picture that faces
the camera. These are those pictures for the starter games (forest: clumps of trees; mountain: peaks),
24 x 32 pixels each (one square wide, a third taller), transparent around the shapes, with the starter
set's dark outline and the same greens and greys as the top-down tiles. The ground picture underneath
is plain ground: a leafy floor for the forest, rock for the mountain (24 x 24).

Everything is drawn from shapes with fixed seeds; no input images."""
import art_tiles as tl
from pix import Pix, rng

SW, SH = 24, 32     # a standing picture's cell
T = 24              # a ground cell
FOOT = SH - 2       # row of the feet: the picture's lowest rows stay clear for the outline

LEAF = (0x2F6B34, 0x45883F, 0x6AAE58, 0x245A2B)   # canopy: base, light, highlight, shade (as tl.tree)
PINE = (0x275A33, 0x3A7A44, 0x1E4A2A)             # pine: base, light, shade
TRUNK = (0x6B4526, 0x56391F)


def tree(p, cx, foot, r=5):
    """A round tree, trunk standing on row `foot`, canopy radius r."""
    top = foot - 3 - 2 * r + 1
    cy = top + r
    p.rect(cx - 1, foot - 3, cx, foot, TRUNK[0])
    p.rect(cx, foot - 3, cx, foot, TRUNK[1])
    p.circle(cx, cy, r, LEAF[0])
    p.circle(cx - r * 0.3, cy - r * 0.3, r * 0.6, LEAF[1])
    p.set(int(cx - r * 0.6), int(cy - r * 0.4), LEAF[2])
    p.set(int(cx - r * 0.4), int(cy - r * 0.6), LEAF[2])
    for dx, dy in ((0.5, 0.6), (0.8, 0.2), (0.3, 0.9), (0.7, 0.45)):
        p.set(int(cx + dx * r), int(cy + dy * r), LEAF[3])


def pine(p, cx, foot, h=17):
    """A pine: a trunk and three drooping layers; h rows tall."""
    p.rect(cx, foot - 2, cx, foot, TRUNK[1])
    p.rect(cx - 1, foot - 2, cx - 1, foot, TRUNK[0])
    layers = 3
    layer_h = (h - 2) // layers + 1
    for i in range(layers):
        bottom = foot - 2 - i * (layer_h - 2)
        top = bottom - layer_h
        for y in range(top, bottom + 1):
            half = 1 + (y - top) * (3 + i * 1) // layer_h
            for x in range(cx - half, cx + half + 1):
                c = PINE[1] if x < cx else PINE[0]
                if x == cx + half or (y == bottom and x > cx):
                    c = PINE[2]
                p.set(x, y, c)


def finish(p):
    p.outline(tl.DARKLINE)
    return p


def forest(variant):
    p = Pix(SW, SH)
    if variant == 0:      # three round trees, one behind two
        tree(p, 12, FOOT - 9, 5)
        tree(p, 6, FOOT, 5)
        tree(p, 18, FOOT - 1, 5)
    elif variant == 1:    # two pines and a round tree
        pine(p, 7, FOOT - 1, 20)
        pine(p, 17, FOOT - 5, 18)
        tree(p, 12, FOOT, 5)
    else:                 # a thicker clump of smaller trees
        tree(p, 6, FOOT - 7, 4)
        tree(p, 18, FOOT - 8, 4)
        tree(p, 12, FOOT - 3, 5)
        tree(p, 5, FOOT, 4)
        tree(p, 19, FOOT - 1, 4)
    return finish(p)


def peak(p, ax, ay, base_y, hw, snow, light, dark, seed):
    """A peak: apex (ax, ay), standing on row base_y with half width hw there; lit left, shaded right."""
    r = rng('peak', seed)
    for y in range(ay, base_y + 1):
        t = (y - ay) / float(max(1, base_y - ay))
        half = int(0.5 + t * hw + (r.random() - 0.5) * 0.9 * t)
        for x in range(ax - half, ax + half + 1):
            c = light if x <= ax else dark
            if y - ay < snow:
                c = 0xF4F6F8 if x <= ax else 0xC6D0DC
                if y - ay == snow - 1 and (x + y) % 2 == 0:
                    c = light if x <= ax else dark
            p.set(x, y, c)
    # ridge lines down the flanks
    p.line(ax, ay + snow, ax - hw // 3, base_y - 2, 0x8E8878 if light == 0xA89C8A else 0x877B69)
    p.line(ax + 1, ay + snow + 1, ax + hw // 2, base_y - 1, 0x4F4535)


def mountain(variant):
    p = Pix(SW, SH)
    if variant == 0:      # a big peak with a small one beside it
        peak(p, 13, 2, FOOT, 10, 7, 0xA89C8A, 0x6F6351, 1)
        peak(p, 5, 15, FOOT, 5, 3, 0xA39785, 0x7A6E5C, 2)
    else:                 # two peaks of about the same height
        peak(p, 7, 5, FOOT, 7, 5, 0xA39785, 0x7A6E5C, 3)
        peak(p, 16, 8, FOOT, 7, 4, 0xA89C8A, 0x6F6351, 4)
    return finish(p)


def forest_floor(seed=7):
    """Ground under a forest: a darker, leafy grass."""
    p = Pix(T, T, 0x6C9E4D)
    r = rng('floor', seed)
    for _ in range(14):
        x, y = r.randrange(T), r.randrange(T)
        p.set(x, y, 0x5C8B40)
        p.set(x + 1, y, 0x5C8B40)
    for _ in range(9):
        p.set(r.randrange(T), r.randrange(T), 0x86B45E)
    for _ in range(5):  # fallen leaves
        p.set(r.randrange(T), r.randrange(T), 0xA9904A)
    return p


def rock_floor(seed=9):
    """Ground under a mountain: bare stony earth."""
    p = Pix(T, T, 0x9A9484)
    r = rng('rock', seed)
    for _ in range(18):
        x, y = r.randrange(T), r.randrange(T)
        p.set(x, y, 0x7F796A)
        p.set(x + 1, y, 0x7F796A)
    for _ in range(10):
        p.set(r.randrange(T), r.randrange(T), 0xB3AD9C)
    for _ in range(3):  # a few pebbles
        x, y = r.randrange(2, T - 2), r.randrange(2, T - 2)
        p.rect(x, y, x + 1, y + 1, 0x6B6658)
    return p


# Sheet "stand": one row of cells, forest variants then mountain variants.
STAND_ORDER = [('forest', 0), ('forest', 1), ('forest', 2), ('mountain', 0), ('mountain', 1)]
GROUND_ORDER = ['forest', 'mountain']


def stand_sheet():
    sheet = Pix(SW * len(STAND_ORDER), SH)
    for i, (kind, v) in enumerate(STAND_ORDER):
        sheet.paste(forest(v) if kind == 'forest' else mountain(v), i * SW, 0)
    return sheet


def ground_sheet():
    sheet = Pix(T * len(GROUND_ORDER), T)
    sheet.paste(forest_floor(), 0, 0)
    sheet.paste(rock_floor(), T, 0)
    return sheet


def terrain_looks(terrain_ids):
    """Terrain id -> look with a standing part, for the terrain the starter art stands up."""
    def cells(kind):
        return [[i, 0] for i, (k, _) in enumerate(STAND_ORDER) if k == kind]
    out = {}
    for tid in ('forest', 'mountain'):
        if tid in terrain_ids:
            out[tid] = {'sheet': 'ground3d', 'cell': [GROUND_ORDER.index(tid), 0],
                        'stand': {'sheet': 'stand', 'variants': cells(tid)}}
    return out
