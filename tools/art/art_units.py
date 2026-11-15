"""Unit sprites (24 x 24 cells, two idle frames) of the starter art set.

Every sprite stands on row 17: the bottom of the cell is left to the engine's
side-colour base and the HP bar. Colours are mostly neutral (metal, leather,
cloth in greys and browns) so the side marker is what tells the sides apart.
Frame 1 is the idle: the upper body sinks/rises one pixel (`b`), wheels and
tracks shift, wings flap.
"""
from pix import Pix

T = 24
FEET = 17
OUT = 0x25252F
SKIN, SKIN_D = 0xEBBE90, 0xC99468
STEEL, STEEL_D, STEEL_L = 0xA9B3C4, 0x77839A, 0xDCE3EE
LEATHER, LEATHER_D = 0x92623A, 0x5F3E24
CLOTH, CLOTH_D = 0xCFC7AE, 0xA39B84
GREY, GREY_D = 0x8E919C, 0x626571
GOLD = 0xE0B030
WOOD = 0xA67540
HORSE, HORSE_D, HORSE_L = 0xB28A5E, 0x80603E, 0xD2B088
WHITE = 0xF3F3F0
OLIVE, OLIVE_D = 0x7E8B67, 0x5A6648


def head(p, cx, top, b, hair=LEATHER_D, w=4):
    x0 = cx - w // 2
    p.rect(x0, top - b, x0 + w - 1, top - b + 3, SKIN)
    p.rect(x0, top - b, x0 + w - 1, top - b, hair)
    p.set(x0 + w - 1, top - b + 1, hair) if False else None
    p.set(x0 + w - 2, top - b + 2, 0x30303A)  # eye


def legs(p, cx, top, c=GREY_D, boot=LEATHER_D, spread=2):
    for lx in (cx - spread, cx + spread - 1):
        p.rect(lx, top, lx + 1, FEET, c)
        p.rect(lx, FEET - 1, lx + 1, FEET, boot)


def lord(b):
    p = Pix(T, T)
    p.rect(6, 8 - b, 8, 15, CLOTH_D)  # cape behind
    legs(p, 12, 13)
    p.rect(8, 8 - b, 15, 14, STEEL)
    p.rect(8, 8 - b, 15, 8 - b, STEEL_L)
    p.rect(14, 9 - b, 15, 14, STEEL_D)
    p.rect(8, 12, 15, 12, GOLD)  # belt
    head(p, 12, 3, b, hair=0xD8B24A)
    p.rect(10, 2 - b, 13, 2 - b, GOLD)  # circlet
    p.rect(16, 10, 16, 11, GOLD)  # sword: hilt, guard, blade
    p.rect(15, 10, 17, 10, GOLD)
    p.line(16, 9, 19, 2 - b, STEEL_L)
    p.line(17, 9, 20, 2 - b, STEEL)
    p.outline(OUT)
    return p


def horse(p, x0, y0, b, flap=0):
    """A horse facing right: body x0..x0+13, legs to FEET."""
    p.ellipse(x0 + 6, y0 + 3, 7, 3.4, HORSE)
    p.ellipse(x0 + 5, y0 + 5, 5, 1.5, HORSE_D)
    for lx in (x0 + 1, x0 + 4, x0 + 9, x0 + 12):
        sway = 1 if (lx in (x0 + 4, x0 + 12) and b) else 0
        p.rect(lx, y0 + 5, lx + 1, FEET - 1, HORSE)
        p.rect(lx + sway, FEET - 1, lx + 1 + sway, FEET, LEATHER_D)
    p.rect(x0 + 11, y0 - 3, x0 + 13, y0 + 1, HORSE)  # neck
    p.rect(x0 + 13, y0 - 3, x0 + 16, y0, HORSE)  # head
    p.rect(x0 + 14, y0 - 4, x0 + 14, y0 - 4, HORSE_D)
    p.set(x0 + 15, y0 - 2, 0x30303A)
    p.rect(x0 + 10, y0 - 3, x0 + 11, y0 + 1, HORSE_D)  # mane
    p.rect(x0 - 2, y0 + 1, x0, y0 + 2 + b, HORSE_D)  # tail


