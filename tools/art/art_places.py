"""Starter pictures of the strategy-RPG template's doors, chests and villages (docs/SRPG.md section 10).

Each is 24 x 28 like the other one-square structures (art_structures.py draws the paved plot they stand on),
and each look of a place is a picture of its own: the shut and the open door, the shut and the opened
chest, the village, the visited village and the ruin. Original art, drawn from shapes; no input files.
"""
import art_structures as st
from pix import Pix

OUT = st.OUT
WOOD, WOOD_D, WOOD_L = 0x7A5A3A, 0x5A402A, 0x9A7650
IRON, GOLD = 0x4A4A52, 0xE0B030


def _stone_gate(p, opening):
    """The stone frame both door pictures share; `opening` is drawn inside the arch."""
    st.plot(p, 0, 22, 23, 27, 0xB4AE9C, 0xD0CAB8, 0x8E8878)
    p.rect(2, 5, 21, 22, 0xB4AE9C)
    p.rect(2, 5, 21, 6, 0xD0CAB8)
    p.rect(19, 7, 21, 22, 0x8E8878)
    for y in (10, 15, 20):  # courses of stone
        p.rect(2, y, 6, y, 0x9C9684)
        p.rect(17, y, 21, y, 0x9C9684)
    for x in range(2, 22, 4):  # battlements
        p.rect(x, 3, x + 1, 4, 0xB4AE9C)
        p.rect(x, 3, x + 1, 3, 0xD0CAB8)
    opening(p)
    p.outline(OUT)


def door():
    """A shut wooden door in a stone gate: planks, iron bands and a handle."""
    p = Pix(24, 28)

    def leaf(p):
        p.rect(7, 11, 16, 22, WOOD)
        p.rect(8, 9, 15, 10, WOOD)  # the rounded top of the arch
        for x in (10, 13):
            p.rect(x, 10, x, 22, WOOD_D)
        p.rect(7, 14, 16, 14, IRON)
        p.rect(7, 19, 16, 19, IRON)
        p.rect(7, 11, 7, 22, WOOD_D)
        p.set(14, 17, GOLD)
        p.set(14, 16, GOLD)
    _stone_gate(p, leaf)
    return p


def door_open():
    """The same gate open: a dark doorway with the leaf swung back against the wall."""
    p = Pix(24, 28)

    def doorway(p):
        p.rect(7, 11, 16, 22, 0x2E2A26)
        p.rect(8, 9, 15, 10, 0x2E2A26)
        p.rect(9, 14, 14, 22, 0x4A423A)  # a lighter floor of the room beyond
        p.rect(4, 11, 6, 22, WOOD)       # the leaf, edge on
        p.rect(4, 11, 4, 22, WOOD_D)
        p.rect(4, 14, 6, 14, IRON)
        p.rect(4, 19, 6, 19, IRON)
    _stone_gate(p, doorway)
    return p


def _chest_body(p, lid_top):
    """The base of a chest; the lid is drawn by the caller. `lid_top` is the row the closed lid starts on."""
    st.plot(p, 0, 22, 23, 27, 0xC9C3B0, 0xE0DBC8, 0xA8A392)
    p.rect(4, 14, 19, 22, 0xB08040)
    p.rect(4, 14, 19, 14, 0xC89A58)
    p.rect(17, 15, 19, 22, 0x805A28)
    p.rect(4, 17, 19, 17, 0x805A28)  # a seam between planks
    for x in (6, 16):
        p.rect(x, 14, x + 1, 22, IRON)


def chest():
    """A shut chest with a rounded lid and a gold lock."""
    p = Pix(24, 28)
    _chest_body(p, 9)
    p.rect(4, 10, 19, 13, 0xB08040)
    p.rect(5, 9, 18, 9, 0xB08040)
    p.rect(5, 9, 18, 9, 0xC89A58)
    p.rect(4, 13, 19, 13, 0x805A28)
    for x in (6, 16):
        p.rect(x, 9, x + 1, 13, IRON)
    p.rect(11, 12, 12, 16, GOLD)
    p.set(11, 14, 0x805A28)
    p.outline(OUT)
    return p


