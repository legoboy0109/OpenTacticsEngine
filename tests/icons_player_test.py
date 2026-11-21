#!/usr/bin/env python3
"""Icons next to item and weapon names in the player's lists (docs/GRAPHICS.md "Icons"), windowless.

  python3 tests/icons_player_test.py ./build/engine/player/ote-player .
  python3 tests/icons_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR              # keep the screenshots (BMP)
  python3 tests/icons_player_test.py ./build/engine/player/ote-player . --baseline OTHER_PLAYER      # a game without icons, byte for byte

Plays a copy of tests/fixtures/icon-lists. Its art/icons.png has flat colour cells (red = iron sword, green = potion, blue = the bow
weapon); graphics.json gives those three items an icon and the other three (charm, salve, cloak) none. Each list that names an item
is opened and photographed: the item menu, the weapon choice, trade, storage, the shop, the unit panel, the unit details and the attack
forecast. The checks read the pixels: the icon is where the row says (same column for every row of a list, centred on the row, at the
sheet's cell size), the names of rows without an icon line up with those that have one, a greyed row's icon is dimmed, a taller icon
makes the rows taller and a smaller one does not. A game without "icons" is photographed too: its names stand exactly the icon
column further left, and (with --baseline) its screenshots equal another build's byte for byte.
"""
import os
import shutil
import struct
import sys
import zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, edit, failures, inp, lines, player_and_repo, read_jsonc, run_text, scratch_dir, shots_dir, summary, write_json  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
BASELINE = os.path.abspath(sys.argv[sys.argv.index('--baseline') + 1]) if '--baseline' in sys.argv else ''
TMP = scratch_dir()
RED, GREEN, BLUE = (221, 41, 43), (41, 199, 61), (61, 91, 229)
PANEL_X = 352  # the info panel starts here (480 wide, panel 128)
ROW_BG = (30, 34, 54)  # the background of an unselected list row (ui/chrome.cpp drawRowBox)
PANEL_BG = (18, 20, 32)  # the item panels' own background (chrome::kBg), seen between the two columns of the trade panel

# ---- the games -----------------------------------------------------------------------------------------------------------


def icon_png(scale):
    """The fixture's icon sheet (4 x 2 cells of 8 x 8 pixels, flat colour with a dark frame), every pixel `scale` times as big."""
    cols = {(0, 0): RED, (1, 0): GREEN, (2, 0): BLUE, (3, 0): (231, 201, 41)}
    cell = 8 * scale
    w, h = 4 * cell, 2 * cell
    rows = []
    for y in range(h):
        row = bytearray([0])
        for x in range(w):
            c = cols.get((x // cell, y // cell))
            if c is None:
                row += bytes((0, 0, 0, 0))
                continue
            ix, iy = (x % cell) // scale, (y % cell) // scale
            row += bytes((30, 30, 40, 255) if ix in (0, 7) or iy in (0, 7) else c + (255,))
        rows.append(bytes(row))

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))


def make_game(gid, icons=True, scale=1, gold=100):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/icon-lists'), g)
    if scale != 1:  # bigger pictures: the cells are 16 x 16 pixels (or whatever the scale says)
        with open(os.path.join(g, 'art', 'icons.png'), 'wb') as f:
            f.write(icon_png(scale))
        edit(g, 'data/graphics.json', lambda gj: gj['sheets'][0].update({'cellWidth': 8 * scale, 'cellHeight': 8 * scale}))
    if not icons:
        edit(g, 'data/graphics.json', lambda gj: gj.pop('icons'))
    if gold != 100:
        edit(g, 'maps/field.json', lambda m: m['sides'][0]['resources'].update({'gold': gold}))
    return g


def play(gid, script, name, extra=()):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--no-saving',
           '--game', gid, '--map', 'field', '--battle-scene', 'map', '--fixed-dt', '0.016', '--input-interval', '4',
           '--frames', str(ticks(script) * 4 + 100), '--input', script, '--screenshot', os.path.join(TMP, name + '.bmp')] + list(extra)
    log = run_text(cmd, timeout=120).stdout
    path = os.path.join(TMP, name + '.bmp')
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return log, Shot(path)


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


# ---- reading pictures ---------------------------------------------------------------------------------------------------