def cavalier(b):
    p = Pix(T, T)
    horse(p, 3, 9, b)
    p.rect(8, 6 - b, 11, 9, STEEL)  # rider
    p.rect(8, 6 - b, 11, 6 - b, STEEL_L)
    p.rect(9, 2 - b, 11, 5 - b, SKIN)
    p.rect(9, 1 - b, 11, 2 - b, STEEL)
    p.set(11, 4 - b, 0x30303A)
    p.line(13, 8, 21, 3 + b, WOOD)  # lance
    p.set(21, 3 + b, STEEL_L)
    p.set(20, 3 + b, STEEL_L)
    p.outline(OUT)
    return p


def archer(b):
    p = Pix(T, T)
    legs(p, 12, 13, c=OLIVE_D)
    p.rect(8, 8 - b, 15, 14, OLIVE)
    p.rect(14, 9 - b, 15, 14, OLIVE_D)
    p.rect(8, 12, 15, 12, LEATHER)
    head(p, 12, 3, b, hair=OLIVE_D)
    p.rect(9, 2 - b, 14, 3 - b, OLIVE_D)  # hood
    p.rect(9, 4 - b, 10, 6 - b, OLIVE_D)
    p.rect(15, 7 - b, 15, 11, LEATHER_D)  # quiver
    p.line(16, 5 - b, 17, 9, LEATHER_D) if False else None
    # bow on the right side, string, arrow
    for y, x in ((3, 19), (4, 20), (5, 20), (6, 21), (7, 21), (8, 21), (9, 21), (10, 21), (11, 20), (12, 20), (13, 19)):
        p.set(x, y - b if y < 8 else y, WOOD)
    p.line(19, 3 - b, 19, 13, WHITE)
    p.line(10, 9, 20, 8, STEEL_L)
    p.outline(OUT)
    return p


def fighter(b):
    p = Pix(T, T)
    legs(p, 12, 13, c=LEATHER, boot=LEATHER_D)
    p.rect(7, 8 - b, 16, 14, SKIN)  # bare arms
    p.rect(9, 8 - b, 14, 14, LEATHER)  # vest
    p.rect(9, 12, 14, 12, LEATHER_D)
    head(p, 12, 3, b, hair=0x6B3F22)
    p.rect(10, 3 - b, 13, 3 - b, 0xC8483A)  # headband
    p.rect(9, 3 - b, 9, 4 - b, 0xC8483A)
    # axe
    p.line(18, 4 - b, 18, 15, WOOD)
    p.rect(16, 3 - b, 19, 6 - b, STEEL)
    p.rect(15, 4 - b, 16, 5 - b, STEEL_L)
    p.outline(OUT)
    return p


def knight(b):
    p = Pix(T, T)
    legs(p, 12, 13, c=STEEL_D, boot=GREY_D, spread=3)
    p.rect(7, 7 - b, 16, 14, STEEL)
    p.rect(7, 7 - b, 16, 7 - b, STEEL_L)
    p.rect(14, 8 - b, 16, 14, STEEL_D)
    p.rect(9, 3 - b, 14, 7 - b, STEEL)  # great helm
    p.rect(9, 3 - b, 14, 3 - b, STEEL_L)
    p.rect(10, 5 - b, 13, 5 - b, 0x2A2A33)  # visor slit
    p.rect(11, 0 - b if b == 0 else 0, 12, 2 - b, 0xA6A6B0)  # plume
    # tower shield on the left
    p.rect(3, 8, 8, 16, GREY)
    p.rect(3, 8, 8, 8, STEEL_L)
    p.rect(5, 9, 6, 15, GOLD)
    p.rect(3, 12, 8, 12, GOLD)
    # spear/sword
    p.line(18, 12, 18, 2 - b, WOOD)
    p.set(18, 1 - b, STEEL_L); p.set(18, 2 - b, STEEL_L)
    p.outline(OUT)
    return p


def soldier(b):
    p = Pix(T, T)
    legs(p, 12, 13, c=GREY_D)
    p.rect(8, 8 - b, 15, 14, 0xB8AE92)
    p.rect(14, 9 - b, 15, 14, CLOTH_D)
    p.rect(8, 12, 15, 12, LEATHER)
    head(p, 12, 3, b, hair=LEATHER_D)
    p.rect(9, 2 - b, 14, 3 - b, STEEL)  # helmet
    p.rect(9, 2 - b, 14, 2 - b, STEEL_L)
    p.circle(7, 11, 3, WOOD)  # round shield
    p.circle(7, 11, 1.2, GOLD)
    p.line(18, 15, 18, 1 - b, WOOD)  # spear
    p.rect(18, 0 - b if b == 0 else 0, 18, 1 - b, STEEL_L)
    p.outline(OUT)
    return p


