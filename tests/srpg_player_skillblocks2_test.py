#!/usr/bin/env python3
"""The player's part of docs/SRPG.md "Skill blocks, third set": the strike number of a fight (items 2), a steal condition that reads the
item (4), a strike that harms its wielder (5) and a light that shrinks (9), windowless.

  python3 tests/srpg_player_skillblocks2_test.py ./build/engine/player/ote-player .

Copies tests/fixtures/srpg-skillblocks2 (a rogue next to a guard) and tests/fixtures/srpg-staves2 ("mist": fog, a sage with staves) into a temp
folder, changes a few files, runs the player headless (--no-audio --no-settings, --saves on a temp folder) with --input scripts, and judges the
[info] lines it logs, the forecast it draws and the Continue save it leaves:

  - Strike number: with a skill that gives the first strike +5, the forecast reads "Dmg 9/4 x2" (each strike's own number) and the fight
    does 9 then 4.
  - Steal: the item list greys the items the maker's formula rules out with the maker's reason ("too heavy"), and one that is allowed can be taken.
  - Recoil: the forecast carries a note for the biting weapon; the wielder loses the HP ("unit hurt"); one with too few HP is defeated.
  - Light: the torch is saved with its shrink; after a turn the radius is smaller; after the next it is gone.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
TMP = tempfile.mkdtemp(prefix='ote-skillblocks2-')
passed, failures = 0, []
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def make_game(gid, fixture, tweak, width=480, height=270):
    """A copy of the fixture with this view size; `tweak(game_dir)` may change its files."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures', fixture), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def edit(g, rel, fn):
    path = os.path.join(g, rel)
    obj = read_json(path)
    fn(obj)
    write_json(path, obj)


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(gid, inp, map_id='field', interval=4):
    """Runs the player on a fresh battle; returns (log, the Continue save's battle with its units by story id)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--saves', work, '--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', map_id, '--input', inp]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    battle, units = {}, {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
        for u in battle['units']:
            units[u.get('id') or u['name']] = u
    except (OSError, ValueError, KeyError):
        pass
    return p.stdout, units, battle


def lines(log, key):
    return [l for l in log.splitlines() if key in l]


def inp(*parts):
    return ','.join(parts)


# The thief (the first unit) is under the cursor: confirm selects it, confirm again opens its action menu.
OPEN = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']
SKILLS = {'skills': [
    {'id': 'opener', 'name': 'Opener', 'blocks': [{'when': 'strike', 'if': [{'strike': 'first'}], 'do': [{'damage': 5}]}]},
    {'id': 'cursed', 'name': 'Cursed Edge', 'blocks': [{'when': 'strike', 'do': [{'recoil': 2}]}]},
    {'id': 'deadly', 'name': 'Deadly Edge', 'blocks': [{'when': 'strike', 'do': [{'recoil': 99}]}]}]}


def only_thief_ability(ability):
    def fn(g):
        edit(g, 'data/units.json', lambda d: d['units'][0].update({'abilities': ability}))
    return fn


def strike_game(g):
    def game(d):
        d['formulas']['strikes'] = '2'
    edit(g, 'game.json', game)
    write_json(g + '/data/skills.json', SKILLS)
    edit(g, 'data/units.json', lambda d: d['units'][0].update({'skills': ['opener']}))


def recoil_game(skill):
    def tweak(g):
        write_json(g + '/data/skills.json', SKILLS)
        edit(g, 'data/weapons.json', lambda d: d['weapons'][0].update({'skills': [skill]}))
    return tweak


def steal_game(g):
    def weapons(d):
        d['weapons'][0]['props']['weight'] = 7
    edit(g, 'data/weapons.json', weapons)
    def items(d):
        for it in d['items']:
            if it['id'] == 'potion':
                it['props'] = {'weight': 1}
    edit(g, 'data/items.json', items)
    edit(g, 'data/abilities.json', lambda d: d['abilities'].append(
        {'id': 'light_hands', 'name': 'Light Hands', 'target': 'enemy',
         'effect': {'steal': {'if': 'it.weight < a.str', 'reason': 'too heavy', 'weapons': True, 'equipped': True}}}))
    only_thief_ability(['light_hands'])(g)


def torch_game(g):
    def items(d):
        d['items'].insert(0, {'id': 'torch_staff', 'name': 'Torch', 'kind': 'staff', 'group': 'staff', 'uses': 3, 'range': 6,
                              'effects': [{'light': {'radius': 4, 'shrink': 2}}]})
    edit(g, 'data/items.json', items)
    def mist(m):
        [u for u in m['units'] if u.get('id') == 'sage'][0]['items'] = ['torch_staff']
    edit(g, 'maps/mist.json', mist)


def test_strike_number(tag, w, h):
    gid = 'strike_' + tag
    make_game(gid, 'srpg-skillblocks2', strike_game, w, h)
    # Attack (the first entry), the foe is the only one in reach: the pick shows the forecast, the next confirm fights.
    log, units, battle = run(gid, inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3'))
    fc = lines(log, 'forecast:')
    check('%s strike number: the forecast shows each strike (Dmg 9/4)' % tag, any('Dmg 9/4' in f for f in fc), str(fc[-2:]))
    log, units, battle = run(gid, inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:40'))
    check('%s strike number: the fight did 9 then 4 (13 in all)' % tag, units.get('mark', {}).get('hp') == 30 - 13, str(units.get('mark')) + log[-300:])


def test_steal_reason(tag, w, h):
    gid = 'steal_' + tag
    make_game(gid, 'srpg-skillblocks2', steal_game, w, h)
    log, units, battle = run(gid, inp(*OPEN, 'down', 'confirm', 'wait:3', 'confirm', 'wait:3'))  # Steal, the foe
    steal = lines(log, 'steal list from')
    check('%s steal: the heavy sword is greyed with the maker\'s reason' % tag, any('Iron Sword [too heavy]' in l for l in steal), str(steal))
    check('%s steal: the unstealable relic keeps its own reason' % tag, any('Relic [cannot be taken]' in l for l in steal), str(steal))
    check('%s steal: the potion is listed without a reason' % tag, any('Potion' in l and 'Potion [' not in l for l in steal), str(steal))
    # The list opens on the first item that can be taken (the potion): confirm takes it.
    log, units, battle = run(gid, inp(*OPEN, 'down', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:30'))
    check('%s steal: the allowed item is taken' % tag, bool(lines(log, 'stole Potion')), log[-300:])


def test_recoil(tag, w, h):
    gid = 'recoil_' + tag
    make_game(gid, 'srpg-skillblocks2', recoil_game('cursed'), w, h)
    log, units, battle = run(gid, inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3'))
    fc = lines(log, 'forecast:')
    check('%s recoil: the forecast carries a note for the biting weapon' % tag, any('Cursed Edge 100%' in f for f in fc), str(fc[-2:]))
    log, units, battle = run(gid, inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:40'))
    check('%s recoil: the wielder is hurt by the weapon' % tag, bool(lines(log, 'unit hurt: Rogue -2')), log[-400:])
    # The thief loses 2 to its weapon and 6 to the guard's answer (5 + 3 - 2); the guard took 4, and its own answer with the same weapon bit it for 2.
    check('%s recoil: hit points after the fight' % tag, units.get('thief', {}).get('hp') == 20 - 2 - 6 and units.get('mark', {}).get('hp') == 30 - 4 - 2,
          str((units.get('thief'), units.get('mark'))))
    gid = 'deadly_' + tag
    make_game(gid, 'srpg-skillblocks2', recoil_game('deadly'), w, h)
    log, units, battle = run(gid, inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:60'))
    check('%s recoil: a wielder with too few hit points is defeated' % tag,
          bool(lines(log, 'unit hurt: Rogue')) and (units.get('thief') is None or not units['thief'].get('alive', True) or units['thief'].get('hp') == 0), str(units.get('thief')) + log[-400:])
    check('%s recoil: the guard it struck is unharmed (it never answered)' % tag, units.get('mark', {}).get('hp', 0) in (26,), str(units.get('mark')))


def test_light(tag, w, h):
    gid = 'torch_' + tag
    make_game(gid, 'srpg-staves2', torch_game, w, h)
    script = inp(IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'prev', 'wait:2')
    log, units, battle = run(gid, inp(script, 'confirm', 'wait:30'), 'mist')
    lit = lines(log, 'staff lit:')
    check('%s light: the torch is lit with its radius and the turns it burns' % tag, any('radius 4 for 2 turns' in l for l in lit), str(lit) + log[-300:])
    lights = battle.get('lights', [])
    check('%s light: the saved light has its shrink' % tag, len(lights) == 1 and lights[0].get('shrink') == 2 and lights[0].get('radius') == 4, str(lights))
    # A turn later: the light has shrunk (End turn through the menu key; red's turn plays by itself).
    log, units, battle = run(gid, inp(script, 'confirm', 'wait:30', 'menu', 'wait:3', 'confirm', 'wait:200'), 'mist')
    lights = battle.get('lights', [])
    check('%s light: after a turn the radius is smaller (4 to 2) and the light has one turn left' % tag,
          len(lights) == 1 and lights[0]['radius'] == 2 and lights[0]['turns'] == 1 and battle.get('round') == 2, str((lights, battle.get('round'))))


def main():
    try:
        for tag, w, h in (('desktop', 480, 270), ('phone', 270, 480)):
            test_strike_number(tag, w, h)
            test_steal_reason(tag, w, h)
            test_recoil(tag, w, h)
            test_light(tag, w, h)
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
