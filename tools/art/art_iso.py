"""Starter art for the isometric kind of view (docs/VIEWS.md): terrain blocks,
structures on plinths and unit sprites cut for standing on a diamond.

Geometry matches the engine's defaults for tileSize 24: a square is a 48 x 24
diamond and one height level is 12 pixels. A terrain picture is drawn 48 wide
with its bottom edge 12 pixels under the diamond (the side faces); whatever is
above the diamond (trees, peaks) sticks up."""
import art_structures as st
import art_tiles as tl
import art_units as un
from pix import Pix, hexc, rng

TW, TH, STEP = 48, 24, 12
CW, CH = 48, 56           # terrain cell: 20 rows of room above the diamond
TOP = CH - STEP - TH      # row of the diamond's top corner
UNIT_W, UNIT_H = 24, 22   # unit cell: the top-down sprite without the rows under its feet

EARTH = (0x8D6B45, 0x6C5035)   # side faces: lit (left), shaded (right)
ROCK = (0x8E8878, 0x6B6658)
GRASS, GRASS_D, GRASS_L = tl.GRASS


def face(px, py, w=1, h=1, ox=0, oy=0):
    """(inside, gx, gy) of pixel (px, py) for the top face of a w x h footprint whose box starts at (ox, oy)."""
    hw, hh = TW / 2, TH / 2
    u = (px + 0.5 - ox - h * hw) / hw
    v = (py + 0.5 - oy) / hh
    gx, gy = (u + v) / 2, (v - u) / 2
    return (0 <= gx < w and 0 <= gy < h), gx, gy


def shade(c, k):
    r, g, b, a = c if isinstance(c, tuple) else hexc(c)
    return (int(r * k), int(g * k), int(b * k), a)


def block(p, top, sides, w=1, h=1, ox=0, oy=TOP, depth=STEP, lip=None):
    """Draws a block into p: `top(gx, gy, x, y)` gives the top face's colour, `sides` = (left, right)."""
    fw, fh = (w + h) * TW // 2, (w + h) * TH // 2
    front = ox + w * TW // 2
    for x in range(ox, ox + fw):
        last = None
        for y in range(oy, oy + fh):
            ok, gx, gy = face(x, y, w, h, ox, oy)
            if ok:
                p.set(x, y, top(gx, gy, x, y))
                last = y
        if last is None:
            continue
        p.set(x, last, shade(p.get(x, last), 0.88))
        side = sides[0] if x < front else sides[1]
        for d in range(1, depth + 1):
            c = side
            if lip is not None and d <= 2:
                c = shade(lip, 0.92 if x < front else 0.74)
            elif d == depth:
                c = shade(side, 0.8)
            p.set(x, last + d, c)


def speckle(base, dark, light, seed, dark_n=0.10, light_n=0.05):
    """A top-face colour function: base colour with scattered darker and lighter pixels."""
    r = rng('iso', seed)
    noise = {}
    def top(gx, gy, x, y):
        if (x, y) not in noise:
            t = r.random()
            noise[(x, y)] = dark if t < dark_n else light if t < dark_n + light_n else base
        return noise[(x, y)]
    return top


def grass(seed, flowers=0):
    p = Pix(CW, CH)
    block(p, speckle(GRASS, GRASS_D, GRASS_L, seed), EARTH, lip=GRASS_D)
    r = rng('isoflower', seed)
    for _ in range(flowers):
        x, y = r.randint(14, 34), TOP + r.randint(7, 17)
        p.set(x, y, 0xF2E9A0)
    return p


def forest():
    p = grass(11)
    for cx, cy in ((24, TOP + 3), (14, TOP + 8), (34, TOP + 8), (24, TOP + 13)):
        tl.tree(p, cx, cy)
    return p


def mountain():
    p = Pix(CW, CH)
    block(p, speckle(0x9A9484, 0x7F796A, 0xB3AD9C, 5), ROCK)
    # A peak standing on the block: lit left flank, shaded right flank, snow cap.
    apex_x, apex_y, base_y = 24, 3, TOP + 17
    for y in range(apex_y, base_y + 1):
        t = (y - apex_y) / (base_y - apex_y)
        half = int(1 + t * 17)
        droop = int(t * 5)  # the base follows the diamond's front edges
        for x in range(apex_x - half, apex_x + half + 1):
            if y > base_y - droop + abs(x - apex_x) * 0 - 0 and abs(x - apex_x) > half - droop:
                continue
            c = 0xA9A392 if x < apex_x else 0x7A7466
            if (x + y * 3) % 7 == 0:
                c = 0x8F8979 if x < apex_x else 0x68635A
            if y < apex_y + 6:
                c = 0xF4F4F0 if x <= apex_x else 0xCFD3DA
            p.set(x, y, c)
    for y in range(apex_y, base_y + 1):  # dark ridge and outline
        t = (y - apex_y) / (base_y - apex_y)
        half = int(1 + t * 17)
        droop = int(t * 5)
        if y <= base_y - droop:
            p.set(apex_x - half - 1, y, tl.DARKLINE)
            p.set(apex_x + half + 1, y, tl.DARKLINE)
    return p


