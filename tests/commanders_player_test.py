#!/usr/bin/env python3
"""Commanders in the real player, windowless (docs/COMMANDERS.md "Player").

  python3 tests/commanders_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays copies of the synthetic game tests/fixtures/commanders (not a demo): commanders ada, bram, cato; factions order (ada, cato), horde (bram), free (any). Map "pick"
leaves both sides to "choose" (faction and commander). The copy used here gives Ada a meter that one blow fills and a cheap one-turn power, so the script can play the whole
loop in a few inputs. Judged from the [info] lines the player logs, from the saved game and from the screenshots:

  - A person's side gets the faction screen, then the commander screen, which lists only the commanders of the picked faction (at UI scale 100 and 150 on the 480x270
    canvas); the pick goes into the battle; Cancel leaves; a computer side is never asked; --autoplay asks nobody.
  - The meter shows in the side panel (a bar with a tick per power); a blow fills it; the map menu lists "Power: <name>" (greyed with what is missing while the meter is short,
    with the price when it can be used); using one shows the banner, pays the meter, and the power ends when its turns are up.
  - The Continue save carries the commander, the meter and the active power; a restored battle plays on and the power ends on time; a save naming a commander the game lacks is
    refused.
  - A game without data/commanders.json never shows the screen or an entry.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check, player_cmd, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:40'  # the turn banner plays first; it takes the first inputs otherwise
# Faction screen: order, horde, free; picking order (first row) leaves ada and cato for the commander screen.
PICK_ORDER_ADA = 'wait:3,confirm,wait:3,confirm,wait:3,'
# The soldier starts under the cursor at (2,1): select it, walk two squares to (4,1), Attack (the first entry), the first target.
HIT = 'confirm,wait:3,right,right,confirm,wait:3,confirm,wait:3,confirm,wait:30'
MENU_POWER = 'menu,wait:2,down,confirm,wait:3'  # the map menu: End Turn first, then the powers
BANNER_OVER = 'wait:60'  # the power's banner takes the inputs while it is up (as any animation: Confirm skips it)
LEAVE = 'menu,wait:2,down,down,down,down,confirm,wait:5'  # "Leave battle": writes the Continue save as things stand
END_TURN = 'menu,wait:2,confirm,wait:2'  # (the game stands still while a scripted wait runs: the computer's turn plays in the frames after the script, see `tail`)


def write_json(path, doc):
    with open(path, 'w') as f:
        json.dump(doc, f)


def make_games():
    shutil.rmtree(GAMES, ignore_errors=True)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'commanders'), os.path.join(GAMES, 'cmd'))
    gpath = os.path.join(GAMES, 'cmd', 'game.json')
    game = json.load(open(gpath))
    game['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d', 'background': '#161a24'}]  # for the full 3D look
    write_json(gpath, game)
    path = os.path.join(GAMES, 'cmd', 'data', 'commanders.json')
    doc = json.load(open(path))
    ada = next(c for c in doc['commanders'] if c['id'] == 'ada')
    ada['meter'] = {'max': 100, 'perDamageDealt': 50}  # one blow fills it
    ada['powers'] = [{'id': 'rally', 'name': 'Rally', 'cost': 40, 'modifiers': ['frenzy'], 'effects': [{'heal': 5}]},
                     {'id': 'surge', 'name': 'Surge', 'cost': 90, 'modifiers': ['fleet']}]
    write_json(path, doc)
    # The same game without the commanders file and the keys that name them: nothing should ever show the screen.
    shutil.copytree(os.path.join(GAMES, 'cmd'), os.path.join(GAMES, 'cmd_none'))
    os.remove(os.path.join(GAMES, 'cmd_none', 'data', 'commanders.json'))
    fpath = os.path.join(GAMES, 'cmd_none', 'data', 'factions.json')
    f = json.load(open(fpath))
    for x in f['factions']:
        x.pop('commanders', None)
    write_json(fpath, f)
    for name in ('field', 'pick', 'duel', 'plain'):
        mpath = os.path.join(GAMES, 'cmd_none', 'maps', name + '.json')
        m = json.load(open(mpath))
        for s in m['sides']:
            s.pop('commander', None)
        write_json(mpath, m)


def play(game, map_, inp, name, scale=None, saves=None, extra=(), tail=80):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = player_cmd(PLAYER, GAMES, REPO) + ['--frames', str(ticks(inp) * 4 + tail), '--input-interval', '4', '--fixed-dt', '0.016',
                                              '--game', game, '--3d', 'software', '--input', inp, '--screenshot', shot]
    if map_:
        cmd += ['--map', map_]
    if saves:
        cmd += ['--saves', saves]
    else:
        cmd += ['--no-saving']
    if scale:
        cmd += ['--ui-scale', str(scale)]
    cmd += list(extra)
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'cmd_' + name + '.bmp'))
    return log


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def main():
    make_games()

    # ---- the faction screen, then the commander screen of the picked faction, at 100 % and 150 %
    for tag, scale in (('screen', None), ('screen_150', 150)):
        log = play('cmd', 'pick', 'wait:3,confirm,wait:3', tag, scale)
        check('%s: the commander screen follows the faction pick and lists the faction\'s two commanders' % tag,
              lines(log, 'commander choice: Blue picks') == ['commander choice: Blue picks from 2 commanders'], lines(log, 'commander choice'))
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])
    log = play('cmd', 'pick', 'wait:3,down,confirm,wait:3', 'screen_horde')
    check('horde: its list has only Bram', lines(log, 'commander choice: Blue picks') == ['commander choice: Blue picks from 1 commanders'], lines(log, 'commander choice'))

    # ---- a pick goes into the battle; a blow fills the meter; the map menu lists the powers
    for tag, scale in (('meter', None), ('meter_150', 150)):
        log = play('cmd', 'pick', PICK_ORDER_ADA + IDLE + ',' + HIT, tag, scale)
        check('%s: Blue picked Ada, and it was written into the battle' % tag,
              lines(log, 'commander choice: Blue picked') == ['commander choice: Blue picked "ada"'] and lines(log, 'commander: side "blue" plays ada') != [],
              lines(log, 'commander'))
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])
    log = play('cmd', 'pick', PICK_ORDER_ADA + IDLE + ',' + HIT + ',' + 'menu,wait:3', 'menu')
    pe = lines(log, 'power entries:')
    check('menu: one entry per power; the meter is full so both can be used, Surge is dearer',
          bool(pe) and pe[-1] == 'power entries: Power: Rally (40); Power: Surge (90) (meter 100/100)', pe)
    log = play('cmd', 'pick', PICK_ORDER_ADA + IDLE + ',menu,wait:3', 'menu_short')
    pe = lines(log, 'power entries:')
    check('menu: with an empty meter both are greyed and say what is missing',
          bool(pe) and pe[-1] == 'power entries: Power: Rally (needs 40 more) [grey]; Power: Surge (needs 90 more) [grey] (meter 0/100)', pe)

    # ---- using a power: the banner, the meter paid, the power active, ended when its two turns are up
    use = PICK_ORDER_ADA + IDLE + ',' + HIT + ',' + MENU_POWER
    for tag, scale in (('use', None), ('use_150', 150)):
        log = play('cmd', 'pick', use, tag, scale, tail=40)  # a shorter tail: the banner is still up in the picture
        check('%s: the banner names the commander and the power' % tag, lines(log, 'power used:') == ['power used: Ada - Rally (cost 40, meter now 60)'], lines(log, 'power used'))
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])
    log = play('cmd', 'pick', use + ',' + BANNER_OVER, 'after', tail=80)
    check('after: the panel shows the active power', clean(log), log[-300:])
    log = play('cmd', 'pick', use, 'use_3d', extra=['--style', 'full-3d'], tail=40)
    check('use_3d: the power can be used in the full 3D view too', lines(log, 'power used:') == ['power used: Ada - Rally (cost 40, meter now 60)'] and clean(log), log[-300:])
    log = play('cmd', 'pick', use + ',' + BANNER_OVER + ',' + END_TURN, 'ends', tail=1600)  # the computer plays its turn in the tail, then blue's next turn starts
    check('a power of one turn ends when the side\'s next turn starts',
          lines(log, 'power ended:') == ['power ended: Ada - Rally'], lines(log, 'power ended'))

    # ---- the computer side is never asked, --autoplay asks nobody
    log = play('cmd', 'pick', 'wait:60', 'auto', extra=['--autoplay'])
    check('autoplay: no commander screen at all', not lines(log, 'commander choice:') and bool(lines(log, 'ai: Blue acts')), log[-300:])

    # ---- Cancel on the screen leaves it, nothing starts
    log = play('cmd', 'pick', 'wait:3,confirm,wait:3,cancel,wait:5', 'cancel')
    check('cancel: the screen was left and no battle began', bool(lines(log, 'commander choice: Blue picks')) and not lines(log, 'turn:'), log[-300:])

    # ---- a game without commanders never shows the screen or an entry
    log = play('cmd_none', 'pick', 'wait:3,confirm,wait:3,' + IDLE + ',menu,wait:3', 'none')
    check('no commanders file: no screen, no entries, the battle starts',
          not lines(log, 'commander choice:') and not lines(log, 'power entries:') and bool(lines(log, 'turn: Blue')), log[-300:])

    # ---- the enemy's meter is in the panel too (a row under the own meter), at both scales, and it changes when the enemy deals damage (the counter)
    for tag, scale in (('enemy', None), ('enemy_150', 150)):
        log = play('cmd', 'field', IDLE + ',' + HIT, tag, scale)
        om = lines(log, 'other meter:')
        check('%s: the enemy commander\'s meter row is there at 0 and has grown after its counter-attack' % tag,
              len(om) >= 2 and om[0].startswith('other meter: Bram 0/60') and om[-1] != om[0] and om[-1].startswith('other meter: Bram ') and clean(log), om)
        # At 150 the block is the bar alone, so that the unit's weapon line still fits; at 100 it has its name row.
        check('%s: the enemy meter is %s' % (tag, 'the bar alone' if scale else 'a full row'),
              bool(om) and all(l.endswith('(bar only)') == bool(scale) for l in om), om)

    # ---- a fixed commander (map "field"): no screen; the panel and the entries are there from the start
    log = play('cmd', 'field', IDLE + ',menu,wait:3', 'field')
    check('field: no commander screen for a side whose commander the map names',
          not lines(log, 'commander choice:') and lines(log, 'power entries:')[-1].endswith('(meter 0/100)'), lines(log, 'power entries'))

    # ---- the save carries the commander, the meter and the active power; a restored battle plays on; a commander the game lacks is refused
    sv = os.path.join(TMP, 'saves')
    shutil.rmtree(sv, ignore_errors=True)
    log = play('cmd', 'pick', use + ',' + BANNER_OVER + ',' + LEAVE, 'save', saves=sv)
    path = os.path.join(sv, 'cmd', 'continue.json')
    save = json.load(open(path)) if os.path.exists(path) else None
    sides = save['battle']['sides'] if save else []
    mine = sides[0].get('commander', {}) if sides else {}
    check('save: the Continue save has Ada, a meter of 60, and Rally still active with one turn left (Leave battle writes the save as things stand)',
          mine.get('id') == 'ada' and mine.get('meter') == 60 and mine.get('power') == 0 and mine.get('left') == 1 and mine.get('uses') == [1, 0], mine)
    log = play('cmd', None, 'wait:40,menu,wait:3', 'resume', saves=sv, extra=['--continue'])
    pe = lines(log, 'power entries:')
    check('resume: the battle comes back with the power active (both entries say so)',
          bool(pe) and pe[-1] == 'power entries: Power: Rally (active) [grey]; Power: Surge (active) [grey] (meter 60/100)' and clean(log), (pe, log[-300:]))
    log = play('cmd', None, 'wait:40,' + END_TURN, 'resume_end', saves=sv, extra=['--continue'], tail=1600)
    check('resume: the power ends on time in the restored battle', lines(log, 'power ended:') == ['power ended: Ada - Rally'], lines(log, 'power ended'))
    if save:
        save['battle']['sides'][0]['commander']['id'] = 'zed'
        write_json(path, save)
    log = play('cmd', None, 'wait:30', 'refused', saves=sv, extra=['--continue'])
    check('refused: a save naming an unknown commander is not loaded', 'names unknown commander "zed"' in log, log[-500:])

    summary()
    if not SHOTS:
        shutil.rmtree(TMP, ignore_errors=True)


if __name__ == '__main__':
    main()
