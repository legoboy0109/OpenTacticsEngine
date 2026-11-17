#!/usr/bin/env python3
"""Original pixel art for the battle scene (docs/BATTLE_SCENE.md), drawn by this script. No third-party art, no Pillow:
the PNG writer is plain zlib, so tests can import this file.

  python3 tools/art/battle_art.py OUT_DIR        # writes battle_knight.png, battle_knight_red.png, battle_fx.png, battle_meadow.png, walk.png

Sheets (the sheet's cell size is the frame size):
  battle_knight.png   48 x 48 frames, 4 columns x 6 rows: row 0 stand (2), row 1 attack (4, the sword lands on frame 2),
                      row 2 hurt (2), row 3 dodge (2), row 4 defeat (3), row 5 cast (3). No crit row: a crit plays the attack.
                      Foot point (24, 46). The art looks to the right.
  battle_knight_red.png  the same sheet in red (for a side's own sheet: "sides" of a set).
  walk.png            32 x 32 frames, 4 columns x 3 rows: a small walking figure, row 0 facing down, row 1 up, row 2 to the right
                      (the figure's shirt differs per row, so a picture shows which row was drawn). For walking frames of a unit picture.
  battle_fx.png       16 x 16 frames, 4 columns x 1 row: an arrow (2 frames) and a spark (2 frames).
  battle_meadow.png   one 240 x 135 background picture: sky, hills and a meadow.
`BATTLE_SET` / `BATTLE_SHEETS` are the matching data/graphics.json entries.
"""
import os
import struct
import sys
import zlib

