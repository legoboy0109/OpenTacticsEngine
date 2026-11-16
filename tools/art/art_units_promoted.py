"""Sprites of the seven promoted / summoned unit types of the story demo (24 x 24 cells, two idle frames).

Each is drawn on the same grid and palette as its source type in art_units.py, grander or lighter in the way
its name says, so a player reads the family at a glance: captain (lord), lancer / outrider (cavalier),
marksman (archer), brute (fighter), houndmaster (soldier) and hound (a four-legged dog, no rider and no person).
Not imported on its own: art_units.py pulls PROMOTED into UNITS at its end (the palette lives there).
"""
from pix import Pix
import art_units as au
from art_units import (T, FEET, OUT, SKIN, SKIN_D, STEEL, STEEL_D, STEEL_L, LEATHER, LEATHER_D, CLOTH,
                       GREY, GOLD, WOOD, WHITE, OLIVE, OLIVE_D)

EYE_C = 0x30303A
CRIMSON, CRIMSON_D = 0xB8403A, 0x8A2E2E
FUR, FUR_D, FUR_L = 0xD9D2C0, 0xA89F8A, 0xF0EBDD
BONE = 0xE8E0C8
COAT, COAT_D, COAT_L = 0xA08560, 0x6E5A3E, 0xD0C0A0   # a dog's coat (and a handler's coat): tan, shadow, light


def _px(p, x, y, c):
    """set() that ignores the rows above the cell (a plume on the idle's raised frame)."""
    if y >= 0:
        p.set(x, y, c)


def captain(b):
    """A lord grown grander: tall crimson plume on a steel helm, long gold-edged cape, gold shoulders and a long blade."""
    p = Pix(T, T)
    p.rect(5, 8 - b, 9, 16, CRIMSON_D)  # long cape behind, down to the boots
    p.rect(5, 8 - b, 5, 16, GOLD)
    p.rect(9, 9 - b, 9, 16, CRIMSON)
    au.legs(p, 12, 13)
    p.rect(8, 8 - b, 15, 14, STEEL)
    p.rect(8, 8 - b, 15, 8 - b, STEEL_L)
    p.rect(14, 9 - b, 15, 14, STEEL_D)
    p.rect(11, 10 - b, 12, 11, GOLD)  # chest crest
    p.rect(8, 12, 15, 12, GOLD)
    p.rect(7, 7 - b, 9, 8 - b, GOLD)  # pauldrons
    p.rect(14, 7 - b, 16, 8 - b, GOLD)
    au.head(p, 12, 3, b, hair=STEEL)
    p.rect(10, 2 - b, 13, 3 - b, STEEL)  # helm with a gold brow
    p.rect(10, 3 - b, 13, 3 - b, GOLD)
    for x, y in ((11, 1), (12, 1), (12, 0), (13, 0), (13, 1), (14, 2)):  # plume sweeping back-right
        _px(p, x, y - b, CRIMSON)
    p.rect(17, 10, 17, 11, GOLD)  # hilt, wide guard, long blade
    p.rect(16, 10, 18, 10, GOLD)
    p.line(17, 9, 20, 1 - b, STEEL_L)
    p.line(18, 9, 21, 1 - b, STEEL)
    p.outline(OUT)
    return p


def lancer(b):
    """A cavalier in heavy plate on a barded horse, couching a long pennant lance."""
    p = Pix(T, T)
    au.horse(p, 3, 9, b)
    p.rect(5, 10, 14, 13, STEEL_D)  # barding over the flank
    p.rect(5, 10, 14, 10, STEEL)
    p.rect(8, 11, 11, 12, CLOTH)  # caparison cloth with a gold bar
    p.rect(8, 12, 11, 12, GOLD)
    p.rect(16, 6, 19, 8, STEEL)  # chamfron
    p.rect(17, 5, 17, 5, STEEL_L)
    p.rect(14, 6, 15, 9, STEEL_D)  # neck armour
    p.rect(7, 5 - b, 12, 9, STEEL)  # broad rider
    p.rect(7, 5 - b, 12, 5 - b, STEEL_L)
    p.rect(8, 1 - b, 11, 4 - b, STEEL)  # closed helm
    p.rect(8, 1 - b, 11, 1 - b, STEEL_L)
    p.rect(9, 3 - b, 11, 3 - b, 0x2A2A33)  # visor slit
    _px(p, 10, -b, GREY)  # short crest
    p.line(11, 9, 23, 3 + b, WOOD)  # long lance
    p.line(11, 10, 23, 4 + b, LEATHER_D)
    p.rect(22, 3 + b, 23, 4 + b, STEEL_L)
    p.rect(19, 4 + b, 20, 6 + b, CRIMSON)  # pennant
    p.outline(OUT)
    return p


