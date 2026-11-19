#!/usr/bin/env python3
"""The player's menu options through the real player, windowless (docs/SRPG.md "Menu options").

  python3 tests/menu_options_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Three things, each with scripted input and the [info] lines the player logs:

  1. Unit pictures in the preparation screen's lists: the Units list (and the "Whose items?" list) at 480x270, 270x480 and a
     270x480 phone with a big UI scale: each row shows the unit's map picture (the standing picture of its type; a type with no
     picture keeps the map's placeholder block), its name, its level and class. Pixels of the test art are counted.
  2. game.json "menuUnavailable": a unit with nothing to heal and nothing in reach: with "grey" (and without the key) the Mend
     ability and the Staff entry are in the menu greyed; with "hide" they are gone; "Lunge" (before moving only) stays greyed
     with "hide" once the unit has moved, because that is not a missing target. The player's own setting (a temporary settings
     file) beats the game both ways. Keyboard, mouse and touch choose the first row; the cursor never lands on a hidden one.
  3. game.json "attackWeaponChoice": "pick": Attack opens the weapon list (rows, uses, stats, the weapon in hand preselected);
     the second weapon reaches another enemy; Back from the targets returns to the list; confirming logs the Equip order and then
     the Attack with that weapon, and the Continue save shows the weapon in front; Cancel everywhere leaves the equipped weapon;
     a tap on an enemy starts on the weapon that reaches it; mouse / touch pick; the one-weapon case still shows the list; a game
     without the key goes straight to the targets.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, failures, player_and_repo, shots_dir, summary, ticks, write_json  # noqa: E402

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hex_player_test as hp  # noqa: E402  (its image reader, PNG writer and checker)

check, Image, png_bytes = hp.check, hp.Image, hp.png_bytes
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = tempfile.mkdtemp(prefix='ote-menuopt-')
GAMES = os.path.join(TMP, 'games')
TILE, MAP_W, MAP_H, VIEW_W, VIEW_H = 24, 9, 7, 352, 259
IDLE = 'wait:14'
SELECT = 'confirm,confirm'  # select the unit under the cursor, stay: its action menu opens


def read_json(path):
    return json.load(open(path))


def play(game, inp, map_id=None, shot=None, saves=None, settings=None, extra=(), interval=4):
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * interval + 80), '--input-interval', str(interval), '--fixed-dt', '0.016',
           '--game', game, '--input', inp]
    if map_id:
        cmd += ['--map', map_id]
    cmd += ['--settings', settings] if settings else ['--no-settings']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(extra))
    return p.stdout


def keep(name, path):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def run(name, game, inp, map_id=None, saves=False, **kw):
    """Returns (log, image, continue-save or None)."""
    path = os.path.join(TMP, name + '.bmp')
    sdir = os.path.join(TMP, 'saves_' + name) if saves else None
    if sdir:
        shutil.rmtree(sdir, ignore_errors=True)
    log = play(game, inp, map_id, shot=path, saves=sdir, **kw)
    keep(name, path)
    save = None
    if sdir and os.path.exists(os.path.join(sdir, game, 'continue.json')):
        save = read_json(os.path.join(sdir, game, 'continue.json'))
    return log, (Image(path) if os.path.exists(path) else None), save


def lines(log, key):
    return [l.split('] ', 1)[-1] for l in log.splitlines() if key in l]


def last(log, key):
    found = lines(log, key)
    return found[-1] if found else ''


def clean(log, what='UI bug'):
    return what not in log and '[warn]' not in log and '[error]' not in log


# ---- 1. unit pictures in the preparation lists ---------------------------------------------------------------------

SOLDIER, ARCHER = (230, 40, 180), (40, 200, 200)  # the body colours of the two test pictures (no UI colour is either)


def figure(body, head=(250, 220, 160)):
    """A small original standing figure, 24 x 24: a round head, a body, two legs; the rest see-through."""
    def pixel(x, y):
        if (x - 12) ** 2 + (y - 7) ** 2 <= 20:
            return head + (255,)
        if 7 <= x <= 16 and 12 <= y <= 19:
            return body + (255,)
        if (7 <= x <= 10 or 13 <= x <= 16) and 20 <= y <= 23:
            return (60, 50, 40, 255)
        return (0, 0, 0, 0)
    return pixel


def men_sheet():
    pixels = [figure(SOLDIER), figure(SOLDIER, (255, 200, 140)), figure(ARCHER)]  # soldier (2 frames), archer
    return png_bytes(72, 24, lambda x, y: pixels[x // 24](x % 24, y))


def make_prep_game(gid, width, height):
    g = os.path.join(GAMES, gid)
    shutil.rmtree(g, ignore_errors=True)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    os.makedirs(g + '/art')
    open(g + '/art/men.png', 'wb').write(men_sheet())
    # soldier and archer have pictures (the soldier idles on two frames); glider and raider keep the placeholder block
    write_json(g + '/data/graphics.json', {
        'sheets': [{'id': 'men', 'file': 'art/men.png', 'cellWidth': 24, 'cellHeight': 24}],
        'units': {'soldier': {'sheet': 'men', 'frames': [[0, 0], [1, 0]], 'frameMs': 400}, 'archer': {'sheet': 'men', 'cell': [2, 0]}}})
    return g


TO_PREP = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40'


def count_colour(img, rgb, tol=0):
    n = 0
    for y in range(img.h):
        for x in range(img.w):
            p = img.px(x, y)
            if p and max(abs(p[0] - rgb[0]), abs(p[1] - rgb[1]), abs(p[2] - rgb[2])) <= tol:
                n += 1
    return n


def prep_lists():
    for tag, (w, h), extra in (('480x270', (480, 270), ()), ('270x480', (270, 480), ()), ('270x480_big', (270, 480), ('--ui-scale', '150'))):
        gid = 'prep_' + tag
        make_prep_game(gid, w, h)
        log, img, _ = run('prep_units_' + tag, gid, TO_PREP + ',confirm,wait:6', extra=('--campaign',) + extra)
        names = last(log, 'prep units:')
        check('prep %s: the Units list opens with every character' % tag, 'Sera' in names and 'Tomas' in names and 'Wren' in names and 'Bram' in names, names)
        check('prep %s: no warnings' % tag, clean(log), log[-300:])
        check('prep %s: the soldier\'s picture is drawn in the list' % tag, img is not None and count_colour(img, SOLDIER) > 20,
              count_colour(img, SOLDIER) if img else 'no picture')
        check('prep %s: the archer\'s picture is drawn in the list' % tag, img is not None and count_colour(img, ARCHER) > 8,
              count_colour(img, ARCHER) if img else 'no picture')
        # the Items owner list (the same rows)
        log, img, _ = run('prep_items_' + tag, gid, TO_PREP + ',down,down,confirm,wait:6', extra=('--campaign',) + extra)
        check('prep %s: the "Whose items?" list shows the pictures too' % tag,
              'prep items:' in log and img is not None and count_colour(img, SOLDIER) > 20, last(log, 'prep items:'))


# ---- the battle game -----------------------------------------------------------------------------------------------

def make_battle_game(gid, hide=None, pick=None, width=480, height=270):
    g = os.path.join(GAMES, gid)
    shutil.rmtree(g, ignore_errors=True)
    game = {'formatVersion': 1, 'name': 'Menu Options ' + gid, 'view': {'width': width, 'height': height, 'tileSize': TILE},
            'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Atk'}, {'id': 'def', 'name': 'Def'}, {'id': 'mov', 'name': 'Move'}],
            'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
            'rules': {'defeat': ['noUnits'], 'counterAttacks': True, 'items': {'weapons': 'carried', 'inventorySize': 5}},
            'formulas': {'damage': 'a.atk + w.might - b.def', 'hit': '90'},
            'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in ('pick', 'single', 'nothing')]}
    if hide:
        game['menuUnavailable'] = hide
    if pick:
        game['attackWeaponChoice'] = pick
    write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'}]})
    stats = lambda hp_, mv: {'hp': hp_, 'atk': 3, 'def': 1, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'hero', 'name': 'Hero', 'moveType': 'foot', 'stats': stats(20, 4), 'wields': ['sword', 'bow', 'staff'],
         'abilities': ['mend', 'lunge']},
        {'id': 'pal', 'name': 'Pal', 'moveType': 'foot', 'stats': stats(20, 4)},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(20, 3)}]})
    write_json(g + '/data/weapons.json', {'weapons': [
        {'id': 'sword', 'name': 'Sword', 'group': 'sword', 'props': {'might': 5}},
        {'id': 'bow', 'name': 'Bow', 'group': 'bow', 'range': [2, 2], 'props': {'might': 4}}]})
    write_json(g + '/data/items.json', {'items': [
        {'id': 'iron_sword', 'name': 'Iron Sword', 'kind': 'weapon', 'weapon': 'sword', 'uses': 30},
        {'id': 'long_bow', 'name': 'Long Bow', 'kind': 'weapon', 'weapon': 'bow', 'uses': 20},
        {'id': 'mend_staff', 'name': 'Mend Staff', 'kind': 'staff', 'group': 'staff', 'uses': 6, 'range': 2, 'target': 'ally',
         'effects': [{'heal': 5}]}]})
    write_json(g + '/data/abilities.json', {'abilities': [
        {'id': 'mend', 'name': 'Mend', 'target': 'ally', 'range': [1, 2], 'effect': {'heal': 10}},
        {'id': 'lunge', 'name': 'Lunge', 'target': 'enemy', 'range': [1, 1], 'effect': {'push': 1}, 'afterMove': False}]})

    def make_map(units):
        return {'name': 'Map', 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'legend': {'.': 'plain'},
                'tiles': ['.' * MAP_W] * MAP_H,
                'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#3d85c6', 'controller': 'human'},
                          {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai'}], 'units': units}
    hero = lambda items: {'id': 'hero', 'type': 'hero', 'side': 'blue', 'x': 2, 'y': 2, 'items': items}
    grunt = lambda x, y: {'type': 'grunt', 'side': 'red', 'x': x, 'y': y}
    # pick: the sword reaches the grunt at 3,2; the bow the one at 4,2 (two squares away)
    write_json(g + '/maps/pick.json', make_map([hero(['iron_sword', 'long_bow']), grunt(3, 2), grunt(4, 2)]))
    write_json(g + '/maps/single.json', make_map([hero(['iron_sword']), grunt(3, 2)]))
    # nothing: nobody hurt, nobody in reach
    write_json(g + '/maps/nothing.json', make_map([hero(['iron_sword', 'mend_staff']), {'id': 'pal', 'type': 'pal', 'side': 'blue', 'x': 4, 'y': 2}, grunt(8, 6)]))
    return g


def tile_centre(x, y):
    """The middle of a map square on the screen (a 9 x 7 map centred in the 352 x 259 viewport)."""
    import math
    ox = int(math.floor((VIEW_W - MAP_W * TILE) / 2.0 + 0.5))
    oy = int(math.floor((VIEW_H - MAP_H * TILE) / 2.0 + 0.5))
    return ox + x * TILE + TILE // 2, oy + y * TILE + TILE // 2


def menu_labels(log):
    """The labels of the last action menu: [(label, greyed)]."""
    text = last(log, 'menu entries:')
    if not text:
        return []
    out = []
    for part in text.split('menu entries: ', 1)[-1].split(', '):
        part = part.strip()
        out.append((part.replace(' [grey]', ''), part.endswith('[grey]')))
    return out


def row_points(log, key):
    return {int(i): (int(x), int(y)) for i, x, y in re.findall(r' (\d+)@(\d+),(\d+)', last(log, key))}


# ---- 2. hide ---------------------------------------------------------------------------------------------------------

def hide_tests():
    plain = make_battle_game('mo_plain')
    grey = make_battle_game('mo_grey', hide='grey')
    hide = make_battle_game('mo_hide', hide='hide')
    del plain, grey, hide
    first = IDLE + ',' + SELECT
    for gid in ('mo_plain', 'mo_grey'):
        log, img, _ = run(gid + '_menu', gid, first, 'nothing')
        labels = menu_labels(log)
        names = [n for n, _ in labels]
        check('%s: Mend and Staff are in the menu, greyed (today)' % gid, ('Mend', True) in labels and ('Staff', True) in labels, labels)
        check('%s: Items, Info and Wait follow, usable' % gid, names[-3:] == ['Items', 'Info', 'Wait'] and all(not g for n, g in labels[-3:]), labels)
        check('%s: no Attack (nothing in reach), as before' % gid, 'Attack' not in names, labels)
    log, img, _ = run('mo_hide_menu', 'mo_hide', first, 'nothing')
    labels = menu_labels(log)
    names = [n for n, _ in labels]
    check('hide: Mend, Lunge and Staff are left out; Items, Info and Wait remain', names == ['Items', 'Info', 'Wait'], labels)
    check('hide: no row is greyed', all(not g for _, g in labels), labels)
    check('hide: no warnings', clean(log), log[-300:])
    # the cursor starts on the first (usable) row, and keyboard Down walks only the three that are there
    log, _, _ = run('mo_hide_kb', 'mo_hide', first + ',confirm,wait:3', 'nothing')
    check('hide: Confirm on the first row opens Items (the cursor is not on a hidden row)', 'items: Iron Sword, Mend Staff' in log, log[-300:])
    log, _, _ = run('mo_hide_down', 'mo_hide', first + ',down,down,down,confirm,wait:6', 'nothing')  # Items, Info, Wait, wraps to Items
    check('hide: Down wraps over the three rows', 'items: Iron Sword, Mend Staff' in log, log[-300:])
    log, _, _ = run('mo_hide_up', 'mo_hide', first + ',up,confirm,wait:30', 'nothing', saves=True)  # Up from Items wraps to Wait
    check('hide: Up from the first row reaches Wait (the last row)', 'items:' not in log and 'UI bug' not in log, log[-300:])
    # mouse and touch: tap the middle of rows from the log
    log, _, _ = run('mo_hide_rows', 'mo_hide', first, 'nothing')
    rows = row_points(log, 'menu entry rows:')
    check('hide: the menu logs three row positions', len(rows) == 3, rows)
    if len(rows) == 3:
        x, y = rows[1]
        log2, _, _ = run('mo_hide_tap', 'mo_hide', first + ',tap:%d:%d,wait:3,tap:%d:%d,wait:6' % (x, y, x, y), 'nothing')
        check('hide: tapping the Info row (the second) selects, tapping again chooses it', 'unit details:' in log2, log2[-300:])
    # moved: "before moving" stays greyed with its reason in both modes; the missing-target ones are gone under hide
    moved = IDLE + ',confirm,right,confirm,wait:3'
    for gid in ('mo_grey', 'mo_hide'):
        log, img, _ = run(gid + '_moved', gid, moved, 'nothing')
        labels = menu_labels(log)
        check('%s: after moving, Lunge stays greyed (not a missing target)' % gid, ('Lunge', True) in labels, labels)
        if gid == 'mo_hide':
            check('hide: after moving, Mend and Staff are still left out', 'Mend' not in dict(labels) and 'Staff' not in dict(labels), labels)
    # the player's own choice beats the game, both ways (a temporary settings file)
    sf = os.path.join(TMP, 'settings.json')
    write_json(sf, {'version': 1, 'global': {'menuUnavailable': 'hide'}})
    log, _, _ = run('mo_grey_player_hide', 'mo_grey', first, 'nothing', settings=sf)
    check('player: "Hidden" hides the entries in a game that greys them', [n for n, _ in menu_labels(log)] == ['Items', 'Info', 'Wait'], menu_labels(log))
    write_json(sf, {'version': 1, 'global': {'menuUnavailable': 'grey'}})
    log, _, _ = run('mo_hide_player_grey', 'mo_hide', first, 'nothing', settings=sf)
    check('player: "Greyed out" keeps them in a game that hides them', ('Mend', True) in menu_labels(log), menu_labels(log))
    write_json(sf, {'version': 1, 'global': {'menuUnavailable': 'grey'}, 'games': {'mo_hide': {'menuUnavailable': 'hide'}}})
    log, _, _ = run('mo_hide_player_game', 'mo_hide', first, 'nothing', settings=sf)
    check('player: a per-game choice beats the global one', [n for n, _ in menu_labels(log)] == ['Items', 'Info', 'Wait'], menu_labels(log))
    # phone size: the hidden menu still fits
    make_battle_game('mo_hide_phone', hide='hide', width=270, height=480)
    log, img, _ = run('mo_hide_phone', 'mo_hide_phone', first, 'nothing')
    check('hide: the phone-sized menu is the same three rows', [n for n, _ in menu_labels(log)] == ['Items', 'Info', 'Wait'], menu_labels(log))


# ---- 3. pick ---------------------------------------------------------------------------------------------------------

def weapon_list(log):
    """[(name, uses, equipped)] of the last weapon list."""
    text = last(log, 'weapon list:')
    out = []
    for part in re.findall(r'(?:^|, )([A-Za-z ]+?)(?: (\d+/\d+))?( \[equipped\])? \(', text.split('weapon list: ', 1)[-1]):
        out.append((part[0], part[1], bool(part[2])))
    return out


def pick_tests():
    make_battle_game('mo_pick', pick='pick')
    make_battle_game('mo_eq')  # no key: today
    atk = IDLE + ',' + SELECT
    log, img, _ = run('pick_list', 'mo_pick', atk + ',confirm,wait:4', 'pick')
    rows = weapon_list(log)
    check('pick: Attack opens the weapon list (name, uses, the weapon in hand marked)',
          rows == [('Iron Sword', '30/30', True), ('Long Bow', '20/20', False)], (rows, last(log, 'weapon list:')))
    check('pick: the list starts on the weapon in hand', 'sel=0' in last(log, 'weapon list rows:'), last(log, 'weapon list rows:'))
    check('pick: the stats line shows damage, hit and range', 'Dmg 7' in last(log, 'weapon list:') and 'Range 2' in last(log, 'weapon list:'), last(log, 'weapon list:'))
    check('pick: the list has no warnings', clean(log), log[-300:])
    # the second weapon: its targets are another enemy than the sword's
    log1, _, _ = run('pick_sword_targets', 'mo_pick', atk + ',confirm,confirm,wait:4', 'pick')
    log2, img2, _ = run('pick_bow_targets', 'mo_pick', atk + ',confirm,down,confirm,wait:4', 'pick')
    check('pick: the equipped sword reaches one enemy', 'weapon picked: Iron Sword (1 targets)' in log1, log1[-300:])
    check('pick: the second weapon (the bow) reaches one enemy', 'weapon picked: Long Bow (1 targets)' in log2, log2[-300:])
    check('pick: the forecast of the sword and of the bow differ', last(log1, 'forecast:') != last(log2, 'forecast:') and 'HP 20 > 13' in last(log1, 'forecast:')
          and 'HP 20 > 14' in last(log2, 'forecast:'), (last(log1, 'forecast:'), last(log2, 'forecast:')))
    # confirm: the Equip order first, then the Attack with that weapon; the save shows the bow in front
    log, _, save = run('pick_confirm', 'mo_pick', atk + ',confirm,down,confirm,confirm,wait:40', 'pick', saves=True)
    orders = [l for l in lines(log, 'order: ')]
    check('pick: confirming logs the Equip, then the Attack with the picked weapon',
          len(orders) >= 2 and orders[0].startswith('order: equip Hero slot 1') and orders[1].startswith('order: attack Hero -> unit 2 with Bow'), orders)
    units = save['battle']['units'] if save else []
    check('pick: the bow is in front of the sword afterwards (what equipping by hand does)',
          bool(units) and [i['item'] for i in units[0]['items']] == [1, 0] and units[0]['items'][0]['uses'] == 19,
          units[0]['items'] if units else save)
    check('pick: the grunt the bow reaches was hit, the other was not', bool(units) and units[2]['hp'] == 14 and units[1]['hp'] == 20, [u['hp'] for u in units])
    # the weapon already in hand: no Equip order
    log, _, save = run('pick_inhand', 'mo_pick', atk + ',confirm,confirm,confirm,wait:40', 'pick', saves=True)
    orders = lines(log, 'order: ')
    check('pick: the weapon in hand attacks without an Equip order', len(orders) == 1 and orders[0].startswith('order: attack Hero -> unit 1 with Sword'), orders)
    # Back: from the targets to the list, from the list to the action menu; the equipped weapon is untouched
    log, _, save = run('pick_cancel', 'mo_pick', atk + ',confirm,down,confirm,cancel,wait:3,cancel,wait:3,down,down,down,confirm,wait:30', 'pick', saves=True)
    check('pick: Back from the targets returns to the list (it logs the list again, on the bow)', len(lines(log, 'weapon list:')) == 2 and 'sel=1' in last(log, 'weapon list rows:'),
          lines(log, 'weapon list rows:'))
    check('pick: Back from the list returns to the action menu', len(lines(log, 'menu entries:')) == 2, lines(log, 'menu entries:'))
    check('pick: cancelling out equips nothing and attacks nothing', 'order: equip' not in log and 'order: attack' not in log, lines(log, 'order: '))
    units = save['battle']['units'] if save else []
    check('pick: after cancelling the equipped weapon is unchanged (the sword in front)', bool(units) and [i['item'] for i in units[0]['items']] == [0, 1],
          units[0]['items'] if units else save)
    # a tap on an enemy from the move overlay starts the list on a weapon that reaches it
    x, y = tile_centre(4, 2)
    ux, uy = tile_centre(2, 2)
    log, _, _ = run('pick_tap_enemy', 'mo_pick', IDLE + ',tap:%d:%d,wait:3,tap:%d:%d,wait:4' % (ux, uy, x, y), 'pick')
    check('pick: tapping the far enemy opens the list on the bow (the weapon that reaches it)', 'sel=1' in last(log, 'weapon list rows:'), last(log, 'weapon list rows:'))
    # touch / mouse: the Attack row, the second weapon, then the target
    log, _, _ = run('pick_touch_menu', 'mo_pick', atk, 'pick')
    menu = row_points(log, 'menu entry rows:')
    check('pick: the action menu logs its rows', 0 in menu, menu)
    if 0 in menu:
        mx, my = menu[0]
        inp = atk + ',tap:%d:%d,wait:3,tap:%d:%d,wait:4' % (mx, my, mx, my)  # a first tap selects, the second chooses Attack
        log, _, _ = run('pick_touch_list', 'mo_pick', inp, 'pick')
        wl = last(log, 'weapon list rows:')
        check('pick: a tap on Attack (twice: select, choose) opens the list', bool(wl), log[-300:])
        m = re.search(r'row0@(\d+),(\d+) step=(\d+)', wl)
        if m:
            rx, ry, step = int(m.group(1)), int(m.group(2)), int(m.group(3))
            inp2 = inp + ',tap:%d:%d,wait:3,tap:%d:%d,wait:4' % (rx, ry + step, rx, ry + step)  # the bow row: select, choose
            log, _, _ = run('pick_touch_bow', 'mo_pick', inp2, 'pick')
            check('pick: tapping the second row twice picks the bow', 'weapon picked: Long Bow' in log, log[-300:])
            tx, ty = tile_centre(4, 2)
            log, _, save = run('pick_touch_attack', 'mo_pick', inp2 + ',tap:%d:%d,wait:40' % (tx, ty), 'pick', saves=True)
            orders = lines(log, 'order: ')
            check('pick: tapping the enemy then fires: Equip, then the Attack with the bow',
                  len(orders) >= 2 and orders[0].startswith('order: equip') and 'with Bow' in orders[1], orders)
            # a tap outside the list goes back to the action menu without changing anything
            log, _, _ = run('pick_touch_outside', 'mo_pick', inp + ',tap:5:5,wait:4', 'pick')
            check('pick: a tap outside the list closes it (back to the action menu)', len(lines(log, 'menu entries:')) == 2 and 'order:' not in log, lines(log, 'menu entries:'))
    # one weapon: the list still shows (the weapon in hand is already highlighted: one Confirm goes on)
    log, _, _ = run('pick_single', 'mo_pick', atk + ',confirm,wait:4', 'single')
    check('pick: a unit with one weapon still gets the list (one row)', weapon_list(log) == [('Iron Sword', '30/30', True)], (weapon_list(log), log[-200:]))
    log, _, _ = run('pick_single_go', 'mo_pick', atk + ',confirm,confirm,confirm,wait:30', 'single')
    check('pick: ... and Confirm, Confirm attacks with it (no Equip)', len(lines(log, 'order: ')) == 1 and 'order: attack Hero' in log, lines(log, 'order: '))
    # no key: straight to the targets, today's behaviour
    log, _, _ = run('pick_off', 'mo_eq', atk + ',confirm,wait:4', 'pick')
    check('equipped: without the key Attack goes straight to the targets (no list)', 'weapon list' not in log and 'forecast:' in log, log[-300:])
    # the keys can be written out
    make_battle_game('mo_pick_explicit', pick='equipped')
    log, _, _ = run('pick_off2', 'mo_pick_explicit', atk + ',confirm,wait:4', 'pick')
    check('equipped: "attackWeaponChoice": "equipped" is the same', 'weapon list' not in log and 'forecast:' in log, log[-300:])
    # phone size and a big UI scale: the list still fits (the screenshots are looked at)
    make_battle_game('mo_pick_phone', pick='pick', width=270, height=480)
    log, img, _ = run('pick_phone', 'mo_pick_phone', atk + ',confirm,wait:4', 'pick')
    check('pick: the list opens at phone size', len(weapon_list(log)) == 2, log[-300:])
    log, img, _ = run('pick_phone_forecast', 'mo_pick_phone', atk + ',confirm,down,confirm,wait:6', 'pick')
    check('pick: the bow forecast shows at phone size', 'forecast:' in log, log[-300:])
    log, img, _ = run('pick_scaled', 'mo_pick_phone', atk + ',confirm,wait:4', 'pick', extra=('--ui-scale', '150'))
    check('pick: the list opens with a big UI scale', len(weapon_list(log)) == 2, log[-300:])
    # a forecast shot at the normal size for the pictures
    run('pick_forecast', 'mo_pick', atk + ',confirm,down,confirm,wait:6', 'pick')


def main():
    if not PLAYER or not os.path.exists(PLAYER):
        print('usage: menu_options_player_test.py PLAYER REPO [--shots DIR]')
        sys.exit(2)
    try:
        os.makedirs(GAMES)
        prep_lists()
        hide_tests()
        pick_tests()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    summary()
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