TRANSPARENT = (0, 0, 0, 0)
OUTLINE = (22, 22, 34, 255)
ARMOR = (92, 118, 172, 255)
ARMOR_LIGHT = (142, 170, 214, 255)
ARMOR_DARK = (60, 78, 124, 255)
CLOTH = (186, 58, 58, 255)
SKIN = (232, 192, 150, 255)
STEEL = (226, 230, 240, 255)
STEEL_DARK = (150, 158, 176, 255)
WOOD = (150, 106, 60, 255)
WOOD_LIGHT = (190, 140, 84, 255)
GOLD = (236, 200, 70, 255)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[TRANSPARENT] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def outline(self, c=OUTLINE):
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x][3] == 0 and any(
                        0 <= x + dx < self.w and 0 <= y + dy < self.h and self.px[y + dy][x + dx][3] != 0
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add:
            self.px[y][x] = c

    def tint(self, rgb, amount):
        """Mixes every opaque pixel towards rgb (a hit flash)."""
        for y in range(self.h):
            for x in range(self.w):
                r, g, b, a = self.px[y][x]
                if a:
                    self.px[y][x] = (int(r + (rgb[0] - r) * amount), int(g + (rgb[1] - g) * amount), int(b + (rgb[2] - b) * amount), a)

    def paste(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                if other.px[y][x][3]:
                    self.set(ox + x, oy + y, other.px[y][x])


def write_png(path, canvas):
    raw = b''.join(b'\x00' + b''.join(bytes(p) for p in row) for row in canvas.px)

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)

    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', canvas.w, canvas.h, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(data)


# ---- the knight --------------------------------------------------------------------------------------------------------

def knight(lean=0, crouch=0, tip=(36, 12), hand=(33, 29), slash=False, head_drop=0, lying=False, cast=False):
    """One 48 x 48 frame of the knight (feet at y 46, middle x 24), looking right."""
    c = Canvas(48, 48)
    if lying:  # fallen: a long low shape along the ground
        c.rect(8, 41, 36, 45, ARMOR)
        c.rect(8, 41, 36, 42, ARMOR_LIGHT)
        c.rect(36, 40, 43, 45, SKIN)
        c.rect(38, 41, 40, 42, OUTLINE)
        c.rect(4, 42, 8, 45, ARMOR_DARK)
        c.line(14, 46, 46, 46, STEEL_DARK)
        c.outline()
        return c
    top = crouch
    # legs
    c.rect(19 + lean // 2, 38 + top, 22 + lean // 2, 46, ARMOR_DARK)
    c.rect(26 + lean // 2, 38 + top, 29 + lean // 2, 46, ARMOR_DARK)
    c.rect(18 + lean // 2, 45, 22 + lean // 2, 46, OUTLINE)
    c.rect(26 + lean // 2, 45, 30 + lean // 2, 46, OUTLINE)
    # cape behind
    c.rect(15 + lean, 26 + top, 18 + lean, 37 + top, CLOTH)
    # torso
    x0 = 18 + lean
    c.rect(x0, 25 + top, x0 + 12, 38 + top, ARMOR)
    c.rect(x0, 25 + top, x0 + 12, 27 + top, ARMOR_LIGHT)
    c.rect(x0 + 2, 33 + top, x0 + 10, 34 + top, ARMOR_DARK)
    c.rect(x0 + 5, 29 + top, x0 + 7, 31 + top, GOLD)
    # shield on the back arm
    c.rect(x0 - 5, 27 + top, x0 - 1, 36 + top, WOOD)
    c.rect(x0 - 5, 27 + top, x0 - 1, 28 + top, WOOD_LIGHT)
    c.set(x0 - 3, 31 + top, GOLD)
    # head with a helmet
    hx = 20 + lean + (head_drop // 2)
    hy = 15 + top + head_drop
    c.rect(hx, hy, hx + 8, hy + 9, ARMOR_LIGHT)
    c.rect(hx + 4, hy + 4, hx + 8, hy + 5, OUTLINE)
    c.rect(hx + 5, hy + 7, hx + 8, hy + 9, SKIN)
    c.rect(hx + 3, hy - 4, hx + 5, hy - 1, CLOTH)
    # the sword arm and the sword
    hxp, hyp = hand
    hxp += lean
    hyp += top
    c.rect(x0 + 11, hyp - 1, hxp, hyp + 1, ARMOR_LIGHT)
    tx, ty = tip
    c.line(hxp, hyp, tx + lean, ty + top, STEEL)
    c.line(hxp, hyp + 1, tx + lean, ty + top + 1, STEEL_DARK)
    c.rect(hxp - 1, hyp - 1, hxp + 1, hyp + 1, GOLD)
    if slash:  # the arc of the blow
        for i in range(6):
            c.set(tx + lean - 3 - i * 2, ty + top - 3 - i, (255, 255, 255, 255 - i * 30))
            c.set(tx + lean - 2 - i * 2, ty + top - 3 - i, (255, 255, 255, 220 - i * 30))
    if cast:  # a glow in the free hand
        c.rect(hxp - 2, hyp - 8, hxp + 2, hyp - 4, (255, 240, 150, 255))
    c.outline()
    return c


def knight_sheet():
    sheet = Canvas(48 * 4, 48 * 6)

    def put(col, row, frame):
        sheet.paste(frame, col * 48, row * 48)

    # row 0: stand (the sword held up, a slow breath)
    put(0, 0, knight())
    put(1, 0, knight(crouch=1, tip=(36, 13), hand=(33, 30)))
    # row 1: attack, the blow lands on frame 2
    put(0, 1, knight(lean=-2, tip=(24, 6), hand=(31, 25)))
    put(1, 1, knight(lean=0, tip=(41, 14), hand=(34, 26)))
    put(2, 1, knight(lean=4, tip=(47, 30), hand=(37, 29), slash=True))
    put(3, 1, knight(lean=2, tip=(42, 38), hand=(36, 31)))
    # row 2: hurt (flashes red)
    f = knight(lean=-3, head_drop=1, tip=(32, 14))
    f.tint((255, 70, 70), 0.55)
    put(0, 2, f)
    f = knight(lean=-5, crouch=1, head_drop=2, tip=(30, 18))
    f.tint((255, 70, 70), 0.3)
    put(1, 2, f)
    # row 3: dodge
    put(0, 3, knight(lean=-4, crouch=2, tip=(30, 14), hand=(30, 30)))
    put(1, 3, knight(lean=-7, crouch=3, tip=(28, 16), hand=(28, 31)))
    # row 4: defeat
    put(0, 4, knight(lean=-3, crouch=6, head_drop=2, tip=(26, 30), hand=(30, 34)))
    put(1, 4, knight(lean=-6, crouch=12, head_drop=5, tip=(20, 40), hand=(26, 40)))
    put(2, 4, knight(lying=True))
    # row 5: cast (a free hand glowing)
    put(0, 5, knight(cast=True, tip=(36, 12)))
    put(1, 5, knight(cast=True, lean=2, tip=(38, 10), hand=(35, 26)))
    put(2, 5, knight(cast=True, lean=3, tip=(40, 20), hand=(36, 27)))
    return sheet


def fx_sheet():
    s = Canvas(16 * 4, 16)

    def arrow(ox, wobble):
        c = Canvas(16, 16)
        c.line(1, 8 + wobble, 13, 8 + wobble, WOOD_LIGHT)
        c.rect(13, 7 + wobble, 15, 9 + wobble, STEEL)
        c.set(0, 7 + wobble, CLOTH)
        c.set(0, 9 + wobble, CLOTH)
        c.outline()
        s.paste(c, ox, 0)

    arrow(0, 0)
    arrow(16, 1)

    def spark(ox, r):
        c = Canvas(16, 16)
        for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(8 + dx, 8 + dy, (255, 250, 200, 255))
        for k in range(1, r + 1):
            for dx, dy in ((k, 0), (-k, 0), (0, k), (0, -k), (k, k), (-k, -k), (k, -k), (-k, k)):
                c.set(8 + dx, 8 + dy, GOLD if k % 2 else (255, 150, 60, 255))
        s.paste(c, ox, 0)

    spark(32, 3)
    spark(48, 6)
    return s


def meadow():
    c = Canvas(240, 135)
    for y in range(135):
        if y < 80:  # sky
            t = y / 80.0
            col = (int(70 + 90 * t), int(120 + 80 * t), int(200 + 40 * t), 255)
        else:  # meadow
            t = (y - 80) / 55.0
            col = (int(86 - 30 * t), int(150 - 40 * t), int(80 - 30 * t), 255)
        for x in range(240):
            c.px[y][x] = col
    # far hills
    for x in range(240):
        h = int(60 + 9 * ((x * 7) % 31) / 31.0 + 14 * abs(((x + 40) % 120) - 60) / 60.0)
        for y in range(h, 80):
            c.px[y][x] = (96, 142, 120, 255) if (x // 2 + y) % 11 else (104, 150, 126, 255)
    # a few clouds
    for cx, cy in ((40, 24), (150, 16), (205, 34)):
        for dx in range(-14, 15):
            for dy in range(-3, 4):
                if (dx / 14.0) ** 2 + (dy / 3.0) ** 2 <= 1:
                    c.set(cx + dx, cy + dy, (245, 248, 255, 255))
    # grass tufts
    for i in range(70):
        gx, gy = (i * 53) % 240, 84 + (i * 29) % 50
        c.set(gx, gy, (120, 190, 100, 255))
        c.set(gx, gy - 1, (120, 190, 100, 255))
    return c


# What data/graphics.json needs for them (also written by make_demo_json()).
BATTLE_SHEETS = [
    {"id": "battle_knight", "file": "art/battle_knight.png", "cellWidth": 48, "cellHeight": 48},
    {"id": "battle_fx", "file": "art/battle_fx.png", "cellWidth": 16, "cellHeight": 16},
    {"id": "battle_meadow", "file": "art/battle_meadow.png", "cellWidth": 240, "cellHeight": 135},
]
BATTLE_SET = {
    "sets": {
        "knight": {
            "anchor": [24, 46],
            "animations": {
                "stand": {"sheet": "battle_knight", "frames": [[0, 0], [1, 0]], "ms": 400},
                "attack": {"sheet": "battle_knight", "frames": [[0, 1], [1, 1], [2, 1], [3, 1]], "ms": [140, 90, 110, 160], "hit": 2},
                "hurt": {"sheet": "battle_knight", "frames": [[0, 2], [1, 2]], "ms": 160},
                "dodge": {"sheet": "battle_knight", "frames": [[0, 3], [1, 3]], "ms": 160},
                "defeat": {"sheet": "battle_knight", "frames": [[0, 4], [1, 4], [2, 4]], "ms": [160, 160, 300]},
                "cast": {"sheet": "battle_knight", "frames": [[0, 5], [1, 5], [2, 5]], "ms": [150, 130, 200], "hit": 2, "spawn": 1},
            },
        }
    },
    "effects": [],
    "backgrounds": {"meadow": {"sheet": "battle_meadow", "cell": [0, 0]}},
    "defaultBackground": "meadow",
}


def red_knight_sheet():
    sheet = knight_sheet()
    sheet.tint((214, 40, 40), 0.55)
    return sheet


# One frame of the walking figure: facing "down", "up" or "right"; frame 0..3 swings the legs.
SHIRTS = {"down": (70, 150, 230, 255), "up": (230, 190, 60, 255), "right": (90, 200, 110, 255)}


def walk_figure(facing, frame):
    c = Canvas(32, 32)
    swing = (0, 3, 0, -3)[frame % 4]
    shirt = SHIRTS[facing]
    if facing == "right":
        c.rect(13, 12, 19, 22, shirt)
        c.rect(14 + swing, 23, 17 + swing, 29, (60, 60, 90, 255))
        c.rect(15 - swing, 23, 18 - swing, 29, (60, 60, 90, 255))
        c.rect(13, 3, 20, 11, SKIN)
        c.rect(18, 6, 19, 7, OUTLINE)
    else:
        c.rect(11, 12, 21, 22, shirt)
        c.rect(12, 23, 15, 29 + (1 if swing > 0 else 0), (60, 60, 90, 255))
        c.rect(17, 23, 20, 29 + (1 if swing < 0 else 0), (60, 60, 90, 255))
        c.rect(12, 3, 20, 11, SKIN if facing == "down" else (120, 80, 50, 255))
        if facing == "down":
            c.rect(14, 6, 15, 7, OUTLINE)
            c.rect(17, 6, 18, 7, OUTLINE)
    c.outline()
    return c


def walk_sheet():
    sheet = Canvas(32 * 4, 32 * 3)
    for row, facing in enumerate(("down", "up", "right")):
        for col in range(4):
            sheet.paste(walk_figure(facing, col), col * 32, row * 32)
    return sheet


def main(out):
    os.makedirs(out, exist_ok=True)
    write_png(os.path.join(out, 'battle_knight.png'), knight_sheet())
    write_png(os.path.join(out, 'battle_knight_red.png'), red_knight_sheet())
    write_png(os.path.join(out, 'walk.png'), walk_sheet())
    write_png(os.path.join(out, 'battle_fx.png'), fx_sheet())
    write_png(os.path.join(out, 'battle_meadow.png'), meadow())


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