def near(px, color, tol):
    return all(abs(int(a) - b) <= tol for a, b in zip(px[:3], color))


class Shot(Bmp):
    def blocks(self, color, x0=0, x1=None, tol=0):
        """Bounding boxes (x0, y0, x1, y1; inclusive) of the connected groups of pixels of exactly `color` in columns x0..x1-1, by y then x."""
        x1 = x1 or self.w
        seen, found = set(), []
        for y in range(self.h):
            for x in range(x0, x1):
                if (x, y) in seen or not near(self.px[y][x], color, tol):
                    continue
                stack, pts = [(x, y)], []
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    pts.append((cx, cy))
                    for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                        if x0 <= nx < x1 and 0 <= ny < self.h and (nx, ny) not in seen and near(self.px[ny][nx], color, tol):
                            seen.add((nx, ny))
                            stack.append((nx, ny))
                found.append((min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)))
        return sorted(found, key=lambda b: (b[1], b[0]))

    def box_left(self, y, x0, x1):
        """x of the first pixel of a list row's background (an unselected row) in the row at height y, columns x0..x1-1."""
        for x in range(x0, x1):
            if near(self.px[y][x], ROW_BG, 3):
                return x
        return None

    def column_left(self, y, x_from, x_to):
        """x of the left edge of the list column that ends at x_from: scanning left along row y to the panel's own background (the gap between columns)."""
        for x in range(x_from, x_to, -1):
            if near(self.px[y][x], PANEL_BG, 3):
                return x + 1
        return None

    def text_left(self, y0, y1, x0, x1):
        """x of the leftmost light (text) pixel in the rows y0..y1-1 and columns x0..x1-1 (the icon colours do not count), or None."""
        icon = (RED, GREEN, BLUE)
        for x in range(x0, x1):
            for y in range(y0, y1):
                p = tuple(self.px[y][x][:3])
                if min(p) >= 150 and p not in icon:
                    return x
        return None


def interior(box, scale=1):
    """The picture's whole cell from the box of its flat colour: the colour is the 6 x 6 inside of an 8 x 8 cell (a dark frame round it)."""
    return box[0] - scale, box[1] - scale, box[2] + scale, box[3] + scale


# ---- the screens: input scripts (the hero stands on the shop; the action menu is Attack, Items, Trade, Storage, Shop, Info, Wait) ---
OPEN = ['wait:30', 'confirm', 'confirm', 'wait:5']  # select the hero, "move" nowhere: the action menu
SCREENS = {
    'items': OPEN + ['down', 'confirm', 'wait:10'],
    'weapons': OPEN + ['confirm', 'wait:10'],
    'trade': OPEN + ['down', 'down', 'confirm', 'wait:10'],
    'storage': OPEN + ['down'] * 3 + ['confirm', 'wait:10'],
    'shop': OPEN + ['down'] * 4 + ['confirm', 'wait:10'],
    'details': OPEN + ['down'] * 5 + ['confirm', 'wait:8', 'right', 'right', 'wait:6'],
    'forecast': OPEN + ['confirm', 'wait:5', 'confirm', 'wait:10'],
}


def shoot(gid, which, name=None):
    return play(gid, inp(SCREENS[which]), name or gid + '_' + which)[1]


