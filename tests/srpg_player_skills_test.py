#!/usr/bin/env python3
"""The player's skills and unit details (docs/SRPG.md step U1), windowless.

  python3 tests/srpg_player_skills_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_skills_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

This is U1's part of the player test of the contract (tests/srpg_player_test.py): five steps run in parallel, so each
writes its own file. Writes a small strategy-RPG game in a temp folder (two window sizes), runs the player headless with
--input scripts and --no-saving, and reads the player's log lines and the pixels of the screenshots:

  - the action menu has Info (only in a game that uses strategy-RPG data), Info opens the unit details, whose pages
    Stats, Skills and Items turn with Right; the battle menu's Unit opens the details of the unit under the cursor;
  - the forecast lists the skills that could fire with their chance, "Strikes first" for a defender that does, and
    strikes in a row; a skill that fires shows its name over the unit before the strike; "Survived!";
  - a level-up that teaches a skill lists it in the level-up box;
  - a unit that has acted (move again) shows its range and a menu of Info and Wait only.
"""
import json
import os
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, failures, inp, lines, player_and_repo, player_cmd, shots_dir, summary, write_json)
Image = Bmp  # (the name this script uses)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-u1-')

IDLE = 'wait:14'  # the turn banner plays first (38 frames at 4 per input); it takes the first inputs otherwise
GOLD = (255, 225, 110)      # a skill label floating over its owner
LEVEL_FRAME = (255, 220, 90)  # the level-up box border


def skill(sid, name, when, do, chance=None, desc=None):
    block = {'when': when, 'do': do}
    if chance is not None:
        block['chance'] = chance
    s = {'id': sid, 'name': name, 'blocks': [block]}
    if desc:
        s['description'] = desc
    return s


def unit(character=None, type_=None, side='blue', x=0, y=0, **extra):
    u = {'side': side, 'x': x, 'y': y}
    u.update({'character': character} if character else {'type': type_})
    u.update(extra)
    return u


def tiles(w, h):
    return ['.' * w for _ in range(h)]


def map_json(name, blue, red, w=8, h=4):
    return {'name': name, 'grid': {'type': 'square', 'width': w, 'height': h}, 'legend': {'.': 'plain'},
            'tiles': tiles(w, h), 'units': blue + red,
            'sides': [{'id': 'blue', 'name': 'Blue', 'controller': 'human'}, {'id': 'red', 'name': 'Red', 'controller': 'ai'}]}