def outrider(b):
    """A light, quick rider: hooded, scarf streaming, sword out, speed streaks behind the horse."""
    p = Pix(T, T)
    for i, (x, y) in enumerate(((0, 9), (1, 12), (0, 14))):  # speed lines behind the tail
        p.rect(x, y + b, x + 1 + (i == 1), y + b, GREY)
    au.horse(p, 3, 9, b)
    p.rect(8, 6 - b, 11, 9, OLIVE)  # light jerkin
    p.rect(8, 6 - b, 8, 9, OLIVE_D)
    p.rect(9, 2 - b, 11, 5 - b, SKIN)
    p.rect(8, 1 - b, 11, 2 - b, OLIVE_D)  # hood
    p.rect(8, 3 - b, 8, 5 - b, OLIVE_D)
    p.set(11, 4 - b, EYE_C)
    p.line(7, 5 - b, 3, 4 + b, CRIMSON)  # scarf blowing back
    p.line(7, 6 - b, 3, 6, CRIMSON_D)
    p.line(12, 8, 19, 3 + b, STEEL_L)  # slim sword held forward and up
    p.line(12, 9, 19, 4 + b, STEEL)
    p.rect(12, 8, 13, 9, GOLD)
    p.outline(OUT)
    return p


def marksman(b):
    """An archer with a tall bow, an arrow drawn and a full quiver; a feathered cap under the hood."""
    p = Pix(T, T)
    au.legs(p, 12, 13, c=OLIVE_D)
    p.rect(8, 8 - b, 15, 14, OLIVE)
    p.rect(14, 9 - b, 15, 14, OLIVE_D)
    p.rect(8, 12, 15, 12, LEATHER)
    p.line(8, 8 - b, 15, 13, LEATHER_D)  # bandolier
    au.head(p, 12, 3, b, hair=OLIVE_D)
    p.rect(9, 2 - b, 14, 3 - b, OLIVE_D)
    p.rect(9, 4 - b, 10, 6 - b, OLIVE_D)
    _px(p, 13, 0 - b, WHITE)  # cap feather
    _px(p, 14, 1 - b, WHITE)
    p.rect(7, 5 - b, 8, 12, LEATHER_D)  # quiver on the back, arrow tails showing
    p.rect(7, 3 - b, 7, 4 - b, WHITE)
    p.rect(8, 4 - b, 8, 4 - b, CRIMSON)
    for y, x in ((1, 20), (2, 21), (3, 22), (4, 22), (5, 22), (6, 22), (7, 22), (8, 22), (9, 22), (10, 22), (11, 22),
                 (12, 22), (13, 22), (14, 21), (15, 20)):  # tall bow
        p.set(x, y, WOOD)
    p.line(20, 1, 19, 8, WHITE)  # string drawn back
    p.line(19, 8, 20, 15, WHITE)
    p.line(11, 8, 22, 8, STEEL_L)  # arrow
    p.outline(OUT)
    return p


def brute(b):
    """A hulking fighter: horned, bare-chested under a fur mantle, a great two-handed axe."""
    p = Pix(T, T)
    au.legs(p, 11, 13, c=LEATHER, boot=LEATHER_D, spread=3)
    p.rect(5, 7 - b, 16, 12, SKIN)  # broad bare torso and arms
    p.rect(6, 11 - b, 15, 13, LEATHER)  # kilt of hide
    p.rect(6, 11, 15, 11, LEATHER_D)
    p.rect(5, 6 - b, 16, 8 - b, FUR_D)  # fur mantle over the shoulders
    p.rect(5, 6 - b, 16, 6 - b, FUR)
    p.rect(8, 9 - b, 13, 10 - b, SKIN_D)  # chest shadow
    au.head(p, 11, 2, b, hair=0x4A2A18, w=4)
    p.rect(9, 1 - b, 12, 1 - b, STEEL_D)  # iron cap
    _px(p, 8, 0 - b, BONE)  # horns
    _px(p, 13, 0 - b, BONE)
    p.set(8, 1 - b, BONE)
    p.set(13, 1 - b, BONE)
    p.set(7, 1 - b, BONE)
    p.set(14, 1 - b, BONE)
    p.line(19, 2 - b, 19, 16, WOOD)  # great axe haft
    p.rect(17, 1 - b, 21, 6 - b, STEEL)  # big double-bitted head
    p.rect(16, 2 - b, 16, 5 - b, STEEL_L)
    p.rect(22, 2 - b, 22, 5 - b, STEEL_L)
    p.rect(18, 2 - b, 20, 5 - b, STEEL_D)
    p.rect(17, 11, 18, 11, SKIN)  # fist on the haft
    p.rect(18, 10, 19, 11, SKIN)
    p.outline(OUT)
    return p


