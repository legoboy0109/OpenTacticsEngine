#!/usr/bin/env python3
"""Damage types in the real player, windowless (docs/SRPG.md "Damage types").

  python3 tests/damage_types_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/damage-types (not a demo; every fighter has strength 6 and defense 2, every weapon might 4,
damage = max(1, str + might - def - tb.def - rs)). Judged from the [info] lines the player logs and from the screenshots:

  - The forecast of the gunner's kinetic Slasher into the plating (kinetic resistance +3, standing in woods): its column names the weapon's type with
    the defender's resistance ("Kinetic  res +3"), the plating's column names its own type without one (the gunner resists nothing), damage 3 / 8.
  - The same forecast at UI scale 150 on the 480x270 canvas still draws (no UI bug) and its text fits the box.
  - The fight does what the forecast said (the gunner is down 8 hit points).
  - The unit details of the plating list "Kinetic res +3, EMF res -2, Fire res +1" (a unit without resistance lists none).
  - The same game without damage types (the keys stripped) shows no type and no resistance anywhere.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
SELECT = 'confirm,confirm'  # select the unit under the cursor: its action menu opens (Attack first)


def make_games():
    shutil.rmtree(GAMES, ignore_errors=True)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'damage-types'), os.path.join(GAMES, 'dt'))
    shutil.copytree(os.path.join(GAMES, 'dt'), os.path.join(GAMES, 'plain'))
    # The control: the same game with every damage-type key taken out.
    p = os.path.join(GAMES, 'plain')
    for rel, keys in (('game.json', ('damageTypes', 'resistRange')), ('data/weapons.json', ('damageType',)), ('data/units.json', ('resist',)),
                      ('data/characters.json', ('resist',))):
        j = json.load(open(os.path.join(p, rel)))
        for entry in ([j] if rel == 'game.json' else list(j.values())[0]):
            for k in keys:
                entry.pop(k, None)
        json.dump(j, open(os.path.join(p, rel), 'w'))
    m = json.load(open(os.path.join(p, 'maps', 'field.json')))
    for u in m['units']:
        u.pop('resist', None)
    json.dump(m, open(os.path.join(p, 'maps', 'field.json'), 'w'))
    # The skill that tests a type goes too (the plain game has no types to test).
    json.dump({'skills': []}, open(os.path.join(p, 'data', 'skills.json'), 'w'))
    u = json.load(open(os.path.join(p, 'data', 'units.json')))
    for t in u['units']:
        t.pop('skills', None)
    json.dump(u, open(os.path.join(p, 'data', 'units.json'), 'w'))


def play(game, inp, name, scale=None, saves=False):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', game, '--map', 'field', '--3d', 'software', '--no-settings', '--input', inp, '--screenshot', shot]
    sdir = os.path.join(TMP, 'saves_' + name)
    if saves:
        shutil.rmtree(sdir, ignore_errors=True)
        cmd += ['--saves', sdir]
    else:
        cmd += ['--no-saving']
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'dt_' + name + '.bmp'))
    save = None
    if saves and os.path.exists(os.path.join(sdir, game, 'continue.json')):
        save = json.load(open(os.path.join(sdir, game, 'continue.json')))
    return log, save


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def hp_of(save, story_id):
    for u in save['battle']['units']:
        if u.get('id') == story_id:
            return u['hp']
    return None


def main():
    make_games()
    attack = IDLE + ',' + SELECT + ',confirm,wait:6'  # the first target in reach is the plating (1,1)

    # ---- the forecast, at the default and at 150 %
    for tag, scale in (('forecast', None), ('forecast_150', 150)):
        log, _ = play('dt', attack, tag, scale)
        f = lines(log, 'forecast:')
        # One line "Kinetic  res +3" when the column is wide enough, else the resistance on its own line ("Kinetic; res +3"): never cut off.
        check('%s: the forecast names the type and the resistance' % tag, bool(f) and ('Kinetic  res +3' in f[-1] or 'Kinetic; res +3' in f[-1]), f)
        check('%s: it is one line at the default size and two at 150 %%' % tag, bool(f) and (('Kinetic  res +3' in f[-1]) == (scale is None)), f)
        check('%s: the defender names its own type without a resistance' % tag, bool(f) and '| Plating; Slasher; Kinetic; ' in f[-1], f)
        check('%s: damage 3 down, 8 back' % tag, bool(f) and 'Dmg 3;' in f[-1] and 'Dmg 8;' in f[-1], f)
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])

    # ---- the fight does what the forecast said
    log, save = play('dt', IDLE + ',' + SELECT + ',confirm,confirm,wait:40', 'fight', saves=True)
    check('the fight: the gunner took the answer (40 -> 32)', save is not None and hp_of(save, 'gunner') == 32, hp_of(save, 'gunner') if save else 'no save')
    check('the fight: the plating took the forecast damage (40 -> 37)', save is not None and hp_of(save, 'wall') == 37, hp_of(save, 'wall') if save else 'no save')

    # ---- the unit details of the plating (cursor right of the gunner), and of a unit without resistances
    log, _ = play('dt', IDLE + ',right,menu,down,confirm,wait:4', 'info')
    check('the unit details list the non-zero resistances', 'unit resist: Plating: Kinetic res +3, EMF res -2, Fire res +1' in lines(log, 'unit resist:'),
          lines(log, 'unit resist:'))
    check('details: no warnings', clean(log), log[-300:])
    log, _ = play('dt', IDLE + ',menu,down,confirm,wait:4', 'info_none')
    check('a unit without resistances lists none', 'unit resist: Gunner: ' in lines(log, 'unit resist:'), lines(log, 'unit resist:'))
    log, _ = play('dt', IDLE + ',right,menu,down,confirm,wait:4', 'info_150', 150)
    check('details at UI scale 150: listed and no UI bug', bool(lines(log, 'unit resist: Plating')) and clean(log), log[-300:])

    # ---- the control: no damage types, nothing of it is shown
    log, _ = play('plain', attack, 'plain_forecast')
    f = lines(log, 'forecast:')
    check('plain game: the forecast names no type and no resistance', bool(f) and 'Kinetic' not in f[-1] and ' res ' not in f[-1], f)
    log, _ = play('plain', IDLE + ',right,menu,down,confirm,wait:4', 'plain_info')
    check('plain game: the unit details log no resistances', not lines(log, 'unit resist:') and bool(lines(log, 'unit details:')), lines(log, 'unit details:'))

    summary()


if __name__ == '__main__':
    main()
