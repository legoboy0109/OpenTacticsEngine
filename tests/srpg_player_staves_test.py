#!/usr/bin/env python3
"""The player and staves (docs/SRPG.md section 8), windowless.

  python3 tests/srpg_player_staves_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_staves_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-staves into a temp folder (two window sizes: 480x270 and a 270x480 phone), hurts some units by
editing a Continue save (the maps cannot place a wounded unit), runs the player headless with --input scripts and reads the
player's log lines and the pixels of the screenshots:

  - the action menu has a Staff entry, greyed with the reason when nothing can be used; the list shows each staff with its uses
    and, greyed, the reason a staff cannot be used (rank, character lock, nobody to help);
  - picking a target shows the range (blue) and the forecast box (what each unit gets, the hit chance, "Breaks");
  - using a staff heals (the saved HP changes), spends a use, gives experience; an area staff heals every hurt unit in it.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)
Image = Bmp  # (the name this script uses)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-staves-')

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
BLUE = (60, 120, 230)  # the reach mask is drawn in blue, the area in red (the pixel counts below use these loosely)


def make_game(gid, width, height, tweak=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-staves'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def hurt_save(gid, hp):
    """A Continue save for the game (`hp`: story id -> hit points), made by letting the player write one and editing it."""
    saves = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(saves, ignore_errors=True)
    subprocess.run(player_cmd(PLAYER, TMP, REPO) + ['--saves', saves, '--frames', '120', '--fixed-dt', '0.016', '--game', gid, '--map', 'clinic', '--input', 'wait:20'],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    path = os.path.join(saves, gid, 'continue.json')
    doc = read_json(path)
    for u in doc['battle']['units']:
        if u.get('id') in hp:
            u['hp'] = hp[u['id']]
    write_json(path, doc)
    return saves


def run(gid, saves, inp, shot=None, frames=None, interval=4):
    """Runs the player on a copy of the hurt Continue save; returns (code, log, the units of the save written afterwards)."""
    work = os.path.join(TMP, 'run_%s' % gid)
    shutil.rmtree(work, ignore_errors=True)
    shutil.copytree(saves, work)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--continue', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', 'clinic', '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    units = {}
    try:
        for u in read_json(os.path.join(work, gid, 'continue.json'))['battle']['units']:
            units[u.get('id') or u['name']] = u
    except (OSError, ValueError, KeyError):
        pass
    return p.returncode, p.stdout, units


def shot(name, gid, saves, inp, extra=20):
    path = os.path.join(TMP, name + '.bmp')
    code, log, units = run(gid, saves, inp, shot=path, frames=ticks(inp) * 4 + extra)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, units, code


# Wen is the first unit under the cursor: confirm selects her, confirm again stays put and opens her action menu
# (Staff, Items, Trade, Info, Wait).
MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']
LIST = MENU + ['confirm', 'wait:3']             # the Staff list
MEND = LIST + ['confirm', 'wait:3']             # the Mend Staff chosen: picking a unit
HURT = {'tarn': 5, 'ola': 5, 'dax': 15, 'brae': 9}


def test_menu_and_list(gid, tag):
    saves = hurt_save(gid, HURT)
    code, log, units = run(gid, saves, inp(*MENU))
    entry = lines(log, 'staff entry of Wen')
    check('%s: Wen has a usable Staff entry in her action menu' % tag, bool(entry) and entry[-1].endswith('usable'), str(entry[-1:]))
    code, log, units = run(gid, saves, inp(*LIST))
    lst = lines(log, 'staff list:')
    want = 'Mend Staff 6/6, Rally Staff 3/3, High Staff [Needs rank B], Kin Staff 5/5, Chorus Staff 2/2'
    check('%s: the list shows uses left and the reason a staff cannot be used' % tag, bool(lst) and lst[-1].endswith(want), str(lst[-1:]))
    img, log, units, code = shot('staff_list_%s' % tag, gid, saves, inp(*LIST))
    check('%s: the list draws' % tag, code == 0)
    return saves


def test_pick_and_heal(gid, saves, tag):
    img, log, units, code = shot('staff_pick_%s' % tag, gid, saves, inp(*MEND))
    chosen = lines(log, 'staff chosen:')
    check('%s: choosing the Mend Staff starts a pick' % tag, bool(chosen) and 'Mend Staff by Wen' in chosen[-1] and 'reach 1-3' in chosen[-1], str(chosen[-1:]))
    fc = lines(log, 'staff forecast:')
    # The candidates in unit order: tarn, ola, dax (brae at 9 of 30 is 2 away too).
    check('%s: the forecast box says what the unit would get' % tag,
          bool(fc) and 'Mend Staff 6;' in fc[-1] and 'HP 5 > 13' in fc[-1] and 'Exp 15' in fc[-1], str(fc[-1:]))
    check('%s: the reach is drawn' % tag, code == 0 and img.count((70, 90, 170), 40) > 50)
    # Confirm: Tarn is healed by 8, one use is spent, Wen has her experience.
    img, log, units, code = shot('staff_result_%s' % tag, gid, saves, inp(*MEND, 'confirm', 'wait:7'), extra=6)
    check('%s: the result draws while the heal plays' % tag, code == 0)
    img, log, units, code = shot('staff_healed_%s' % tag, gid, saves, inp(*MEND, 'confirm', 'wait:20'))
    check('%s: the use is logged' % tag, bool(lines(log, 'staff: Wen uses Mend Staff')), log[-400:])
    check('%s: Tarn was healed by 8' % tag, units.get('tarn', {}).get('hp') == 13, str(units.get('tarn')))
    wen = units.get('wen', {})
    check('%s: a use was spent and she earned her experience' % tag,
          wen.get('items', [{}])[0].get('uses') == 5 and wen.get('xp') == 15 and wen.get('done') is True, str(wen))
    check('%s: the result draws' % tag, code == 0)


def test_area_and_reasons(gid, saves, tag):
    # Rally Staff (second in the list): Wen is rank C, so it can be used. Pick Tarn: Dax, next to him, is hurt too.
    rally = LIST + ['down', 'confirm', 'wait:3']
    img, log, units, code = shot('staff_area_%s' % tag, gid, saves, inp(*rally))
    fc = lines(log, 'staff forecast:')
    check('%s: the area forecast lists every unit it would heal' % tag,
          bool(fc) and 'Rally Staff 3;' in fc[-1] and 'Tarn  HP 5 > 11;' in fc[-1] and 'Dax  HP 15 > 21;' in fc[-1], str(fc[-1:]))
    img, log, units, code = shot('staff_area_done_%s' % tag, gid, saves, inp(*rally, 'confirm', 'wait:20'))
    check('%s: an area heals each hurt unit in it' % tag,
          units.get('tarn', {}).get('hp') == 11 and units.get('dax', {}).get('hp') == 21 and units.get('brae', {}).get('hp') == 9,
          str({k: units.get(k, {}).get('hp') for k in ('tarn', 'dax', 'brae')}))


def test_extras(gid, tag):
    """A staff that can miss and one with a single use: the forecast says "Hit 60%" and "Breaks"."""
    saves = hurt_save(gid, {'tarn': 5})
    fortune = LIST + ['confirm', 'wait:3']
    img, log, units, code = shot('staff_hit_%s' % tag, gid, saves, inp(*fortune))
    fc = lines(log, 'staff forecast:')
    check('%s: the forecast of a staff that can miss says so' % tag,
          bool(fc) and fc[-1].endswith('Fortune Staff 3; Tarn  HP 5 > 24; Hit 60%; Exp 10; '), str(fc[-1:]))
    img, log, units, code = shot('staff_breaks_%s' % tag, gid, saves, inp(*LIST, 'down', 'confirm', 'wait:3'))
    fc = lines(log, 'staff forecast:')
    check('%s: the forecast of the last use says it breaks' % tag,
          bool(fc) and fc[-1].endswith('Last Light 1; Tarn  HP 5 > 15; Exp 5; Breaks; '), str(fc[-1:]))
    code, log, units = run(gid, saves, inp(*LIST, 'down', 'confirm', 'wait:3', 'confirm', 'wait:30'))
    names = [i for i in units.get('wen', {}).get('items', [])]
    check('%s: the staff is gone after its last use' % tag, len(names) == 4 and units.get('tarn', {}).get('hp') == 15, str(units.get('wen')))


def test_items_screen(gid, tag):
    saves = hurt_save(gid, {'tarn': 5})
    code, log, units = run(gid, saves, inp(*MENU, 'down', 'confirm', 'wait:3'))
    items = lines(log, 'items:')
    check('%s: the Items screen greys a staff the unit cannot wield, with the reason' % tag,
          bool(items) and 'High Staff [Needs rank B]' in items[-1] and 'Mend Staff,' in items[-1], str(items[-1:]))
    img, log, units, code = shot('items_staff_%s' % tag, gid, saves, inp(*MENU, 'down', 'confirm', 'wait:3'))
    check('%s: the Items screen draws' % tag, code == 0)


def test_greyed(gid, tag):
    # Mira (a generic cleric) is not the first unit: the cursor has to be moved to her. She carries the kin staff (Wen's), mend and rest.
    saves = hurt_save(gid, {'ola': 5})  # three squares from Mira, so her Mend Staff has somebody
    code, log, units = run(gid, saves, inp(IDLE, 'prev', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3'))
    lst = lines(log, 'staff list:')
    check('%s: a staff locked to another character is greyed with its reason' % tag,
          bool(lst) and lst[-1].endswith('Kin Staff [Only Wen can wield this], Mend Staff 6/6, Rest Staff [Nobody in reach needs it]'), str(lst[-1:]))
    img, log, units, code = shot('staff_list_locked_%s' % tag, gid, saves, inp(IDLE, 'prev', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3'))
    check('%s: the list with a locked and a useless staff draws' % tag, code == 0)


def test_nobody_hurt(gid, tag):
    saves = hurt_save(gid, {})
    code, log, units = run(gid, saves, inp(*MENU))
    entry = lines(log, 'staff entry of Wen')
    check('%s: with nobody hurt the Staff entry is greyed with its reason' % tag,
          bool(entry) and entry[-1].endswith('greyed: Nobody in reach needs it'), str(entry[-1:]))
    img, log, units, code = shot('staff_greyed_%s' % tag, gid, saves, inp(*MENU))
    check('%s: the greyed entry draws' % tag, code == 0)


def test_plain_game():
    shutil.copytree(os.path.join(REPO, 'games/demo-srpg'), os.path.join(TMP, 'plain'))
    p = subprocess.run(player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '80', '--fixed-dt', '0.016', '--game', 'plain', '--map', 'bridge'],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    check('a game without staves plays as before (no staff entry anywhere)', p.returncode == 0 and 'staff' not in p.stdout.lower(), p.stdout[-300:])


def main():
    try:
        make_game('desk', 480, 270)
        make_game('phone', 270, 480)

        def extras(g):
            chars = read_json(g + '/data/characters.json')
            chars['characters'][0]['items'] = ['fortune_staff', 'last_light', 'mend_staff', 'high_staff', 'kin_staff']
            write_json(g + '/data/characters.json', chars)
        make_game('extras', 480, 270, extras)
        make_game('extrasp', 270, 480, extras)
        saves = test_menu_and_list('desk', 'desktop')
        test_pick_and_heal('desk', saves, 'desktop')
        test_area_and_reasons('desk', saves, 'desktop')
        test_greyed('desk', 'desktop')
        test_extras('extras', 'desktop')
        test_items_screen('desk', 'desktop')
        test_nobody_hurt('desk', 'desktop')
        saves = test_menu_and_list('phone', 'phone')
        test_pick_and_heal('phone', saves, 'phone')
        test_area_and_reasons('phone', saves, 'phone')
        test_nobody_hurt('phone', 'phone')
        test_greyed('phone', 'phone')
        test_extras('extrasp', 'phone')
        test_plain_game()
    finally:
        if not SHOTS:
            shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