def write_game(root, gid, width, height, srpg=True):
    """The test game. srpg=False leaves out every strategy-RPG file (the menus must stay as they were)."""
    g = os.path.join(root, gid)
    maps = {
        # Eirik (Lv 3, learns Heavy Blow at 4) attacks Gorm, who strikes first; one hit is a level.
        'duel': map_json('Duel', [unit('eirik', x=2, y=1, items=['amulet', 'potion'])],
                         [unit('gorm', side='red', x=3, y=1, stats={'hp': 30, 'atk': 9})]),
        # Lyra has six skills; the forecast has to cut its notes.
        'many': map_json('Many', [unit('lyra', x=2, y=1)], [unit(type_='grunt', side='red', x=3, y=1, stats={'hp': 30})]),
        # A grunt at 3 HP that survives a deadly strike.
        'survive': map_json('Survive', [unit(type_='hero', x=2, y=1)],
                            [unit(type_='tough_grunt', side='red', x=3, y=1, stats={'hp': 3}), unit(type_='grunt', side='red', x=7, y=3)]),
        # A scout with "move again".
        'again': map_json('Again', [unit(type_='scout', x=1, y=1)], [unit(type_='grunt', side='red', x=3, y=1, stats={'hp': 30}),
                                                                    unit(type_='grunt', side='red', x=7, y=3)]),
    }
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'U1 Player Test',
        'view': {'width': width, 'height': height, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Attack'}, {'id': 'skl', 'name': 'Skill'},
                  {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True, 'items': {'weapons': 'fixed', 'inventorySize': 3},
                  'progression': {'enabled': True, 'xpPerLevel': 100, 'maxLevel': 20, 'xpHit': '100', 'xpKill': '100'}},
        'formulas': {'damage': 'a.atk - b.def'},
        'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in maps]})
    if not srpg:  # the same maps with plain unit types (no characters file to name)
        kinds = {'eirik': 'hero', 'gorm': 'grunt', 'lyra': 'hero'}
        for m in maps.values():
            for u in m['units']:
                if 'character' in u:
                    u['type'] = kinds[u.pop('character')]
    for mid, m in maps.items():
        write_json(g + '/maps/%s.json' % mid, m)
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain'}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword', 'skills': ['blade_focus'] if srpg else []}]})
    write_json(g + '/data/items.json', {'items': [
        {'id': 'potion', 'name': 'Potion', 'kind': 'usable', 'uses': 3, 'heal': 5},
        {'id': 'amulet', 'name': 'Amulet', 'kind': 'accessory', 'bonus': {'hp': 2, 'def': 1},
         **({'skills': ['lucky_charm']} if srpg else {})}]})
    stats = lambda hp, atk, skl, d, mov: {'hp': hp, 'atk': atk, 'skl': skl, 'def': d, 'mov': mov}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'hero', 'name': 'Hero', 'glyph': 'H', 'moveType': 'foot', 'stats': stats(24, 8, 4, 3, 4), 'weapons': ['sword'],
         'growths': {'hp': 100, 'atk': 40}, 'tags': ['lord'],
         **({'skills': ['steady_recovery']} if srpg else {})},
        {'id': 'grunt', 'name': 'Grunt', 'glyph': 'g', 'moveType': 'foot', 'stats': stats(12, 4, 1, 1, 3), 'weapons': ['sword'],
         **({'skills': ['counter_stance']} if srpg else {})},
        {'id': 'tough_grunt', 'name': 'Grunt', 'glyph': 't', 'moveType': 'foot', 'stats': stats(12, 4, 1, 1, 3), 'weapons': ['sword'],
         **({'skills': ['last_stand']} if srpg else {})},
        {'id': 'scout', 'name': 'Scout', 'glyph': 's', 'moveType': 'foot', 'stats': stats(14, 7, 3, 2, 6), 'weapons': ['sword'],
         **({'skills': ['strike_and_move', 'venom_edge']} if srpg else {})}]})
    if not srpg:
        return g
    write_json(g + '/data/characters.json', {'characters': [
        {'id': 'eirik', 'name': 'Eirik', 'type': 'hero', 'level': 3, 'skills': ['bold'],
         'learns': [{'level': 4, 'skill': 'heavy_blow'}]},
        {'id': 'gorm', 'name': 'Gorm', 'type': 'grunt'},
        {'id': 'lyra', 'name': 'Lyra', 'type': 'hero', 'level': 5,
         'skills': ['heavy_blow', 'pierce', 'fury', 'lucky_strike', 'frenzy', 'twin_blades']}]})
    write_json(g + '/data/skills.json', {'skills': [
        skill('steady_recovery', 'Steady Recovery', 'turnStart', [{'heal': {'percent': 10}}], desc='Heals a little at the start of each turn.'),
        skill('bold', 'Bold', 'always', [{'stats': {'atk': 1}}], desc='Hits a little harder.'),
        skill('blade_focus', 'Blade Focus', 'always', [{'stats': {'atk': 1}}]),
        skill('lucky_charm', 'Lucky Charm', 'always', [{'stats': {'def': 1}}], desc='A charm that turns small blows aside.'),
        skill('heavy_blow', 'Heavy Blow', 'strike', [{'damagePercent': 200}], chance=40, desc='A chance of double damage.'),
        skill('sure_strike', 'Sure Strike', 'strike', [{'damage': 2}], chance=100),
        skill('pierce', 'Pierce', 'strike', [{'damage': 3}], chance=30),
        skill('fury', 'Fury', 'strike', [{'damage': 4}], chance=20),
        skill('lucky_strike', 'Lucky Strike', 'strike', [{'damage': 5}], chance=10),
        skill('frenzy', 'Frenzy', 'strike', [{'damage': 6}], chance=5),
        skill('twin_blades', 'Twin Blades', 'combat', [{'multiStrike': 2}], desc='Every strike lands twice.'),
        skill('counter_stance', 'Counter Stance', 'combat', [{'strikeFirst': True}], desc='Strikes first when attacked.'),
        skill('last_stand', 'Last Stand', 'struck', [{'survive': True}], desc='Cannot be knocked out in one blow.'),
        skill('venom_edge', 'Venom Edge', 'afterCombat', [{'hurt': 2, 'to': 'foe'}], desc='Leaves the foe poisoned.'),
        skill('strike_and_move', 'Strike and Move', 'always', [{'moveAgain': True}], desc='Can move on after attacking.')]})
    # Eirik also gets a sure activation so a fight always shows a label.
    ch = json.load(open(g + '/data/characters.json'))
    ch['characters'][0]['skills'].append('sure_strike')
    write_json(g + '/data/characters.json', ch)
    return g