def main():
    # ---- the loader accepts the fixture's art, and the game starts
    make_game('icons')
    make_game('plain', icons=False)
    make_game('tall', scale=2)
    make_game('poor', gold=25)
    log, items = play('icons', inp(SCREENS['items']), 'icons_items')
    check('the fixture loads and the item menu opens', 'items: Iron Sword, Hunting Bow, Potion, Charm' in log and 'ERROR' not in log, log[-300:])
    plain_items = shoot('plain', 'items')

    # ---- the item menu: one icon per row that has one, in one column, at the cell size, centred on the row, rows as tall as before
    r, g, b = (items.blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE))
    check('item menu: one red, one blue and one green icon', len(r) == len(g) == len(b) == 1, (r, g, b))
    if len(r) == len(g) == len(b) == 1:
        r, g, b = r[0], g[0], b[0]
        check('item menu: each icon is a 6 x 6 colour inside its 8 x 8 cell (the sheet\'s cell size)', all(x[2] - x[0] == 5 and x[3] - x[1] == 5 for x in (r, g, b)), (r, g, b))
        check('item menu: the icons share one column', r[0] == g[0] == b[0], (r, g, b))
        pitch = b[1] - r[1]
        check('item menu: the rows keep their height (8 pixel text: 12), in the order of the items', pitch == 12 and g[1] - b[1] == 12, (r, b, g))
        # the names: rows with an icon and the one without start at the same x; without icons in the game they start 11 pixels (8 + 3) earlier
        iconx = interior(r)[0]
        def band(row):
            return r[1] - 1 + row * pitch, r[1] + 9 + row * pitch
        potion = items.text_left(*band(2), iconx + 8, PANEL_X - 8)
        charm = items.text_left(*band(3), iconx, PANEL_X - 8)
        check('item menu: the name of a row without an icon lines up with the names beside icons', potion is not None and potion == charm, (potion, charm))
        charm_plain = plain_items.text_left(*band(3), iconx, PANEL_X - 8)
        check('item menu: without icons in the game the names stand 11 pixels (8 + 3) further left', charm_plain is not None and charm - charm_plain == 11, (charm, charm_plain))
    check('item menu: the game without "icons" shows none of the icon colours', not any(plain_items.blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE)))

    # ---- the unit panel (the right-hand side): the weapon in hand and the item list, with the same icons
    pr, pg, pb = (items.blocks(c, PANEL_X) for c in (RED, GREEN, BLUE))
    check('unit panel: the weapon in hand and the sword in the list, the bow and the potion', len(pr) == 2 and len(pg) == 1 and len(pb) == 1, (pr, pg, pb))
    if len(pr) == 2 and len(pg) == len(pb) == 1:
        check('unit panel: the items list icons share a column', pr[1][0] == pg[0][0] == pb[0][0], (pr, pg, pb))
    check('unit panel: a game without icons has none', not any(plain_items.blocks(c, PANEL_X) for c in (RED, GREEN, BLUE)))

    # ---- the attack's weapon choice
    wp = shoot('icons', 'weapons')
    wr, wb = wp.blocks(RED, 0, PANEL_X), wp.blocks(BLUE, 0, PANEL_X)
    check('weapon choice: the sword and the bow have their icons, in one column', len(wr) == 1 and len(wb) == 1 and wr[0][0] == wb[0][0] and wb[0][1] - wr[0][1] == 12,
          (wr, wb))
    check('weapon choice: no icon colours in the game without icons', not any(shoot('plain', 'weapons').blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE)))

    # ---- trade: both columns share the icon column, so the names of the friend's items (no icons) are indented as well
    tr = shoot('icons', 'trade')
    tr_plain = shoot('plain', 'trade')
    tred, tgreen, tblue = (tr.blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE))
    check('trade: the hero\'s three icons are in one column', len(tred) == len(tgreen) == len(tblue) == 1 and tred[0][0] == tgreen[0][0] == tblue[0][0], (tred, tgreen, tblue))
    if tred:
        y0 = tred[0][1] - 1
        shifts = []
        for row in (0, 1):  # the friend's two rows: Salve and Cloak (the right column: from x = 232 on); the text's distance from its row's left edge
            offs = []
            for shot in (tr, tr_plain):
                y = y0 + row * 12 + 1
                left = shot.column_left(y, PANEL_X - 26, 150)  # (inside the right column: the panel ends near x = 336)
                text = shot.text_left(y0 + row * 12, y0 + row * 12 + 10, left, PANEL_X - 8) if left is not None else None
                offs.append(text - left if text is not None else None)
            shifts.append(offs[0] - offs[1] if None not in offs else None)
        check('trade: the friend\'s names (no icons) are shifted by the icon column too', shifts == [11, 11], shifts)

    # ---- storage and the shop
    st = shoot('icons', 'storage')
    sr, sg = st.blocks(RED, 0, PANEL_X), st.blocks(GREEN, 0, PANEL_X)
    check('storage: the sword and the potion carry their icons', len(sr) == 1 and len(sg) == 1 and sr[0][0] == sg[0][0], (sr, sg))
    sh = shoot('icons', 'shop')
    shr, shg, shb = (sh.blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE))
    check('shop: potion, sword and bow have icons in one column, in the shop\'s order',
          len(shr) == len(shg) == len(shb) == 1 and shr[0][0] == shg[0][0] == shb[0][0] and shg[0][1] < shr[0][1] < shb[0][1], (shr, shg, shb))
    if len(shr) == len(shg) == len(shb) == 1:
        offs = []
        for shot in (sh, shoot('plain', 'shop')):  # the fourth row (Cloak): the text's distance from its row's left edge
            y = shr[0][1] - 1 + 36
            left = shot.box_left(y + 1, 40, PANEL_X - 8)
            text = shot.text_left(y, y + 10, left, PANEL_X - 8) if left is not None else None
            offs.append(text - left if text is not None else None)
        check('shop: a row without an icon (Cloak) keeps the icon column empty', None not in offs and offs[0] - offs[1] == 11, offs)

    # ---- a greyed row dims its icon the way it dims its text (25 gold: the potion at 20 can be bought, the 40 gold sword cannot)
    pp = shoot('poor', 'shop')
    pshr, pshg = pp.blocks(RED, 0, PANEL_X), pp.blocks(GREEN, 0, PANEL_X)
    dim_red = pp.blocks((104, 20, 25), 0, PANEL_X, tol=4)  # (255 x 120/255, 124/255, 148/255 of the red)
    check('shop: the icon of a row that cannot be bought is dimmed, the others are not', not pshr and len(pshg) == 1 and len(dim_red) == 1, (pshr, pshg, dim_red))

    # ---- the unit details' item page and the forecast
    de = shoot('icons', 'details')
    dr, dg, db = (de.blocks(c, 0, PANEL_X + 120) for c in (RED, GREEN, BLUE))
    check('unit details: the items page shows the three icons in one column', len(dr) >= 1 and len(dg) >= 1 and len(db) >= 1 and dr[0][0] == dg[0][0] == db[0][0], (dr, dg, db))
    fc = shoot('icons', 'forecast')
    fr = fc.blocks(RED, 0, PANEL_X)
    check('forecast: both fighters\' weapon lines carry the sword icon', len(fr) == 2 and fr[0][1] == fr[1][1], fr)

    # ---- icons taller than the text row make the rows taller; the cell size is the sheet's
    tall = shoot('tall', 'items')
    tr_, tg_, tb_ = (tall.blocks(c, 0, PANEL_X) for c in (RED, GREEN, BLUE))
    check('16 x 16 icons: drawn at their cell size (12 x 12 colour inside a 16 x 16 cell)', len(tr_) == 1 and tr_[0][2] - tr_[0][0] == 11 and tr_[0][3] - tr_[0][1] == 11, tr_)
    check('16 x 16 icons: the rows are 16 pixels tall (an 8 pixel icon did not change the 12)',
          len(tr_) == len(tb_) == len(tg_) == 1 and tb_[0][1] - tr_[0][1] == 16 and tg_[0][1] - tb_[0][1] == 16, (tr_, tb_, tg_))

    # ---- a game without "icons" is the same picture with an empty "icons" map; with --baseline, the same as the other build's
    make_game('empty')
    edit(os.path.join(TMP, 'empty'), 'data/graphics.json', lambda gj: gj.update({'icons': {}}))
    for which in ('items', 'weapons', 'trade', 'shop', 'details', 'forecast'):
        a, b2 = shoot('plain', which), shoot('empty', which)
        check('no icons: an empty "icons" map and none draw the same %s screen' % which, a.px == b2.px)
    if BASELINE:  # the same fixture without "icons", played by the other build: every screen byte for byte alike
        for which in SCREENS:
            shoot('plain', which)
            other = os.path.join(TMP, 'baseline_' + which + '.bmp')
            script = inp(SCREENS[which])
            run_text([BASELINE, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--no-saving',
                      '--game', 'plain', '--map', 'field', '--battle-scene', 'map', '--fixed-dt', '0.016', '--input-interval', '4',
                      '--frames', str(ticks(script) * 4 + 100), '--input', script, '--screenshot', other], timeout=120)
            check('baseline build: a game without icons draws the %s screen byte for byte alike' % which,
                  open(other, 'rb').read() == open(os.path.join(TMP, 'plain_' + which + '.bmp'), 'rb').read())

    if not failures:
        shutil.rmtree(TMP, ignore_errors=True)
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
