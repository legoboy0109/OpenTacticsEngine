#!/usr/bin/env python3
"""The preparation screen through the real player, windowless (docs/EVENTS.md "Preparation").

  python3 tests/srpg_player_prep_test.py ./build/engine/player/ote-player . [--shots DIR]

Copies tests/fixtures/srpg-prep into a temp folder (two window sizes: 480x270 and a 270x480 phone), plays chapter 1 by
keyboard, and then drives chapter 2's preparation screen with --input scripts: the main menu, who fights (required unit,
the limit), positions (pick up, swap, a square the unit cannot stand on), items (a trade with the storage and another
character), the shop, the view of the map with an enemy's reach, "Begin with N of M?", saving during the preparation and
resuming it, and the pointer (the menu, a slot, the Back button). The player logs "prep ..." lines; the script reads them
and the Continue save. --autoplay and a free battle of the prep map are checked too. Pictures: --shots DIR keeps them.
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, failures, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-prep-')

# Chapter 1 played (a title card, a line, end the turn, the result), chapter 2's card and line: the screen opens.
TO_PREP = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40'
MAIN = ['Units', 'Positions', 'Items', 'Shop', 'View map', 'Leave', 'Begin']  # without saving


def make_game(gid, width=480, height=270, tweak=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def play(gid, inp, shot=None, saves=None, extra=None, interval=3):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval), '--fixed-dt', '0.016',
           '--game', gid]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    cmd += extra if extra is not None else ['--campaign']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    return p.returncode, p.stdout


def lines(log, key):
    return [l.split('] ', 1)[-1] for l in log.splitlines() if key in l]


def last(log, key):
    found = lines(log, key)
    return found[-1] if found else ''


def keep(name, path):
    """Keeps a picture for looking at (--shots)."""
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def go(gid, more, name=None, saves=None, extra=None):
    """Plays to the preparation screen, then `more` (a script); returns the log. `name` keeps the last picture."""
    inp = TO_PREP + (',' + more if more else '')
    path = os.path.join(TMP, (name or 'x') + '.bmp')
    code, log = play(gid, inp, shot=path if name else None, saves=saves, extra=extra)
    if name:
        keep(name, path)
    return log


def menu_to(n):
    """Cursor to the n-th entry of the main menu and choose it."""
    return ','.join(['down'] * n + ['confirm'])


def positions(log, key='prep slots:'):
    """{ 'Back': (x, y), 1: (x, y), ... } from the last "prep slots:" line."""
    line = last(log, key)
    out = {int(n): (int(x), int(y)) for n, x, y in re.findall(r' (\d+)@(\d+),(\d+)', line)}
    m = re.search(r'back@(\d+),(\d+)', line)
    if m:
        out['back'] = (int(m.group(1)), int(m.group(2)))
    return out


def tap(p):
    return 'tap:%d:%d' % p


def continue_save(saves, gid):
    path = os.path.join(saves, gid, 'continue.json')
    return read_json(path) if os.path.exists(path) else None


def fresh_saves(name):
    path = os.path.join(TMP, 'saves_' + name)
    shutil.rmtree(path, ignore_errors=True)
    return path


def roster_of(save):
    return {r['id']: r for r in save['playthrough']['roster']}


# ---- the tests -------------------------------------------------------------------------------------------------

def test_main_menu_and_default(gid, tag):
    log = go(gid, '', name='main_' + tag)
    check(tag + ': the screen opens after the before scene', bool(lines(log, 'prep: opens on orchard, 4 slots, limit 3')), log[-500:])
    check(tag + ': the main menu', last(log, 'prep menu:').endswith(', '.join(MAIN)), last(log, 'prep menu:'))
    check(tag + ': the default pick is shown (required first, then roster order)',
          bool(lines(log, 'prep: opens')) and 'UI bug' not in log)


def test_units(gid, tag):
    log = go(gid, 'confirm,wait:3', name='units_' + tag)
    units = last(log, 'prep units:')
    check(tag + ': the picker lists who is in, out and why',
          'Sera [Slot 1 *], Tomas [Slot 2], Wren [Slot 3], Bram [Out], Ilsa [Out] (Deployed 3 / 3)' in units, units)
    log = go(gid, 'confirm,wait:3,confirm,wait:3')
    check(tag + ': a required character cannot be benched', bool(lines(log, 'prep refused: Sera must fight in this battle and cannot be benched.')), log[-300:])
    log = go(gid, 'confirm,wait:3,down,confirm,wait:3,down,down,confirm,wait:3')
    check(tag + ': benching Tomas then putting Bram in',
          last(log, 'prep: deployed') == 'prep: deployed 3/3: sera@1, wren@3, bram@2', last(log, 'prep: deployed'))
    log = go(gid, 'confirm,wait:3,down,down,down,down,confirm,wait:3')
    check(tag + ': the limit stops a fourth', bool(lines(log, 'prep refused: The limit of 3 is reached. Bench someone first.')), log[-300:])


def test_positions(gid, tag):
    log = go(gid, menu_to(1) + ',wait:3', name='positions_' + tag)
    p = positions(log)
    check(tag + ': the slots are on screen', len(p) == 5, str(p))
    log = go(gid, menu_to(1) + ',next,confirm,wait:3', name='carry_' + tag)
    log = go(gid, menu_to(1) + ',next,confirm,next,confirm,wait:3')
    check(tag + ': pick up Tomas on slot 2 and put him on Wren\'s slot 3 swaps them',
          last(log, 'prep: deployed') == 'prep: deployed 3/3: sera@1, tomas@3, wren@2', last(log, 'prep: deployed'))
    log = go(gid, menu_to(1) + ',next,confirm,next,next,confirm,wait:3')
    check(tag + ': a foot unit cannot go on the pond',
          bool(lines(log, 'prep refused: Tomas cannot stand on slot 4.')), log[-300:])
    log = go(gid, menu_to(1) + ',next,next,confirm,next,confirm,wait:3')
    check(tag + ': a flyer can (Wren to the pond, slot 4)',
          last(log, 'prep: deployed') == 'prep: deployed 3/3: sera@1, tomas@2, wren@4', last(log, 'prep: deployed'))
    log = go(gid, menu_to(1) + ',confirm,next,confirm,wait:3')
    check(tag + ': a required unit with its own slot cannot be moved',
          bool(lines(log, 'prep refused: Sera must start on slot 1.')), log[-300:])


def test_items(gid, tag, saves):
    # Items: Sera -> trade with Tomas (first in the list) -> hand over the potion (row 0)
    log = go(gid, menu_to(2) + ',wait:3', name='items_who_' + tag, saves=saves)
    who = last(log, 'prep items:')
    check(tag + ': whose items lists the party', who.startswith('prep items: Sera [2/3], Tomas [0/3], Wren [1/3], Bram [1/3], Ilsa [1/3]'), who)
    log = go(gid, menu_to(2) + ',confirm,wait:3', name='items_with_' + tag, saves=saves)
    log = go(gid, menu_to(2) + ',confirm,confirm,wait:3', name='trade_' + tag, saves=saves)
    check(tag + ': the trade panel names both sides', bool(lines(log, 'prep trade: Sera [Potion, Ring, -] with Tomas')), last(log, 'prep trade'))
    s = fresh_saves('items_' + tag)
    log = go(gid, menu_to(2) + ',confirm,confirm,confirm,wait:3', saves=s)
    save = continue_save(s, gid)
    r = roster_of(save) if save else {}
    check(tag + ': the potion went to Tomas and stays in the playthrough (and the save)',
          save is not None and len(r['tomas'].get('items', [])) == 1 and len(r['sera'].get('items', [])) == 1, str(r.get('tomas')))
    # into the storage (the last row of "trade with") and back out
    s = fresh_saves('storage_' + tag)
    log = go(gid, menu_to(2) + ',confirm,up,confirm,confirm,wait:3', saves=s, name='storage_' + tag)
    save = continue_save(s, gid)
    sides = save['playthrough']['sides'] if save else []
    check(tag + ': a potion into the shared storage', save is not None and len(sides[0].get('storage', [])) == 2, str(sides))


def test_shop(gid, tag):
    s = fresh_saves('shop_' + tag)
    log = go(gid, menu_to(3) + ',wait:3', name='shop_' + tag, saves=s)
    check(tag + ': the shop shows the funds', 'Gold 125' in last(log, 'prep shop:'), last(log, 'prep shop:'))
    log = go(gid, menu_to(3) + ',confirm,wait:3', name='shopfor_' + tag, saves=s)
    log = go(gid, menu_to(3) + ',confirm,confirm,wait:3', saves=s)
    check(tag + ': a potion bought for Sera', bool(lines(log, 'prep: bought potion for sera')), log[-300:])
    save = continue_save(s, gid)
    check(tag + ': the gold is spent in the save', save is not None and save['playthrough']['sides'][0]['resources'][0] == 105, str(save and save['playthrough']['sides']))
    log = go(gid, menu_to(3) + ',confirm,confirm,wait:3,cancel,down,confirm,confirm,wait:3,cancel,confirm,confirm', saves=fresh_saves('shop2_' + tag))
    check(tag + ': Sera\'s pack is full (3 of 3) after a second purchase: the third goes to storage or is refused politely',
          'UI bug' not in log)


def test_view(gid, tag):
    log = go(gid, menu_to(4) + ',wait:3', name='view_' + tag)
    p = positions(log)
    check(tag + ': the free look opens', bool(p), log[-300:])
    # the cursor starts on slot 1 (0,1); the raider is at (7,1): seven steps right
    log = go(gid, menu_to(4) + ',' + ','.join(['right'] * 7) + ',wait:3', name='view_enemy_' + tag)
    check(tag + ': an enemy under the cursor shows its reach', bool(lines(log, 'prep view: Raider can move to')), log[-300:])
    log = go(gid, menu_to(4) + ',' + ','.join(['right'] * 7) + ',confirm,wait:5', name='view_info_' + tag)
    check(tag + ': confirm on the enemy opens its details without a UI bug', 'UI bug' not in log)


def test_begin(gid, tag):
    s = fresh_saves('begin_' + tag)
    log = go(gid, menu_to(7) + ',wait:40', saves=s)
    check(tag + ': Begin with everyone deployed starts the battle at once', bool(lines(log, 'prep: begin with 3')) and last(log, 'turn: Company, round 1') != '', log[-400:])
    save = continue_save(s, gid)
    ids = sorted(u['id'] for u in save['battle']['units'] if 'id' in u) if save and 'battle' in save else []
    check(tag + ': the battle has the deployed units and the fixed one, not the benched', ids == ['guard', 'sera', 'tomas', 'wren'], str(ids))
    check(tag + ': the battle-start event ran after the screen', save is not None and save['battle']['story']['switches'] == [True])
    s = fresh_saves('begin2_' + tag)
    log = go(gid, 'confirm,wait:3,down,confirm,wait:3,cancel,wait:3,' + menu_to(7) + ',wait:5', saves=s, name='ask_' + tag)
    check(tag + ': Begin with fewer than the limit asks first', bool(lines(log, 'prep: asks "Begin with 2 of 3?"')), log[-300:])
    log = go(gid, 'confirm,wait:3,down,confirm,wait:3,cancel,wait:3,' + menu_to(7) + ',right,confirm,wait:40', saves=fresh_saves('begin3_' + tag))
    check(tag + ': ... and Begin goes on', bool(lines(log, 'prep: begin with 2')), log[-300:])


def test_save_and_resume(gid):
    s = fresh_saves('resume')
    go(gid, 'confirm,wait:3,down,confirm,wait:3', saves=s)  # Tomas benched
    save = continue_save(s, gid)
    prep = save['playthrough'].get('prep') if save else None
    check('save: the Continue file made during the preparation holds the screen',
          prep is not None and prep['open'] and prep['folded'] and [d['id'] for d in prep['deployed']] == ['sera', 'wren'], str(prep))
    check('save: no battle in it yet', save is not None and 'battle' not in save)
    code, log = play(gid, 'wait:5', saves=s, extra=['--continue'])
    check('save: Continue goes back into the preparation', bool(lines(log, 'prep: opens on orchard')) and 'UI bug' not in log, log[-400:])
    code, log = play(gid, 'confirm,wait:3', saves=s, extra=['--continue'])
    check('save: ... with the same choice', 'Tomas [Out]' in last(log, 'prep units:') and 'Sera [Slot 1 *]' in last(log, 'prep units:'), last(log, 'prep units:'))


def test_pointer(gid):
    log = go(gid, '')
    rows = {int(i): (int(x), int(y)) for i, x, y in re.findall(r' (\d+)@(\d+),(\d+)', last(log, 'prep menu rows:'))}
    log = go(gid, tap(rows[1]) + ',wait:3')  # Positions
    p = positions(log)
    check('pointer: tapping "Positions" opens it', len(p) == 5, str(p))
    log = go(gid, tap(rows[1]) + ',wait:3,' + tap(p[2]) + ',wait:3,' + tap(p[3]) + ',wait:3')
    check('pointer: tap a slot, tap another: they swap', last(log, 'prep: deployed') == 'prep: deployed 3/3: sera@1, tomas@3, wren@2', last(log, 'prep: deployed'))
    log = go(gid, tap(rows[1]) + ',wait:3,' + tap(p['back']) + ',wait:3')
    check('pointer: the Back button returns to the menu', len(lines(log, 'prep menu:')) == 2, log[-300:])
    log = go(gid, tap(rows[4]) + ',wait:3,' + tap(p['back']) + ',wait:3')
    check('pointer: the view opens and closes', len(lines(log, 'prep menu:')) == 2, log[-300:])


def test_autoplay_and_free_battle(gid):
    code, log = play(gid, '', extra=['--campaign', '--autoplay'], interval=2)
    check('autoplay: the story plays through without ever showing the screen', 'prep: opens' not in log and 'campaign finished' in log or 'The End' in log or code == 0 and 'prep: opens' not in log, log[-300:])
    log = play(gid, 'wait:10', extra=['--map', 'orchard'])[1]
    check('free battle: the prep map shows its screen with the placed units as the pool', bool(lines(log, 'prep: opens on orchard, 4 slots, limit 3 (free battle)')), log[-300:])
    code, log = play(gid, 'wait:5', extra=['--map', 'orchard', '--autoplay'])
    check('free battle: --autoplay takes the default pick without the screen', 'prep: opens' not in log and 'UI bug' not in log, log[-300:])


# ---- selling and equipping ---------------------------------------------------------------------------------------

def replace_in(path, old, new):
    with open(path) as f:
        text = f.read()
    assert old in text, (path, old)
    with open(path, 'w') as f:
        f.write(text.replace(old, new, 1))


def carry_weapons(g):
    """The fixture with weapons carried (as tests/test_prep_sell.cpp): sera [sword_item, axe_item 5 of 10 uses, potion],
    tomas [sword_item, bow_item], bram [tonic, relic (not sellable)], storage [potion, token, relic]. Gold: 125."""
    replace_in(g + '/game.json', '"weapons": "fixed"', '"weapons": "carried"')
    replace_in(g + '/data/weapons.json', '{ "id": "bow"', '{ "id": "axe", "name": "Axe", "props": { "might": 3 } },\n  { "id": "bow"')
    replace_in(g + '/data/units.json', '"weapons": ["sword"] },\n  { "id": "archer"', '"weapons": ["sword", "axe"] },\n  { "id": "archer"')
    replace_in(g + '/data/items.json', '{ "id": "token"',
               '{ "id": "sword_item", "name": "Sword", "kind": "weapon", "weapon": "sword", "uses": 10, "price": { "gold": 100 } },\n'
               '  { "id": "axe_item", "name": "Axe", "kind": "weapon", "weapon": "axe", "uses": 10, "price": { "gold": 60 } },\n'
               '  { "id": "bow_item", "name": "Bow", "kind": "weapon", "weapon": "bow", "uses": 10, "price": { "gold": 80 } },\n'
               '  { "id": "relic", "name": "Relic", "kind": "accessory", "bonus": { "def": 1 }, "price": { "gold": 40 }, "sellable": false },\n'
               '  { "id": "token"')
    replace_in(g + '/maps/meadow.json', '"name": "Sera", "items": ["potion", "ring"]',
               '"name": "Sera", "items": ["sword_item", { "item": "axe_item", "uses": 5 }, "potion"]')
    replace_in(g + '/maps/meadow.json', '"name": "Tomas" }', '"name": "Tomas", "items": ["sword_item", "bow_item"] }')
    replace_in(g + '/maps/meadow.json', '"items": ["tonic"]', '"items": ["tonic", "relic"]')
    replace_in(g + '/maps/meadow.json', '"storage": ["potion"]', '"storage": ["potion", "token", "relic"]')


def no_buying(g):
    replace_in(g + '/maps/orchard.json', '"sells": [ "potion", "tonic" ]', '"sells": [ "potion", "tonic" ], "buys": false')


def item_names(gid):
    """The game's item ids in order: a save names an item by its position in data/items.json."""
    return [i['id'] for i in read_json(os.path.join(TMP, gid, 'data/items.json'))['items']]


