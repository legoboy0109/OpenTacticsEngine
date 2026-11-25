#!/usr/bin/env python3
"""Map art for prerendered isometric games through the real player, windowless (docs/VIEWS.md "Backdrop", docs/GRAPHICS.md
"Character looks", "Poses and facing", "Unit offset", "Portraits").

  python3 tests/iso_map_art_player_test.py ./build/engine/player/ote-player .
  python3 tests/iso_map_art_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR     # keep the screenshots (BMP)

Copies tests/fixtures/srpg-quotes into a temp folder and gives it original art made here with tools/art/battle_art.py (flat colours, so a
screenshot shows which picture was drawn), then runs the player headless (no audio, no window, no settings):

  - backdrop: the 2D isometric style draws the map's picture (its sky shows round the squares, no generated block), a unit standing
    behind a piece of the over picture is hidden by it (only shown through, faintly), a unit in front of a piece stands over it; the
    top-down style of the same game draws no backdrop;
  - character look: a character is drawn with its own picture, a unit of the same type without a character with the type's;
  - unit offset: a sheet's "unitOffset" [0, 4] moves the picture 4 pixels down (1 art pixel = 1 screen pixel here), nothing else;
  - facing: after a move whose last step goes up the map (up-right on the isometric screen) the unit stands with its back frames;
  - attack / hurt / KO: the attacker shows its attack frames during the strike, the target its hurt frames, then KO when it falls;
  - portraits: "portraitSize": 64 draws a 64 x 64 face on a 64 x 64 plate; without it the plate is 48 x 48 (as always).
"""
import os
import shutil
import struct
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, failures, player_and_repo, player_cmd, read_jsonc, shots_dir, summary, write_json)
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

# Flat colours, each used by exactly one picture.
SKY = (40, 160, 200, 255)       # the backdrop round the squares
GROUND = (120, 180, 90, 255)    # the backdrop's squares
WALL = (150, 110, 70, 255)      # the backdrop's lower walls
MARK = (255, 0, 255, 255)       # 4 x 4 at the backdrop's top-left corner: where the picture is on the screen
TREE = (20, 90, 40, 255)        # the over picture's pieces
STAND = (200, 60, 200, 255)     # the hero type's standing shirt
CHAR = (60, 220, 220, 255)      # Eirik's own look
FRONT, BACK = (70, 150, 230, 255), (230, 190, 60, 255)   # Eirik standing per facing: seen from the front / from behind
ATK, HURT, KO = (240, 120, 30, 255), (220, 40, 40, 255), (90, 90, 90, 255)
ENEMY = (160, 160, 60, 255)     # the brute and grunt
PORT = (250, 230, 200, 255)     # the portrait

ISO = {'tileWidth': 32, 'tileHeight': 16, 'heightStep': 8, 'unitWidth': 16}
ORIGIN = (80, 24)               # the backdrop's pixel under the top corner of square 0,0