def wall():
    p = Pix(CW, CH)
    def top(gx, gy, x, y):
        return 0xC9C4B4 if (int(gx * 4) + int(gy * 4)) % 2 else 0xBDB8A8
    block(p, top, (0x9B9585, 0x777264), oy=TOP - 14, depth=14 + STEP)
    for x in range(CW):  # courses of stone on the side faces
        for y in range(CH):
            c = p.get(x, y)
            if c in (hexc(0x9B9585), hexc(0x777264)) and (y + (x // 6 % 2) * 3) % 6 == 0:
                p.set(x, y, shade(c, 0.82))
    return p


def road():
    p = Pix(CW, CH)
    dirt = speckle(0xC9B48A, 0xB09B74, 0xDCCAA2, 21)
    green = speckle(GRASS, GRASS_D, GRASS_L, 22)
    def top(gx, gy, x, y):
        rim = min(gx, gy, 1 - gx, 1 - gy)
        return green(gx, gy, x, y) if rim < 0.09 else dirt(gx, gy, x, y)
    block(p, top, EARTH, lip=GRASS_D)
    return p


def water(frame, pal):
    deep, mid, light, seed = pal
    p = Pix(CW, CH)
    r = rng('isowater', seed)
    glints = [(r.random(), r.random()) for _ in range(9)]
    def top(gx, gy, x, y):
        for wx, wy in glints:
            # short ripples lying along the diamond, sliding with the frame
            if abs(gy - wy) < 0.035 and 0 <= (gx - wx - frame * 0.07) % 1.0 < 0.16:
                return light
        return mid
    block(p, top, (shade(deep, 0.95), shade(deep, 0.75)))
    return p


def bridge():
    p = Pix(CW, CH)
    def top(gx, gy, x, y):
        if gy < 0.10 or gy > 0.90:
            return 0x6E4A2A  # rails along the crossing
        return 0xA67B4C if int(gx * 6) % 2 else 0xB98A58
    block(p, top, (0x7A5632, 0x5C4026))
    return p


TERRAIN = {
    'plain0': lambda: grass(1), 'plain1': lambda: grass(2), 'plain2': lambda: grass(3, flowers=4),
    'forest': forest, 'mountain': mountain, 'wall': wall, 'road': road, 'bridge': bridge,
    'sea0': lambda: water(0, tl.SEA), 'sea1': lambda: water(1, tl.SEA),
    'water0': lambda: water(0, tl.RIVER), 'water1': lambda: water(1, tl.RIVER),
}
ORDER = list(TERRAIN.keys())


def terrain_sheet():
    sheet = Pix(CW * len(ORDER), CH)
    for i, name in enumerate(ORDER):
        sheet.paste(TERRAIN[name](), i * CW, 0)
    return sheet


def terrain_looks(terrain_ids, sheet_id, frame_ms):
    def cell(name):
        return [ORDER.index(name), 0]
    known = {
        'plain': {'sheet': sheet_id, 'variants': [cell('plain0'), cell('plain1'), cell('plain2')]},
        'forest': {'sheet': sheet_id, 'cell': cell('forest')},
        'mountain': {'sheet': sheet_id, 'cell': cell('mountain')},
        'wall': {'sheet': sheet_id, 'cell': cell('wall')},
        'road': {'sheet': sheet_id, 'cell': cell('road')},
        'bridge': {'sheet': sheet_id, 'cell': cell('bridge')},
        'sea': {'sheet': sheet_id, 'frames': [cell('sea0'), cell('sea1')], 'frameMs': frame_ms},
        'water': {'sheet': sheet_id, 'frames': [cell('water0'), cell('water1')], 'frameMs': frame_ms},
    }
    return {tid: known[tid] for tid in terrain_ids if tid in known}


def unit_sheet(unit_ids):
    """Two frames per unit, like the top-down sheet, cut just under the feet."""
    sheet = Pix(2 * UNIT_W, max(1, len(unit_ids)) * UNIT_H)
    for row, uid in enumerate(unit_ids):
        for col in range(2):
            src = un.UNITS[uid](col)
            cut = Pix(UNIT_W, UNIT_H)
            for y in range(UNIT_H):
                for x in range(UNIT_W):
                    cut.p[y][x] = src.get(x, y)
            sheet.paste(cut, col * UNIT_W, row * UNIT_H)
    return sheet


def structure_cell(w, h):
    tw, th = st.cell_size(w, h)
    return (w + h) * TW // 2, (w + h) * TH // 2 + th - 4 * min(w, h)


def structure(sid, w, h):
    """The top-down picture of the building standing on a stone plinth that fills the footprint."""
    cw, ch = structure_cell(w, h)
    fh = (w + h) * TH // 2
    p = Pix(cw, ch)
    def top(gx, gy, x, y):
        return 0xD6D1C0 if (int(gx * 3) + int(gy * 3)) % 2 else 0xC9C3B0
    block(p, top, (0xA8A392, 0x86816F), w, h, oy=ch - fh - 3, depth=3)
    src = st.PICTURES[sid][(w, h)]()
    # The building stands a little behind the plinth's middle; its own paved plot is hidden by it.
    p.paste(src, (cw - src.w) // 2, ch - fh // 2 - src.h + 2 * min(w, h))
    return p
