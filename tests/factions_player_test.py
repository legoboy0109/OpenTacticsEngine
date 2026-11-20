#!/usr/bin/env python3
"""Factions in the real player, windowless (docs/ECONOMY.md "Factions").

  python3 tests/factions_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays copies of the synthetic game tests/fixtures/factions (not a demo): three factions (order, horde, free). Map "pick" leaves both sides to
"choose"; map "field" names the factions. Judged from the [info] lines the player logs, from the saved game and from the screenshots:

  - A person's side whose faction is "choose" gets the faction screen before the battle (at UI scale 100 and 150 on the 480x270 canvas);
    the pick goes into the battle; Cancel leaves; a computer side is never asked; --autoplay asks nobody.
  - The build menu greys the units the picked faction may not build, with the reason; the research panel and the upgrade menu say it too.
  - The Continue save carries the side's faction, a restored battle plays on, and a save naming a faction the game lacks is refused.
  - A game without data/factions.json never shows the screen.
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
# Cursor squares of map "field" / "pick": it starts on the soldier (2,1); blue's barracks stand at (0,0).
TO_BARRACKS = 'left,left,up'
# The battle menu: End Turn, Unit (the game has skills), Research, ...
OPEN_RESEARCH = 'menu,down,down,confirm,wait:3'


def write_json(path, doc):
    with open(path, 'w') as f:
        json.dump(doc, f)


def make_games():
    shutil.rmtree(GAMES, ignore_errors=True)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'factions'), os.path.join(GAMES, 'fac'))
    # The same game with the factions file taken away and the maps' keys with it: nothing should ever show the screen.
    shutil.copytree(os.path.join(GAMES, 'fac'), os.path.join(GAMES, 'fac_none'))
    os.remove(os.path.join(GAMES, 'fac_none', 'data', 'factions.json'))
    for name in ('field', 'pick', 'duel'):
        path = os.path.join(GAMES, 'fac_none', 'maps', name + '.json')
        m = json.load(open(path))
        for s in m['sides']:
            s.pop('faction', None)
        write_json(path, m)


def play(game, map_, inp, name, scale=None, saves=None, extra=()):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = player_cmd(PLAYER, GAMES, REPO) + ['--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
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
        shutil.copy(shot, os.path.join(SHOTS, 'fac_' + name + '.bmp'))
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

    # ---- the faction screen, at 100 % and 150 %: only the person's side is asked
    for tag, scale in (('screen', None), ('screen_150', 150)):
        log = play('fac', 'pick', 'wait:2', tag, scale)
        check('%s: the screen asks for the person side (Blue) and lists three factions' % tag,
              lines(log, 'faction choice:') == ['faction choice: Blue picks from 3 factions'], lines(log, 'faction choice:'))
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])

    # ---- a pick goes into the battle; the build menu greys what the faction may not build
    pick_horde = 'wait:3,down,confirm,wait:3'
    menu = pick_horde + ',' + IDLE + ',' + TO_BARRACKS + ',confirm,wait:3,confirm,wait:3'  # structure menu -> Build
    for tag, scale in (('build', None), ('build_150', 150)):
        log = play('fac', 'pick', menu, tag, scale)
        check('%s: Blue picked horde, and it was written into the battle' % tag,
              lines(log, 'faction choice: Blue picked') == ['faction choice: Blue picked "horde"'] and lines(log, 'faction: side "blue" plays horde') != [],
              lines(log, 'faction'))
        bm = lines(log, 'build menu:')
        check('%s: the build menu greys the archer and knight with the reason' % tag,
              bool(bm) and bm[0] == 'build menu: Soldier (10 Gold); Archer (not available to Horde) [greyed]; Knight (not available to Horde) [greyed]; Golem (25 Gold)',
              bm)
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])

    # ---- a fixed faction (map "field"): no screen; the research panel and the upgrade menu give the reason too
    log = play('fac', 'field', IDLE + ',' + OPEN_RESEARCH, 'research')
    check('field: no faction screen for a side whose faction the map names', not lines(log, 'faction choice:'), lines(log, 'faction choice:'))
    rows = lines(log, 'research panel:')
    why = lines(log, 'research reasons:')
    check('research: Arcana is greyed for order, Drill and Forging are not (Forging waits for Drill)',
          rows == ['research panel: Drill (20 Gold, 1 turn); Forging (30 Gold, 2 turns) [greyed]; Arcana (25 Gold, 1 turn) [greyed]'], rows)
    check('research: the reasons', why == ['research reasons: Forging: needs Drill; Arcana: not available to Order'], why)
    log = play('fac', 'field', IDLE + ',' + TO_BARRACKS + ',confirm,wait:3,down,confirm,wait:3', 'upgrade')
    um = lines(log, 'upgrade menu:')
    check('upgrade: the keep is greyed for order, the fort is not',
          um == ['upgrade menu: Fort (30 Gold); Keep (not available to Order) [greyed]'], um)
    check('field: no UI bug or warning', clean(log), log[-400:])

    # ---- the computer side is never asked, --autoplay asks nobody
    log = play('fac', 'pick', 'wait:60', 'auto', extra=['--autoplay'])
    check('autoplay: no faction screen at all', not lines(log, 'faction choice:') and bool(lines(log, 'ai: Blue acts')), log[-300:])

    # ---- Cancel on the screen leaves it (back to the game page), nothing starts
    log = play('fac', 'pick', 'wait:3,cancel,wait:5', 'cancel')
    check('cancel: the screen was left and no battle began', bool(lines(log, 'faction choice: Blue picks')) and not lines(log, 'turn:'), log[-300:])

    # ---- a game without factions never shows the screen
    log = play('fac_none', 'pick', IDLE, 'none')
    check('no factions file: no screen, the battle starts', not lines(log, 'faction choice:') and bool(lines(log, 'turn: Blue')), log[-300:])

    # ---- the save carries the faction; a restored battle plays on; a faction the game lacks is refused
    sv = os.path.join(TMP, 'saves')
    shutil.rmtree(sv, ignore_errors=True)
    log = play('fac', 'pick', pick_horde + ',' + IDLE + ',menu,confirm,wait:60', 'save', saves=sv)  # End Turn writes the Continue save
    path = os.path.join(sv, 'fac', 'continue.json')
    save = json.load(open(path)) if os.path.exists(path) else None
    sides = save['battle']['sides'] if save else []
    check('save: the side\'s faction is in the Continue save, the computer side\'s too (from the seed)',
          len(sides) == 2 and sides[0].get('faction') == 'horde' and sides[1].get('faction') in ('order', 'horde', 'free'), sides)
    log = play('fac', None, 'wait:30', 'resume', saves=sv, extra=['--continue'])
    check('resume: the battle comes back and plays on without a warning', bool(lines(log, 'turn:')) and clean(log), log[-400:])
    if save:
        save['battle']['sides'][0]['faction'] = 'elves'
        write_json(path, save)
    log = play('fac', None, 'wait:30', 'refused', saves=sv, extra=['--continue'])
    check('refused: a save naming an unknown faction is not loaded', 'names unknown faction "elves"' in log, log[-500:])

    summary()
    if not SHOTS:
        shutil.rmtree(TMP, ignore_errors=True)


if __name__ == '__main__':
    main()