def ticks(script):
    """Input slots a script takes: a press is one, wait:N is N (every slot is `interval` frames)."""
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(game, map_id, inp, shot=None, frames=None, interval=4, args=()):
    """Runs the player. Without `frames` the run ends 60 frames after the script; with it a screenshot is taken then."""
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', game, '--map', map_id, '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(args))
    return p.returncode, p.stdout


def shot(name, game, map_id, inp, **kw):
    path = os.path.join(TMP, name + '.bmp')
    code, log = run(game, map_id, inp, shot=path, **kw)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, code


# Keys: the unit under the cursor at the start is the first of the human side (Eirik / Lyra / Scout).
ACTION_MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']          # select, stay put: the action menu is open


def frames_at(script, extra):
    """The frame `extra` frames after the last input of the script was pressed."""
    return ticks(script) * 4 + extra


def test_plain_game(game):
    code, log = run(game, 'duel', inp(*ACTION_MENU))
    check('a game without strategy-RPG data has no Info entry', code == 0 and not lines(log, 'action menu:'), log[-300:])
    code, log = run(game, 'duel', inp(IDLE, 'right', 'confirm', 'wait:3'))
    check('and no Unit entry in its battle menu', code == 0 and bool(lines(log, 'battle menu:')) and ', Unit' not in log, log[-300:])


def test_menus_and_details(game):
    code, log = run(game, 'duel', inp(*ACTION_MENU))
    entry = lines(log, 'action menu:')
    check('%s: the action menu lists Info right before Wait' % game, bool(entry) and 'Attack, Items, Info, Wait' in entry[-1], str(entry))
    # Info is the third entry (Attack, Items, Info, Wait): two Downs.
    base = ACTION_MENU + ['down', 'down', 'confirm', 'wait:3']
    for i, page in enumerate(['stats', 'skills', 'items']):
        script = inp(*base, *(['right', 'wait:2'] * i))
        img, log, code = shot('details_%s_%s' % (page, game), game, 'duel', script, frames=frames_at(script, 20))
        pages = lines(log, 'unit details:')
        check('%s: Info opens the unit details, page %s' % (game, page), code == 0 and bool(pages), log[-500:])
        if i == 0 and pages:
            check('%s: the pages are Stats, Skills, Items, of Eirik' % game, 'Eirik (Stats, Skills, Items)' in pages[-1], pages[-1])
    # Cancel closes the details and goes back to the action menu.
    code, log = run(game, 'duel', inp(*base, 'cancel', 'wait:3'))
    check('%s: Cancel in the details returns to the action menu' % game, len(lines(log, 'action menu:')) >= 2, log[-400:])
    # The battle menu has Unit when the cursor is on a unit, and it opens that unit's details (an enemy: Gorm).
    code, log = run(game, 'duel', inp(IDLE, 'right', 'confirm', 'wait:3'))   # the cursor starts on Eirik (2,1); Gorm is at (3,1)
    menu = lines(log, 'battle menu:')
    check('%s: the battle menu has Unit over a unit' % game, bool(menu) and 'Unit' in menu[-1], str(menu))
    code, log = run(game, 'duel', inp(IDLE, 'right', 'confirm', 'wait:3', 'down', 'confirm', 'wait:3'))
    pages = lines(log, 'unit details:')
    check('%s: Unit opens the details of the unit under the cursor (Gorm)' % game, bool(pages) and 'Gorm' in pages[-1], str(pages))
    code, log = run(game, 'duel', inp(IDLE, 'down', 'down', 'confirm', 'wait:3'))
    menu = lines(log, 'battle menu:')
    check('%s: the battle menu has no Unit over an empty square' % game, bool(menu) and 'Unit' not in menu[-1], str(menu))
    # Cancel from the battle menu's details goes back to the map (no action menu opens).
    code, log = run(game, 'duel', inp(IDLE, 'right', 'confirm', 'wait:3', 'down', 'confirm', 'wait:3', 'cancel', 'wait:3'))
    check('%s: Cancel in those details goes back to the map' % game, len(lines(log, 'action menu:')) == 0 and bool(lines(log, 'unit details:')), log[-300:])