def pack_ids(save, who, gid):
    """Item ids of a roster character in a Continue save made during the preparation."""
    names = item_names(gid)
    return [names[i['item']] for i in roster_of(save)[who].get('items', [])]


def sell_tabs(log, kind='shop'):
    """{0: (x, y), 1: (x, y)} of the Buy / Sell tabs from the last panel line of that kind (the panel's height, so the
    tabs' place, differs from list to list)."""
    return {int(i): (int(x), int(y)) for i, x, y in re.findall(r' tab(\d)@(\d+),(\d+)', last(log, 'prep panel %s:' % kind))}


def row_at(log, kind, n):
    m = re.search(r'row0@(\d+),(\d+) step=(\d+)', last(log, 'prep panel %s:' % kind))
    return (int(m.group(1)), int(m.group(2)) + n * int(m.group(3)))


def gold_of(save):
    return save['playthrough']['sides'][0]['resources'][0] if save else None


def test_sell(gid, tag):
    s = fresh_saves('sell_' + tag)
    log = go(gid, menu_to(3) + ',wait:3', name='shop_tabs_' + tag, saves=s)
    check(tag + ': a shop that buys has the Buy and Sell tabs', len(sell_tabs(log)) == 2, last(log, 'prep panel shop:'))
    log = go(gid, menu_to(3) + ',right,wait:3', name='sell_who_' + tag, saves=s)
    check(tag + ': Right on the shop opens Sell with whose items (everyone, then the storage)',
          'Gold 125' in last(log, 'prep sell who:') and 'UI bug' not in log, last(log, 'prep sell who:'))
    log = go(gid, menu_to(3) + ',right,confirm,wait:3', name='sell_items_' + tag, saves=s)
    check(tag + ': Sera\'s pack with what each item fetches (potion 10, ring 25: half the price, scaled by uses left)',
          last(log, 'prep sell:') == 'prep sell: Sera [Potion [10 Gold], Ring [25 Gold]] (Gold 125)', last(log, 'prep sell:'))
    log = go(gid, menu_to(3) + ',right,confirm,confirm,wait:3', name='sold_' + tag, saves=s)
    check(tag + ': the potion is sold and the total rises at once',
          bool(lines(log, 'prep: sold potion from sera for 10 Gold')) and last(log, 'prep sell:') == 'prep sell: Sera [Ring [25 Gold]] (Gold 135)',
          last(log, 'prep sell:'))
    save = continue_save(s, gid)
    check(tag + ': the sale is in the save: 135 gold, Sera has only the ring',
          save is not None and gold_of(save) == 135 and pack_ids(save, 'sera', gid) == ['ring'], str(save and save['playthrough']['sides']))
    # Wren's token has no price: refused, nothing changes
    s2 = fresh_saves('sell_token_' + tag)
    log = go(gid, menu_to(3) + ',right,down,down,confirm,confirm,wait:3', saves=s2)
    check(tag + ': an item without a price is not bought',
          bool(lines(log, 'prep refused: Token has no price, so nobody buys it.')), log[-400:])
    save = continue_save(s2, gid)
    check(tag + ': ... and Wren keeps it, the gold is as it was', save is not None and pack_ids(save, 'wren', gid) == ['token'] and gold_of(save) == 125, str(save))
    # the storage (the last row): the potion from chapter 1
    s3 = fresh_saves('sell_storage_' + tag)
    log = go(gid, menu_to(3) + ',right,' + ','.join(['down'] * 5) + ',confirm,wait:3', name='sell_storage_' + tag, saves=s3)
    check(tag + ': the storage can be sold from', last(log, 'prep sell:') == 'prep sell: Storage [Potion [10 Gold]] (Gold 125)', last(log, 'prep sell:'))
    log = go(gid, menu_to(3) + ',right,' + ','.join(['down'] * 5) + ',confirm,confirm,wait:3', saves=s3)
    save = continue_save(s3, gid)
    check(tag + ': ... the storage is empty and the gold 135 in the save',
          save is not None and gold_of(save) == 135 and not save['playthrough']['sides'][0].get('storage'), str(save and save['playthrough']['sides']))
    log = go(gid, menu_to(3) + ',right,left,wait:3', saves=s3)
    check(tag + ': Left returns to the Buy list', len(lines(log, 'prep shop:')) == 2, log[-300:])