def pegasus(b):
    p = Pix(T, T)
    horse(p, 3, 9, b)
    p.ellipse(9, 12, 6.5, 3.2, WHITE)
    for lx in (4, 7, 12, 15):  # legs recolored white
        p.rect(lx, 14, lx + 1, FEET - 1, WHITE)
    p.ellipse(8, 13, 4.5, 1.2, 0xD6DDE6)
    p.rect(14, 6, 16, 10, WHITE)
    p.rect(16, 6, 19, 9, WHITE)
    p.set(18, 7, 0x30303A)
    p.rect(13, 6, 14, 10, 0xB5C2D4)
    # wing: a filled fan from the shoulder; raised on frame 0, swept back/down on frame 1
    tip = (2, 0) if b == 0 else (0, 8)
    for bx in range(6, 13):
        p.line(bx, 10, tip[0] + (bx - 6) // 2, tip[1] + (bx - 6) // 3, WHITE)
    p.line(6, 10, tip[0], tip[1], 0xC4CEDC)
    p.line(9, 10, tip[0] + 2, tip[1] + 1, 0xD6DDE6)
    p.rect(8, 6 - b, 10, 9, STEEL)  # rider
    p.rect(9, 3 - b, 11, 5 - b, SKIN)
    p.rect(9, 2 - b, 11, 2 - b, STEEL)
    p.set(11, 4 - b, 0x30303A)
    p.line(12, 8, 18, 3 + b, WOOD)
    p.set(18, 3 + b, STEEL_L)
    p.outline(OUT)
    return p


def infantry(b):
    p = Pix(T, T)
    legs(p, 12, 13, c=OLIVE_D, boot=LEATHER_D)
    p.rect(8, 8 - b, 15, 14, OLIVE)
    p.rect(14, 9 - b, 15, 14, OLIVE_D)
    p.rect(6, 8 - b, 8, 13, LEATHER)  # backpack
    head(p, 12, 3, b, hair=LEATHER_D)
    p.rect(9, 2 - b, 14, 3 - b, STEEL_D)  # helmet
    p.rect(9, 2 - b, 14, 2 - b, STEEL)
    p.rect(14, 3 - b, 14, 3 - b, STEEL_D)
    p.line(13, 11, 20, 8, GREY_D)  # rifle
    p.line(13, 12, 20, 9, 0x3A3A44)
    p.rect(10, 9, 13, 11, SKIN_D) if False else None
    p.outline(OUT)
    return p


def mech(b):
    p = Pix(T, T)
    legs(p, 12, 12, c=GREY_D, boot=0x3E3E48, spread=3)
    p.rect(7, 6 - b, 16, 13, GREY)  # heavy torso
    p.rect(7, 6 - b, 16, 6 - b, STEEL_L)
    p.rect(14, 7 - b, 16, 13, GREY_D)
    p.rect(8, 11, 15, 11, LEATHER_D)
    head(p, 12, 2, b, hair=LEATHER_D)
    p.rect(9, 1 - b, 14, 2 - b, 0x4A5160)  # helmet
    p.rect(10, 3 - b, 13, 3 - b, 0x4A5160) if False else None
    p.rect(4, 9 - b, 18, 11 - b, 0x5C6270)  # bazooka tube on the shoulder
    p.rect(4, 9 - b, 18, 9 - b, STEEL)
    p.rect(18, 8 - b, 20, 12 - b, 0x3A3C44)
    p.rect(2, 9 - b, 3, 11 - b, 0x3A3C44)
    p.outline(OUT)
    return p


def recon(b):
    p = Pix(T, T)
    p.rect(3, 9 + b, 20, 14, OLIVE)  # body
    p.rect(3, 9 + b, 20, 9 + b, 0x9AA783)
    p.rect(3, 14, 20, 14, OLIVE_D)
    p.rect(9, 5 + b, 15, 9 + b, OLIVE)  # cabin
    p.rect(10, 6 + b, 14, 8 + b, 0x7FA4C4)  # windows
    p.rect(12, 6 + b, 12, 8 + b, OLIVE_D)
    p.rect(16, 7 + b, 19, 8 + b, GREY_D)  # mounted gun
    p.rect(19, 6 + b, 23, 6 + b, 0x3A3C44) if False else None
    p.line(15, 6 + b, 22, 4 + b, 0x3A3C44)
    for cx in (6, 17):  # wheels
        p.circle(cx, 14.5, 3, 0x2E2E36)
        p.circle(cx, 14.5, 1.2, STEEL)
        p.set(cx + (1 if b else -1), 14 + (0 if b else 1), 0x555560)
    p.rect(2, 12, 2, 13, GOLD)  # tail light
    p.outline(OUT)
    return p


def tank(b):
    p = Pix(T, T)
    p.rect(2, 12, 21, 16, 0x4C4C56)  # tracks
    for x in range(2 + b, 22, 3):
        p.rect(x, 12, x, 12, 0x6C6C78)
        p.rect(x, 16, x, 16, 0x6C6C78)
    for cx in (4, 8, 12, 16, 19):
        p.circle(cx, 14, 1.5, 0x8A8A96)
    p.rect(4, 8 + b, 19, 12, OLIVE)  # hull
    p.rect(4, 8 + b, 19, 8 + b, 0x9AA783)
    p.rect(17, 9 + b, 19, 12, OLIVE_D)
    p.rect(7, 4 + b, 15, 8 + b, OLIVE)  # turret
    p.rect(7, 4 + b, 15, 4 + b, 0x9AA783)
    p.rect(13, 5 + b, 15, 8 + b, OLIVE_D)
    p.rect(15, 5 + b, 22, 6 + b, 0x3C3C46)  # barrel
    p.rect(22, 5 + b, 23, 6 + b, 0x2A2A33)
    p.rect(9, 2 + b, 10, 3 + b, GREY_D)  # hatch
    p.outline(OUT)
    return p


def artillery(b):
    p = Pix(T, T)
    p.rect(3, 13, 20, 16, 0x4C4C56)  # tracked carriage
    for x in range(3 + b, 21, 3):
        p.rect(x, 13, x, 13, 0x6C6C78)
        p.rect(x, 16, x, 16, 0x6C6C78)
    for cx in (5, 9, 13, 17):
        p.circle(cx, 14.5, 1.4, 0x8A8A96)
    p.rect(4, 10 + b, 15, 13, OLIVE)  # body
    p.rect(4, 10 + b, 15, 10 + b, 0x9AA783)
    p.rect(12, 11 + b, 15, 13, OLIVE_D)
    # long barrel pointing up-right, shaking a pixel on frame 1
    p.line(10, 9 + b, 21, 1 + b, 0x3C3C46)
    p.line(10, 10 + b, 21, 2 + b, 0x3C3C46)
    p.line(11, 8 + b, 22, 1 + b, STEEL)
    p.rect(8, 8 + b, 12, 11 + b, GREY_D)
    p.rect(2, 11, 3, 13, 0x3C3C46)  # trail spade
    p.outline(OUT)
    return p


def apc(b):
    """Armoured supply carrier: a boxy hull on tracks with a hatch and a red cross-like supply crate."""
    p = Pix(T, T)
    p.rect(2, 12, 21, 16, 0x4C4C56)  # tracks
    for x in range(2 + b, 22, 3):
        p.rect(x, 12, x, 12, 0x6C6C78)
        p.rect(x, 16, x, 16, 0x6C6C78)
    for cx in (4, 8, 12, 16, 19):
        p.circle(cx, 14, 1.5, 0x8A8A96)
    p.rect(3, 6 + b, 20, 12, OLIVE)  # tall box hull
    p.rect(3, 6 + b, 20, 6 + b, 0x9AA783)
    p.rect(17, 7 + b, 20, 12, OLIVE_D)
    p.rect(14, 4 + b, 19, 6 + b, OLIVE)  # cab
    p.rect(16, 5 + b, 18, 5 + b, 0x7FA4C4)  # window slit
    p.rect(6, 8 + b, 11, 11, WHITE)  # supply marking
    p.rect(8, 8 + b, 9, 11, 0xC03A2E)
    p.rect(6, 9 + b, 11, 10, 0xC03A2E)
    p.outline(OUT)
    return p


# Unit type id -> drawing function. The sheet row of a unit is its position in ORDER.
UNITS = {
    'lord': lord, 'cavalier': cavalier, 'archer': archer, 'fighter': fighter, 'knight': knight,
    'soldier': soldier, 'pegasus': pegasus,
    'infantry': infantry, 'mech': mech, 'recon': recon, 'tank': tank, 'artillery': artillery,
    'apc': apc,
}
ORDER = list(UNITS.keys())