def test_panel():
    # The info panel under the cursor: type, skills (up to three, then +N). Eirik has Bold, Sure Strike, Steady
    # Recovery, Blade Focus and Lucky Charm: three rows and "+2".
    script = inp(IDLE, 'wait:5')
    img, log, code = shot('panel_eirik', 'u1', 'duel', script, frames=frames_at(script, 10))
    check('the panel shows skill names in the panel column', img.count((255, 235, 130), 10, (352, 0, 480, 259)) > 60)


def test_forecast():
    g = 'u1'
    script = inp(*ACTION_MENU, 'confirm', 'wait:3')
    img, log, code = shot('forecast_duel', g, 'duel', script, frames=frames_at(script, 20))
    f = lines(log, 'forecast:')
    check('the forecast lists a skill that could fire with its chance', bool(f) and 'Sure Strike 100%' in f[-1], str(f))
    check('the forecast says who strikes first', bool(f) and f[-1].split('|')[1].count('Strikes first') == 1, str(f))
    img, log, code = shot('forecast_many', g, 'many', script, frames=frames_at(script, 20))
    f = lines(log, 'forecast:')
    check('the forecast cuts the notes at three with "+N more"', bool(f) and '+2 more' in f[-1] and 'Heavy Blow 40%' in f[-1], str(f))
    check('the forecast shows strikes in a row', bool(f) and 'x1 x2' in f[-1], str(f))
    img, log, code = shot('forecast_many_phone', 'u1phone', 'many', script, frames=frames_at(script, 20))
    check('the forecast fits a phone-sized window', code == 0 and img.w == 320)


def test_fight():
    g = 'u1'
    atk = inp(*ACTION_MENU, 'confirm', 'wait:3', 'confirm')
    code, log = run(g, 'duel', atk + ',wait:80')
    shown = lines(log, 'skill activated:')
    check('a skill that fires is named over its owner', any('Eirik - Sure Strike' in s for s in shown), str(shown))
    check('no skill event is left unplayed', not lines(log, 'not shown yet'), log[-500:])
    box = lines(log, 'level-up box:')
    check('a level-up that teaches a skill lists it in the box', any('New skill: Heavy Blow' in s for s in box), str(box))
    # The label itself: gold pixels over the map while it floats; the level-up box after it.
    img, log, code = shot('fight_label', g, 'duel', atk, frames=frames_at(atk, 42))
    check('the skill label is drawn over the map', img.count(GOLD, 20, (0, 0, 352, 259)) > 100)
    img, log, code = shot('levelup_skill', g, 'duel', atk, frames=frames_at(atk, 110))
    check('the level-up box is drawn', img.count(LEVEL_FRAME, 10) > 100)
    code, log = run(g, 'survive', atk)
    check('"Survived!" is shown when a skill keeps a unit up', bool(lines(log, 'Survived!')), log[-500:])


def test_again():
    g = 'u1'
    # Select the scout, walk one square, attack (a level-up box to dismiss), then pick it again: its range shows, no red
    # squares; Confirm on its own square opens a menu of Info and Wait only.
    script = inp(IDLE, 'confirm', 'wait:3', 'right', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:200',
                 'confirm', 'wait:100', 'confirm', 'wait:5')
    img, log, code = shot('acted_range', g, 'again', script, frames=frames_at(script, 10))
    check('a unit that has acted shows its remaining range when picked', img.count((72, 110, 190), 40, (0, 0, 352, 259)) > 200)
    check('and no attack squares', img.count((200, 60, 60), 40, (0, 0, 352, 259)) == 0)
    hurt = lines(log, 'unit hurt:')
    check('the hurt of an afterCombat skill is shown', any('Grunt -2' in s for s in hurt), str(hurt))
    script += ',confirm,wait:5'
    code, log = run(g, 'again', script)
    menu = lines(log, 'action menu:')
    check('a unit that has acted gets Info and Wait only', bool(menu) and menu[-1].endswith('action menu: Info, Wait'), str(menu[-2:]))


def main():
    try:
        write_game(TMP, 'u1', 480, 270)
        write_game(TMP, 'u1phone', 320, 480)
        write_game(TMP, 'plain', 480, 270, srpg=False)
        test_plain_game('plain')
        test_menus_and_details('u1')
        test_menus_and_details('u1phone')
        test_panel()
        test_forecast()
        test_fight()
        test_again()
    finally:
        if not SHOTS:
            shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