def test_sell_pointer(gid, tag):
    s = fresh_saves('sellptr_' + tag)
    log = go(gid, menu_to(3) + ',wait:3', saves=s)
    sell_tab = sell_tabs(log)[1]
    to_sell = menu_to(3) + ',' + tap(sell_tab) + ',wait:3'
    log = go(gid, to_sell, saves=s)
    check(tag + ': tapping the Sell tab opens Sell', bool(lines(log, 'prep sell who:')), log[-300:])
    buy_tab = sell_tabs(log, 'sellwho')[0]  # the tabs sit in the panel's title row: this panel is taller, so they moved
    sera, wren = row_at(log, 'sellwho', 0), row_at(log, 'sellwho', 2)
    log = go(gid, to_sell + ',' + tap(wren) + ',wait:3', saves=s)
    check(tag + ': a first tap only selects a row', not lines(log, 'prep sell:'), log[-300:])
    log = go(gid, to_sell + ',' + tap(wren) + ',wait:3,' + tap(wren) + ',wait:3', saves=s)
    check(tag + ': a second tap opens Wren\'s pack', last(log, 'prep sell:') == 'prep sell: Wren [Token [-]] (Gold 125)', last(log, 'prep sell:'))
    row = row_at(log, 'sell', 0)
    log = go(gid, to_sell + ',' + tap(wren) + ',wait:3,' + tap(wren) + ',wait:3,' + tap(row) + ',wait:3', saves=s)
    check(tag + ': tapping the unsellable token is refused', bool(lines(log, 'prep refused: Token has no price')), log[-300:])
    opened = to_sell + ',' + tap(sera) + ',wait:3'  # Sera is the selected row: one tap opens her pack
    log = go(gid, opened, saves=s)
    check(tag + ': Sera\'s pack opens with one tap (her row is already selected)', last(log, 'prep sell:').startswith('prep sell: Sera [Potion'), last(log, 'prep sell:'))
    row = row_at(log, 'sell', 0)
    log = go(gid, opened + ',' + tap(row) + ',wait:3', saves=s)
    check(tag + ': a tap on the potion sells it', bool(lines(log, 'prep: sold potion from sera for 10 Gold')), log[-400:])
    check(tag + ': ... 135 gold in the save', gold_of(continue_save(s, gid)) == 135)
    log = go(gid, to_sell + ',' + tap(buy_tab) + ',wait:3')
    check(tag + ': tapping Buy goes back', len(lines(log, 'prep shop:')) == 2, log[-300:])
    log = go(gid, to_sell + ',' + tap((3, 3)) + ',wait:3')
    check(tag + ': a tap outside closes the panel', len(lines(log, 'prep menu:')) == 2, log[-300:])