def chest_open():
    """The chest with its lid thrown back and gold showing."""
    p = Pix(24, 28)
    _chest_body(p, 9)
    p.rect(4, 5, 19, 10, 0x805A28)   # the open lid, seen from behind
    p.rect(5, 6, 18, 9, 0x6A4820)
    p.rect(4, 5, 19, 5, 0xC89A58)
    for x in (6, 16):
        p.rect(x, 5, x + 1, 10, IRON)
    p.rect(5, 11, 18, 13, 0x3A2A18)  # the inside
    for x, y in ((7, 11), (10, 12), (13, 11), (15, 12), (9, 13)):  # coins and a gem
        p.rect(x, y, x + 1, y, GOLD)
    p.set(12, 12, 0xD04040)
    p.outline(OUT)
    return p


def _house(p, x0, y0, w, h, wall, wall_d, roof, roof_d, door_c):
    """A small house: walls, a pitched roof, a door."""
    p.rect(x0, y0 + 4, x0 + w - 1, y0 + h - 1, wall)
    p.rect(x0 + w - 2, y0 + 4, x0 + w - 1, y0 + h - 1, wall_d)
    for i in range(4):  # the pitched roof, wider than the walls
        p.rect(x0 - 1 + i, y0 + 3 - i, x0 + w - i, y0 + 3 - i, roof)
    p.rect(x0 - 1, y0 + 3, x0 + w, y0 + 3, roof_d)
    dx = x0 + w // 2 - 1
    p.rect(dx, y0 + h - 4, dx + 1, y0 + h - 1, door_c)


def village():
    """Two cottages with red roofs, a little smoke from the chimney."""
    p = Pix(24, 28)
    st.plot(p, 0, 22, 23, 27, 0xC9B98A, 0xDDCFA4, 0xA89868)
    _house(p, 2, 10, 10, 13, 0xE6D8B0, 0xC4B68C, 0xB8503C, 0x8C3A2A, 0x5A402A)
    _house(p, 13, 6, 9, 17, 0xE6D8B0, 0xC4B68C, 0xC8663A, 0x9A4A2A, 0x5A402A)
    p.rect(5, 12, 5, 13, 0x6A6A72)
    p.set(4, 10, 0xD8D8DC); p.set(5, 9, 0xEAEAEE); p.set(5, 8, 0xD8D8DC)
    p.outline(OUT)
    return p


def village_visited():
    """The village after a visit: the same cottages, shuttered and quiet (muted colours, no smoke)."""
    p = Pix(24, 28)
    st.plot(p, 0, 22, 23, 27, 0xB0A582, 0xC4BA98, 0x908660)
    _house(p, 2, 10, 10, 13, 0xBFB394, 0xA09578, 0x7C6A5C, 0x5E4E44, 0x4A3A2C)
    _house(p, 13, 6, 9, 17, 0xBFB394, 0xA09578, 0x8C7466, 0x6A5448, 0x4A3A2C)
    for x0, y0 in ((4, 15), (16, 12)):  # closed shutters
        p.rect(x0, y0, x0 + 1, y0 + 2, 0x5A402A)
    p.outline(OUT)
    return p


def village_ruin():
    """Burnt walls, fallen beams and rubble where the village stood."""
    p = Pix(24, 28)
    st.plot(p, 0, 22, 23, 27, 0x8E8672, 0xA09884, 0x6E6654)
    for x0, x1, top in ((2, 8, 15), (14, 21, 12)):  # broken walls with ragged tops
        p.rect(x0, top, x1, 22, 0x6A5E50)
        p.rect(x1 - 1, top, x1, 22, 0x4A4036)
        for x in range(x0, x1 + 1, 2):
            p.set(x, top - 1, 0x6A5E50)
    p.line(9, 14, 15, 21, 0x3A2A1C)   # a fallen beam
    p.line(10, 14, 16, 21, 0x5A402A)
    for x, y in ((9, 20), (11, 21), (12, 19), (6, 21), (17, 21), (4, 20)):  # rubble
        p.rect(x, y, x + 1, y + 1, 0x4A4036)
    p.set(10, 9, 0x6A6A72); p.set(11, 7, 0x8A8A92); p.set(10, 5, 0x6A6A72)  # smoke
    p.outline(OUT)
    return p


# Structure id -> {(width, height): picture function}; merged into art_structures.PICTURES.
PICTURES = {
    'door': {(1, 1): door},
    'door_open': {(1, 1): door_open},
    'chest': {(1, 1): chest},
    'chest_open': {(1, 1): chest_open},
    'village': {(1, 1): village},
    'village_visited': {(1, 1): village_visited},
    'village_ruin': {(1, 1): village_ruin},
}
