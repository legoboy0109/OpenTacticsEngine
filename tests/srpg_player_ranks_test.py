#!/usr/bin/env python3
"""The player's weapon ranks and stat caps (docs/SRPG.md section 7), windowless.

  python3 tests/srpg_player_ranks_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_ranks_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-ranks into a temp folder (two window sizes: 480x270 and a 270x480 phone), runs the player
headless with --input scripts and --no-saving, and reads the player's log lines and the pixels of the screenshots:

  - the unit details open from the action menu; the Stats page lists the weapon ranks and a stat at its cap in gold
    ("MAX"); the Items page greys a weapon the rank does not allow and gives the reason in red;
  - the Items screen lists that weapon with its reason, and its choice menu has Equip greyed with the reason;
  - a fight that raises a rank floats "Sword rank C" over the unit (gold) and logs it;
  - a level-up of a unit whose stat is at its cap says so in the level-up box ("Strength: max");
  - the same on a phone-sized window.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402
Image = Bmp  # (the name this script uses)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
GOLD = (255, 225, 110)       # MAX, the rank-up label
RED = (255, 140, 120)        # a reason ("Needs rank B")
GREYED = (110, 114, 140)     # a weapon the rank does not allow
LEVEL_FRAME = (255, 220, 90)  # the level-up box border


def make_game(gid, width, height, strong=False):
    """A copy of the fixture. `strong`: Kell starts at Strength 10 (its cap) and a hit is a level (xpHit 100), and his
    sword experience is 6, one short of rank C (7), so that one more use ranks him up."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-ranks'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    game['formulas']['hit'] = '100'
    if strong:
        game['rules']['progression']['xpHit'] = '100'
    write_json(g + '/game.json', game)
    if strong:
        chars = read_json(g + '/data/characters.json')
        kell = chars['characters'][0]
        kell['ranks'] = {'sword': 6}
        kell['stats'] = {'str': 4}
        write_json(g + '/data/characters.json', chars)
    return g


def run(game, map_id, inp, shot=None, frames=None, interval=4):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', game, '--map', map_id, '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    return p.returncode, p.stdout


def shot(name, game, map_id, inp, **kw):
    path = os.path.join(TMP, name + '.bmp')
    code, log = run(game, map_id, inp, shot=path, **kw)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, code


def frames_at(script, extra):
    return ticks(script) * 4 + extra


ACTION_MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']  # select Kell, stay put: his action menu is open
# Kell's menu: Attack, Promote (the fighter has an offered promotion), Items, Trade (Mara is next to him), Info, Wait.
INFO = ACTION_MENU + ['down'] * 4 + ['confirm', 'wait:3']
ITEMS = ACTION_MENU + ['down'] * 2 + ['confirm', 'wait:3']


def test_details(game, tag):
    code, log = run(game, 'duel', inp(*INFO))
    pages = lines(log, 'unit details:')
    check('%s: Info opens the unit details' % tag, code == 0 and bool(pages), log[-400:])
    titles = pages[-1].split('(')[-1].rstrip(')').split(', ') if pages else []
    check('%s: the pages are Stats and Items' % tag, titles == ['Stats', 'Items'], str(titles))
    script = inp(*INFO)
    img, log, code = shot('details_stats_%s' % tag, game, 'duel', script, frames=frames_at(script, 20))
    box = (0, 0, img.w, img.h)
    check('%s: the Stats page draws (labels, ranks)' % tag, code == 0 and img.count((150, 158, 190), 10, box) > 150)
    script = inp(*INFO, 'right', 'wait:2')
    img, log, code = shot('details_items_%s' % tag, game, 'duel', script, frames=frames_at(script, 20))
    check('%s: the Items page greys the war sword and gives the reason' % tag,
          img.count(GREYED, 10) > 20 and img.count(RED, 10) > 20, 'greyed=%d red=%d' % (img.count(GREYED, 10), img.count(RED, 10)))


def test_item_screens(game):
    code, log = run(game, 'duel', inp(*ITEMS))
    items = lines(log, 'items:')
    check('the Items screen lists the war sword with its reason', bool(items) and 'War Sword [Needs rank B]' in items[-1]
          and 'Steel Sword,' in items[-1], str(items[-1:]))
    script = inp(*ITEMS, 'down', 'down', 'confirm', 'wait:3')  # the war sword's choice menu
    code, log = run(game, 'duel', script)
    menu = lines(log, 'item menu:')
    check('its Equip is greyed with the reason', bool(menu) and 'Equip [Needs rank B]' in menu[-1], str(menu[-1:]))
    img, log, code = shot('item_choice', game, 'duel', script, frames=frames_at(script, 20))
    check('the item screen with the greyed weapon draws', code == 0 and img.count(GREYED, 10) > 20)
    script = inp(*ITEMS, 'down', 'confirm', 'wait:3')  # the steel sword: rank D is enough
    code, log = run(game, 'duel', script)
    menu = lines(log, 'item menu:')
    check('a weapon the rank allows can be equipped', bool(menu) and 'Equip' in menu[-1] and '[' not in menu[-1].split(' - ', 1)[1], str(menu[-1:]))


def test_rank_up_and_cap(game):
    # Attack: the level-up box (a hit is a level) waits for Confirm; the rank-up label comes after it.
    atk = inp(*ACTION_MENU, 'confirm', 'wait:3', 'confirm', 'wait:60', 'confirm', 'wait:4')
    code, log = run(game, 'duel', atk + ',wait:60')
    ups = lines(log, 'rank up:')
    check('a rank-up is logged', any('Kell - Sword rank C' in s for s in ups), str(ups) + log[-300:])
    check('no event is left unplayed', not lines(log, 'not shown yet'), log[-400:])
    img, log, code = shot('rank_up_label', game, 'duel', atk, frames=frames_at(atk, 8))
    check('"Sword rank C" floats over the unit', code == 0 and img.count(GOLD, 20, (0, 0, 352, 259)) > 60,
          str(img.count(GOLD, 20, (0, 0, 352, 259))))
    box = lines(log, 'level-up box:')
    check('a level-up at the cap says so', any('Strength: max' in s for s in box), str(box) + log[-300:])
    lvl = inp(*ACTION_MENU, 'confirm', 'wait:3', 'confirm')
    img, log, code = shot('level_up_cap', game, 'duel', lvl, frames=frames_at(lvl, 110))
    check('the level-up box is drawn', img.count(LEVEL_FRAME, 10) > 100)


def test_plain_game():
    code, log = run('plain', 'bridge', inp(IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3'))
    check('a game without ranks lists no reasons', code == 0 and not lines(log, 'rank up:'), log[-300:])


def main():
    try:
        make_game('ranks', 480, 270)
        make_game('strong', 480, 270, strong=True)
        make_game('phone', 270, 480)
        test_details('ranks', 'desktop')
        test_details('phone', 'phone')
        test_details('strong', 'at_cap')  # Strength 10 of 10: MAX; sword experience 6 of the 7 that rank C needs
        test_item_screens('ranks')
        test_rank_up_and_cap('strong')
    finally:
        if not SHOTS:
            shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