def houndmaster(b):
    """A soldier turned beast-handler: fur collar, long coat, a whistle, a sword and a leash trailing from the fist."""
    p = Pix(T, T)
    au.legs(p, 12, 13, c=COAT_D)
    p.rect(7, 8 - b, 16, 14, COAT)  # long leather coat
    p.rect(14, 9 - b, 16, 14, COAT_D)
    p.rect(7, 14, 16, 14, LEATHER_D)
    p.rect(8, 12, 15, 12, LEATHER)
    p.rect(11, 9 - b, 12, 13, LEATHER_D)  # coat opening
    p.rect(7, 7 - b, 16, 8 - b, FUR)  # big fur collar
    p.rect(7, 8 - b, 16, 8 - b, FUR_D)
    p.set(7, 7 - b, FUR_L)
    p.set(16, 7 - b, FUR_L)
    au.head(p, 12, 2, b, hair=LEATHER_D)
    p.rect(10, 1 - b, 13, 2 - b, CRIMSON_D)  # cap
    p.rect(9, 2 - b, 9, 3 - b, CRIMSON_D)
    p.set(13, 5 - b, GOLD)  # whistle at the lips
    p.line(5, 10, 5, 3 - b, STEEL_L)  # sword held up on the left
    p.rect(4, 10, 6, 10, GOLD)
    p.line(17, 11, 19, 14, LEATHER)  # leash from the right fist down to a loop on the ground
    p.line(19, 14, 20, 16, LEATHER)
    p.rect(19, 16, 22, 17, LEATHER)
    p.rect(20, 16, 21, 17, LEATHER_D)
    p.outline(OUT)
    return p


def hound(b):
    """A war dog on four legs, facing right: pointed ear, long snout, red collar, tail up. No rider."""
    p = Pix(T, T)
    for lx, dark in ((4, True), (6, False), (13, True), (15, False)):  # legs: the far pair a shade darker; frame 1 steps
        sw = 1 if (b and lx in (6, 15)) else 0
        p.rect(lx, 12, lx + 1, FEET - 1, COAT_D if dark else COAT)
        p.rect(lx + sw, FEET - 1, lx + 1 + sw, FEET, LEATHER_D)
    p.ellipse(10, 11, 7.5, 3.2, COAT)  # body: long and low
    p.ellipse(9, 12.5, 6, 1.4, COAT_L)  # belly
    p.rect(5, 8, 13, 8, COAT_D)  # dark saddle along the back
    p.rect(14, 7 - b, 16, 11 - b, COAT)  # neck, chest forward
    p.rect(17, 6 - b, 19, 9 - b, COAT)  # head
    p.rect(20, 8 - b, 22, 9 - b, COAT_L)  # snout
    p.set(22, 8 - b, EYE_C)  # nose
    p.rect(20, 10 - b, 21, 10 - b, WHITE)  # a fang
    p.rect(17, 4 - b, 18, 5 - b, COAT_D)  # ear, pricked
    p.set(18, 3 - b, COAT_D)
    p.set(19, 7 - b, EYE_C)  # eye
    p.rect(14, 10 - b, 16, 10 - b, CRIMSON)  # collar
    for i, (x, y) in enumerate(((3, 8), (2, 7), (2, 6), (1, 5))):  # tail swept up, wagging on frame 1
        p.set(x - (b if i > 1 else 0), y, COAT_D)
    p.outline(OUT)
    return p


PROMOTED = {'captain': captain, 'lancer': lancer, 'outrider': outrider, 'marksman': marksman, 'brute': brute,
            'houndmaster': houndmaster, 'hound': hound}