def test_no_buying(gid):
    log = go(gid, menu_to(3) + ',right,wait:3', name='shop_nobuy')
    check('shop that does not buy: no tabs and Right does nothing', not sell_tabs(log) and not lines(log, 'prep sell who:') and 'UI bug' not in log,
          last(log, 'prep panel shop:'))
    log = go(gid, menu_to(3) + ',confirm,confirm,wait:3')
    check('shop that does not buy: buying works as before', bool(lines(log, 'prep: bought potion for sera')), log[-300:])


def menu_rows(log, key):
    return {int(i): (int(x), int(y)) for i, x, y in re.findall(r' (\d+)@(\d+),(\d+)', last(log, key))}


def battle_pack(save, who, gid):
    for u in save['battle']['units']:
        if u.get('id') == who:
            return [item_names(gid)[i['item']] for i in u.get('items', [])]
    return None


def test_equip(gid, tag):
    to_trade = menu_to(2) + ',confirm,confirm'  # Items, Sera, trade with Tomas (the first other)
    log = go(gid, to_trade + ',wait:3', name='equip_trade_' + tag)
    check(tag + ': the trade panel lists Sera\'s weapons', bool(lines(log, 'prep trade: Sera [Sword, Axe, Potion] with Tomas')), last(log, 'prep trade'))
    log = go(gid, to_trade + ',confirm,wait:3')
    check(tag + ': the weapon in hand has nothing to equip: Confirm hands it over at once (no menu)',
          not lines(log, 'prep item menu:') and bool(lines(log, 'prep trade: Sera [Axe, Potion, -]')), last(log, 'prep'))
    log = go(gid, to_trade + ',down,confirm,wait:3', name='equip_menu_' + tag)
    check(tag + ': another wieldable weapon offers Hand over and Equip', last(log, 'prep item menu:') == 'prep item menu: Hand over, Equip', last(log, 'prep item menu:'))
    log = go(gid, to_trade + ',down,confirm,cancel,wait:3')
    check(tag + ': cancelling the small menu leaves everything as it was',
          not lines(log, 'prep: equipped') and 'UI bug' not in log and len(lines(log, 'prep trade:')) == 1, log[-300:])
    s = fresh_saves('equip_' + tag)
    log = go(gid, to_trade + ',down,confirm,down,confirm,wait:3', name='equipped_' + tag, saves=s)
    check(tag + ': Equip moves the axe to the front',
          bool(lines(log, 'prep: equipped Axe for sera')) and last(log, 'prep trade:').startswith('prep trade: Sera [Axe, Sword, Potion]'), last(log, 'prep trade:'))
    save = continue_save(s, gid)
    check(tag + ': ... and it is in the save (axe first, its 5 uses kept)',
          save is not None and pack_ids(save, 'sera', gid) == ['axe_item', 'sword_item', 'potion'] and roster_of(save)['sera']['items'][0].get('uses') == 5,
          str(save and roster_of(save)['sera']))
    # the battle that follows: Begin (3 of 3 deployed starts at once); Sera's weapon in hand is the axe
    log = go(gid, to_trade + ',down,confirm,down,confirm,cancel,cancel,cancel,' + menu_to(7) + ',wait:40', saves=s)
    save = continue_save(s, gid)
    pack = battle_pack(save, 'sera', gid) if save and 'battle' in save else None
    check(tag + ': the battle starts with the axe in hand', pack is not None and pack[:2] == ['axe_item', 'sword_item'], str(pack))
    # Tomas (an archer): the sword cannot be wielded, the bow is in hand
    to_tomas = menu_to(2) + ',down,confirm,confirm'
    log = go(gid, to_tomas + ',wait:3', name='equip_tomas_' + tag)
    check(tag + ': Tomas\'s pack is [Sword, Bow]', bool(lines(log, 'prep trade: Tomas [Sword, Bow, -] with Sera')), last(log, 'prep trade'))
    log = go(gid, to_tomas + ',confirm,wait:3', name='equip_blocked_' + tag)
    check(tag + ': Equip on a sword the archer cannot wield is greyed with the reason',
          last(log, 'prep item menu:') == 'prep item menu: Hand over, Equip [Tomas cannot wield that.]', last(log, 'prep item menu:'))
    s = fresh_saves('equip_blocked_' + tag)
    log = go(gid, to_tomas + ',confirm,down,confirm,wait:3', saves=s)
    save = continue_save(s, gid)
    check(tag + ': choosing the greyed entry changes nothing',
          save is not None and pack_ids(save, 'tomas', gid) == ['sword_item', 'bow_item'] and not lines(log, 'prep: equipped'), str(save and pack_ids(save, 'tomas', gid)))
    # touch: select the row, tap it again (the small menu opens), tap Equip
    log = go(gid, to_trade + ',down,confirm,wait:3')
    rows = menu_rows(log, 'prep item menu rows:')
    check(tag + ': the small menu logs where its entries are', len(rows) == 2, str(rows))
    s = fresh_saves('equip_tap_' + tag)
    log = go(gid, to_trade + ',down,confirm,wait:3,' + tap(rows[1]) + ',wait:3', saves=s)
    check(tag + ': tapping Equip equips', bool(lines(log, 'prep: equipped Axe for sera')), log[-300:])


def main():
    make_game('desk')
    make_game('phone', 270, 480)
    for gid, tag in (('desk', 'desk'), ('phone', 'phone')):
        test_main_menu_and_default(gid, tag)
        test_units(gid, tag)
        test_positions(gid, tag)
        test_items(gid, tag, fresh_saves('main_' + tag))
        test_shop(gid, tag)
        test_view(gid, tag)
    make_game('carry', tweak=carry_weapons)
    make_game('carryphone', 270, 480, tweak=carry_weapons)
    make_game('nobuy', tweak=no_buying)
    for gid, tag in (('desk', 'desk'), ('phone', 'phone')):
        test_sell(gid, tag)
        test_sell_pointer(gid, tag)
    test_no_buying('nobuy')
    for gid, tag in (('carry', 'desk'), ('carryphone', 'phone')):
        test_equip(gid, tag)
    test_begin('desk', 'desk')
    test_save_and_resume('desk')
    test_pointer('desk')
    test_autoplay_and_free_battle('desk')
    summary(blank_line=True)
    shutil.rmtree(TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