def read_bmp(path):
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    rows = [data[offset + r * stride: offset + r * stride + width * bpp] for r in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows, bpp


def pixels_of(path, rgb, box=None):
    """(count, min x, min y, max x, max y) of the pixels of exactly this colour (inside box = (x0, y0, x1, y1) when given)."""
    w, h, rows, bpp = read_bmp(path)
    want = bytes((rgb[2], rgb[1], rgb[0]))  # BMP stores blue, green, red
    n, x0, y0, x1, y1 = 0, 10 ** 9, 10 ** 9, -1, -1
    for y, row in enumerate(rows):
        if box and not box[1] <= y <= box[3]:
            continue
        for x in range(w):
            if box and not box[0] <= x <= box[2]:
                continue
            if row[x * bpp:x * bpp + 3] == want:
                n += 1
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    return n, x0, y0, x1, y1


def spent(rgb):
    """A colour as a unit that has acted is drawn (multiplied by 140 / 255; the rules have already spent the attacker while its strike plays)."""
    return tuple(c * 140 // 255 for c in rgb[:3])


def pixels_near(path, rgb, tol=2):
    """How many pixels are within `tol` of this colour in each channel."""
    w, h, rows, bpp = read_bmp(path)
    n = 0
    for row in rows:
        for x in range(w):
            b, g, r = row[x * bpp:x * bpp + 3]
            if abs(r - rgb[0]) <= tol and abs(g - rgb[1]) <= tol and abs(b - rgb[2]) <= tol:
                n += 1
    return n


def figure(shirt):
    fig = battle_art.Canvas(16, 16)
    fig.rect(5, 6, 10, 11, shirt)
    fig.rect(5, 1, 10, 5, battle_art.SKIN)
    fig.rect(5, 12, 10, 14, (60, 60, 90, 255))
    fig.outline()
    return fig


def units_sheet():
    """units.png, 16 x 16 cells in a row: hero, Eirik, front, back, attack, hurt, KO, enemy."""
    shirts = [STAND, CHAR, FRONT, BACK, ATK, HURT, KO, ENEMY]
    sheet = battle_art.Canvas(16 * len(shirts), 16)
    for i, s in enumerate(shirts):
        sheet.paste(figure(s), i * 16, 0)
    return sheet


def to_grid(px, py):
    u, v = (px - ORIGIN[0]) / 16.0, (py - ORIGIN[1]) / 8.0
    return (u + v) / 2, (v - u) / 2


def backdrop(gw, gh):
    """The ground picture: sky, the diamond of the map's squares, one level of wall under its two near edges."""
    pic = battle_art.Canvas(176, 128)
    for y in range(pic.h):
        for x in range(pic.w):
            gx, gy = to_grid(x + 0.5, y + 0.5)
            wx, wy = to_grid(x + 0.5, y + 0.5 - 8)  # the square whose top this pixel is one level under
            if 0 <= gx < gw and 0 <= gy < gh:
                pic.set(x, y, GROUND)
            elif 0 <= wx < gw and 0 <= wy < gh:
                pic.set(x, y, WALL)
            else:
                pic.set(x, y, SKY)
    pic.rect(0, 0, 3, 3, MARK)
    return pic


def feet(gx, gy):
    """A unit's feet on square (gx, gy), in the backdrop's pixels (flat map): the middle of the square plus a quarter diamond."""
    return ORIGIN[0] + (gx - gy) * 16, ORIGIN[1] + (gx + gy + 1) * 8 + 4


def figure_box(gx, gy):
    fx, fy = feet(gx, gy)
    return fx - 8, fy - 16, fx + 7, fy - 1  # 16 x 16, standing on the feet


# Pieces of the over picture: one standing on square 2,2 over the hero at 1,1 (it is in front of the hero: drawn after it), one
# standing on square 2,1 over Eirik at 3,2 (behind Eirik: drawn before it).
HIDE_BOX = (68, 30, 24, 26)
BEHIND_BOX = (84, 56, 24, 22)


def over_picture():
    pic = battle_art.Canvas(176, 128)
    for (x, y, w, h) in (HIDE_BOX, BEHIND_BOX):
        pic.rect(x, y, x + w - 1, y + h - 1, TREE)
    return pic


def make_game(gid, mapdef, graphics_extra=None, iso_first=True, offset=None, portrait_size=None, units_extra=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.remove(os.path.join(g, 'data', 'campaign.json'))  # a single battle
    os.makedirs(os.path.join(g, 'art'))
    battle_art.write_png(os.path.join(g, 'art', 'units.png'), units_sheet())
    battle_art.write_png(os.path.join(g, 'art', 'ground.png'), backdrop(5, 4))
    battle_art.write_png(os.path.join(g, 'art', 'over.png'), over_picture())
    face = battle_art.Canvas(64, 64)
    face.rect(0, 0, 63, 63, PORT)
    battle_art.write_png(os.path.join(g, 'art', 'faces.png'), face)
    gj = read_jsonc(os.path.join(g, 'game.json'))
    styles = [{'id': 'iso', 'name': 'Isometric', 'view': 'isometric', 'isometric': ISO}, {'id': 'classic', 'name': 'Classic'}]
    gj['styles'] = styles if iso_first else styles[::-1]
    write_json(os.path.join(g, 'game.json'), gj)
    if units_extra:
        units = read_jsonc(os.path.join(g, 'data/units.json'))
        for u in units['units']:
            u.update(units_extra.get(u['id'], {}))
        write_json(os.path.join(g, 'data/units.json'), units)
    write_json(os.path.join(g, 'maps/ch1.json'), mapdef)
    sheet = {'id': 'units', 'file': 'art/units.png', 'cellWidth': 16, 'cellHeight': 16}
    if offset:
        sheet['unitOffset'] = offset
    look = {'hero': {'sheet': 'units', 'cell': [0, 0]}, 'brute': {'sheet': 'units', 'cell': [7, 0]},
            'grunt': {'sheet': 'units', 'cell': [7, 0]}, 'rider': {'sheet': 'units', 'cell': [7, 0]}}
    chars = {'eirik': {'sheet': 'units', 'cell': [1, 0]}}
    gfx = {'sheets': [sheet, {'id': 'ground', 'file': 'art/ground.png', 'cellWidth': 176, 'cellHeight': 128},
                      {'id': 'over', 'file': 'art/over.png', 'cellWidth': 176, 'cellHeight': 128},
                      {'id': 'faces', 'file': 'art/faces.png', 'cellWidth': 64, 'cellHeight': 64}],
           'unitMarker': False, 'units': look, 'characters': chars, 'portraits': {'eirik': {'sheet': 'faces', 'cell': [0, 0]}},
           'styles': {'iso': {'units': look, 'characters': chars}}}
    if portrait_size:
        gfx['portraitSize'] = portrait_size
    for k, v in (graphics_extra or {}).items():
        gfx[k] = v
    write_json(os.path.join(g, 'data/graphics.json'), gfx)
    return g


def field_map(**extra):
    m = {'name': 'Field', 'grid': {'type': 'square', 'width': 5, 'height': 4}, 'legend': {'.': 'plain'},
         'tiles': ['.....', '.....', '.....', '.....'],
         'sides': [{'id': 'blue', 'controller': 'human'}, {'id': 'red', 'controller': 'ai'}],
         'units': [{'character': 'eirik', 'side': 'blue', 'x': 3, 'y': 2, 'leader': True},
                   {'type': 'hero', 'side': 'blue', 'x': 1, 'y': 1},
                   {'character': 'gorm', 'side': 'red', 'x': 4, 'y': 0, 'leader': True}],
         'backdrop': {'sheet': 'ground', 'origin': list(ORIGIN), 'over': 'over',
                      'pieces': [{'rect': list(HIDE_BOX), 'square': [2, 2]}, {'rect': list(BEHIND_BOX), 'square': [2, 1]}]}}
    m.update(extra)
    return m


def run(gid, inp=None, frames=120, shot=None, interval=4, style=None, autoplay=False, debug=False):
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(frames), '--fixed-dt', '0.016', '--game', gid, '--map', 'ch1']
    if inp:
        cmd += ['--input-interval', str(interval), '--input', ','.join(inp)]
    if autoplay:
        cmd += ['--autoplay']
    if style:
        cmd += ['--style', style]
    if shot:
        cmd += ['--screenshot', shot]
    env = dict(os.environ, OTE_LOG_LEVEL='debug') if debug else None
    return run_text(cmd, env=env).stdout


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def shot_of(gid, name, **kw):
    path = os.path.join(TMP, name + '.bmp')
    log = run(gid, shot=path, **kw)
    keep(path, name)
    return path, log


def corner(path):
    n, x0, y0, _, _ = pixels_of(path, MARK)
    return (x0, y0) if n == 16 else None


def test_backdrop_and_character_look():
    make_game('bd', field_map())
    path, log = shot_of('bd', 'iso-backdrop')
    c = corner(path)
    check('backdrop: the picture is drawn (its corner mark is on the screen)', c is not None, log[-400:])
    if c is None:
        return
    check('backdrop: its sky shows round the squares and its ground under them', pixels_of(path, SKY)[0] > 2000 and pixels_of(path, GROUND)[0] > 1500,
          'sky %d ground %d' % (pixels_of(path, SKY)[0], pixels_of(path, GROUND)[0]))
    # Where the over picture's pieces and the units' figures are on the screen: the picture's corner + their place in it.
    def box(b):
        return (c[0] + b[0], c[1] + b[1], c[0] + b[2], c[1] + b[3])
    hero, eirik = box(figure_box(1, 1)), box(figure_box(3, 2))
    check('over picture: the hero behind the tree piece is hidden (not drawn in its own colours over the piece)',
          pixels_of(path, STAND)[0] == 0 and pixels_of(path, TREE, hero)[0] > 100,
          'hero shirt %d, tree in its box %d' % (pixels_of(path, STAND)[0], pixels_of(path, TREE, hero)[0]))
    n = pixels_of(path, CHAR, eirik)[0]
    check('over picture + character look: Eirik stands in front of the piece behind him, in his own look (6 x 6 shirt)', n == 36, 'Eirik shirt %d' % n)
    path2, _ = shot_of('bd', 'topdown-no-backdrop', style='classic')
    check('top-down style: no backdrop (it is only for the 2D isometric kind)', pixels_of(path2, SKY)[0] == 0 and pixels_of(path2, MARK)[0] == 0)
    check('top-down: the character look is drawn there too, the plain hero with its type\'s', pixels_of(path2, CHAR)[0] > 20 and pixels_of(path2, STAND)[0] > 20)


def test_unit_offset():
    plain = {k: v for k, v in field_map().items() if k != 'backdrop'}  # (generated blocks: the offset is the only difference)
    make_game('off0', plain)
    make_game('off4', plain, offset=[0, 4])
    p0, _ = shot_of('off0', 'iso-offset-0')
    p4, _ = shot_of('off4', 'iso-offset-4')
    a, b = pixels_of(p0, CHAR), pixels_of(p4, CHAR)
    check('unit offset [0, 4]: the picture is 4 pixels lower, same size and column', a[0] == b[0] == 36 and b[2] - a[2] == 4 and b[1] == a[1],
          'without %s, with %s' % (a, b))


# The walk test's move: a U-shaped field whose way from 0,0 to 2,0 is down, down, right, right, up, up (the last step goes up the map:
# up-right on the isometric screen, so Eirik ends with his back to the viewer).
MOVE = ['wait:14', 'confirm', 'wait:3', 'right', 'right', 'confirm']


def facing_map():
    return {'name': 'Field', 'grid': {'type': 'square', 'width': 5, 'height': 3}, 'legend': {'.': 'plain', '#': 'wall'},
            'tiles': ['.#...', '.#...', '.....'],
            'sides': [{'id': 'blue', 'controller': 'human'}, {'id': 'red', 'controller': 'ai'}],
            'units': [{'character': 'eirik', 'side': 'blue', 'x': 0, 'y': 0, 'leader': True},
                      {'character': 'gorm', 'side': 'red', 'x': 4, 'y': 2, 'leader': True}]}


def poses(log):
    return [l.split('pose: ', 1)[1] for l in log.splitlines() if '[debug] pose: ' in l]


def test_facing():
    eirik = {'sheet': 'units', 'cell': [1, 0], 'facing': {'downLeft': [[2, 0]], 'upLeft': [[3, 0]]}}
    make_game('face', facing_map(), graphics_extra={'characters': {'eirik': eirik}, 'styles': {'iso': {'characters': {'eirik': eirik},
              'units': {'brute': {'sheet': 'units', 'cell': [7, 0]}}}}}, units_extra={'hero': {'stats': {'hp': 20, 'str': 6, 'def': 3, 'spd': 6, 'mov': 8}}})
    path, log = shot_of('face', 'iso-facing-before', debug=True)
    check('facing: before any move, the look\'s own picture (no facing known)', pixels_of(path, CHAR)[0] == 36 and not poses(log), str(poses(log)))
    path, log = shot_of('face', 'iso-facing-back', inp=MOVE, frames=260, debug=True)
    got = poses(log)
    check('facing: the unit turns with each step and stands with its back frames after a last step up the map (up-right on screen)',
          got[-1:] == ['Eirik stand upRight'] and pixels_of(path, BACK)[0] == 36 and pixels_of(path, CHAR)[0] == 0, str(got) + log[-300:])
    check('facing: on the way down the map it showed its front (down-left)', 'Eirik stand downLeft' in got, str(got))
    path, log = shot_of('face', 'topdown-facing-back', inp=MOVE, frames=260, debug=True, style='classic')
    check('facing, top-down: the last step up picks the nearest given facing (up-left, the back frames)',
          poses(log)[-1:] == ['Eirik stand upLeft'] and pixels_of(path, BACK)[0] > 20, str(poses(log)))


def test_attack_hurt_ko():
    eirik = {'sheet': 'units', 'cell': [1, 0], 'attack': {'downLeft': [[4, 0]], 'frameMs': 100}}
    gorm = {'sheet': 'units', 'cell': [7, 0], 'hurt': {'down': [[5, 0]]}, 'ko': {'down': [[6, 0]]}}
    m = {'name': 'Field', 'grid': {'type': 'square', 'width': 3, 'height': 1}, 'legend': {'.': 'plain'}, 'tiles': ['...'],
         'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}],
         'units': [{'character': 'eirik', 'side': 'blue', 'x': 0, 'y': 0, 'leader': True},
                   {'character': 'gorm', 'side': 'red', 'x': 1, 'y': 0, 'leader': True}]}
    chars = {'eirik': eirik, 'gorm': gorm}
    make_game('atk', m, graphics_extra={'characters': chars, 'styles': {'iso': {'characters': chars}}},
              units_extra={'hero': {'stats': {'hp': 20, 'str': 60, 'def': 3, 'spd': 6, 'mov': 4}}})
    log = run('atk', frames=400, autoplay=True, debug=True)
    got = poses(log)
    check('attack: the attacker shows its attack frames, facing its target (down-right on the isometric screen: the given down-left turned round)',
          'Eirik attack downRight' in got, str(got) + log[-400:])
    check('hurt, then KO: the target shows its hurt frames when the blow lands, its KO frames as it falls', any(p.startswith('Gorm hurt') for p in got) and
          any(p.startswith('Gorm ko') for p in got) and [p.split()[1] for p in got if p.startswith('Gorm')].index('hurt') <
          [p.split()[1] for p in got if p.startswith('Gorm')].index('ko'), str(got))
    # A screenshot in the middle of the strike: the first frame count whose log ends in the attack line.
    for frames in range(20, 200, 2):
        if poses(run('atk', frames=frames, autoplay=True, debug=True))[-1:] == ['Eirik attack downRight']:
            path, _ = shot_of('atk', 'iso-attack', frames=frames + 4, autoplay=True)
            n = pixels_of(path, ATK)[0] + pixels_near(path, spent(ATK))
            check('attack: the attack frame is on the screen mid-strike', n == 36, 'attack shirt pixels %d' % n)
            break
    else:
        check('attack: found a moment in the middle of the strike', False)


def test_portrait_size():
    m = dict(field_map(), events=[{'id': 'hello', 'when': {'battle': 'start'}, 'do': [{'say': 'The snow is deep.', 'speaker': 'eirik'}]}])
    make_game('pt64', m, portrait_size=64)
    make_game('pt48', m)
    p64, log = shot_of('pt64', 'dialogue-portrait-64', frames=40)
    p48, _ = shot_of('pt48', 'dialogue-portrait-48', frames=40)
    a, b = pixels_of(p64, PORT), pixels_of(p48, PORT)
    # (the plate's 1-pixel frame is drawn over the picture's outer pixels: 62 x 62 of the face show inside a 64 x 64 plate)
    check('portraitSize 64: the 64 x 64 face is drawn whole on a 64 x 64 plate', (a[3] - a[1] + 1, a[4] - a[2] + 1) == (62, 62) and a[0] == 62 * 62,
          '%s %s' % (a, log[-300:]))
    check('no portraitSize: the plate stays 48 x 48 (the face shrunk into it)', (b[3] - b[1] + 1, b[4] - b[2] + 1) == (46, 46), str(b))


try:
    test_backdrop_and_character_look()
    test_unit_offset()
    test_facing()
    test_attack_hurt_ko()
    test_portrait_size()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
