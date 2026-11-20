#!/usr/bin/env python3
"""The player's part of docs/ECONOMY.md (upgrades, research, repair, fuel, ammunition, supply), windowless.

  python3 tests/economy_player_test.py ./build/engine/player/ote-player .
  python3 tests/economy_player_test.py --make-games OUT_DIR .       # only writes the test game
  python3 tests/economy_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots

Writes a small game in a temp folder (never in the repo; games/ has no content for these features yet):
a town that upgrades to a city (and to a fort once Armor is researched), a lab, a factory with all three
entries (Build, Upgrade), research from the battle menu (no building needed; a second copy of the game sets
rules.research.needsStructure), two technologies (Armor unlocks the tank; Field Training needs Armor and gives
infantry +40 HP), repair on own structures, a tank with fuel and a limited cannon, and a supply truck.
Then runs the player with --input scripts and --saves on a temp folder, and checks the [info] lines it logs
for the new menus, panels and events, plus the Continue save for what was paid and what was resumed.
Pictures of the panels are looked at by hand (HANDOFF.md); here one screenshot comparison checks that the
research line shows under the resources.
"""
import json
import os
import re
import shutil
import struct
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, summary, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

PLAYER = ''
REPO = ''
GAMES = ''
SHOTS = ''


# ---- the test game


def unit(type_, side, x, y, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y}
    u.update(kw)
    return u


def make_map(name, w, h, wall_x, blue, red, structures, events=None, funds=5000):
    tiles = []
    for y in range(h):
        tiles.append(''.join('~' if x == wall_x else '.' for x in range(w)))
    m = {'name': name, 'grid': {'type': 'square', 'width': w, 'height': h},
         'legend': {'.': 'plain', '~': 'sea'}, 'tiles': tiles,
         'sides': [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human', 'resources': {'funds': funds}},
                   {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai', 'resources': {'funds': 0}}],
         'structures': structures, 'units': blue + red}
    if events:
        m['events'] = events
    return m


def make_games(root, out):
    g = os.path.join(out, 'eco')
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Economy Player Test',
        'styles': [{'id': 'classic', 'name': 'Classic'},
                   {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                   {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'},
                   {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}],
        'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'},
        'resources': [{'id': 'funds', 'name': 'Funds'}],
        'moveTypes': [{'id': 'foot', 'name': 'Foot'}, {'id': 'tread', 'name': 'Tread'}, {'id': 'wheels', 'name': 'Wheels'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'],
                  'economy': {'enabled': True}, 'production': {'enabled': True, 'builtUnitsCanAct': False},
                  'capture': {'enabled': True, 'amount': 'a.hp / 10'},
                  'supply': {'repair': {'amount': 50, 'costShare': 100}, 'structures': False}},
        'formulas': {'damage': 'vs * (a.hp / a.maxhp)'},
        'maps': [{'id': 'eco', 'name': 'Economy', 'file': 'maps/eco.json'},
                 {'id': 'fight', 'name': 'Fight', 'file': 'maps/fight.json'},
                 {'id': 'repair', 'name': 'Repair', 'file': 'maps/repair.json'},
                 {'id': 'bare', 'name': 'Bare', 'file': 'maps/bare.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain', 'color': '#9ccc65', 'glyph': '.', 'props': {'defense': 1}},
        {'id': 'sea', 'name': 'Sea', 'color': '#3b78c4', 'glyph': '~', 'moveCost': {'foot': 0, 'tread': 0, 'wheels': 0},
         'props': {'defense': 0}}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'infantry', 'name': 'Infantry', 'glyph': 'i', 'moveType': 'foot', 'stats': {'hp': 100, 'mov': 3},
         'weapons': ['rifle'], 'cost': {'funds': 1000}, 'canCapture': True},
        {'id': 'tank', 'name': 'Tank', 'glyph': 'T', 'moveType': 'tread', 'stats': {'hp': 100, 'mov': 6},
         'weapons': ['cannon'], 'cost': {'funds': 3000}, 'fuel': 20},
        {'id': 'truck', 'name': 'Truck', 'glyph': 'S', 'moveType': 'wheels', 'stats': {'hp': 100, 'mov': 5},
         'weapons': [], 'cost': {'funds': 1500}, 'fuel': 40, 'supplies': True}]})
    write_json(g + '/data/weapons.json', {'weapons': [
        {'id': 'rifle', 'name': 'Rifle', 'versus': {'infantry': 55, 'tank': 10, 'truck': 40}},
        {'id': 'cannon', 'name': 'Cannon', 'ammo': 2, 'versus': {'infantry': 60, 'tank': 40, 'truck': 60}}]})
    write_json(g + '/data/structures.json', {'structures': [
        {'id': 'town', 'name': 'Town', 'color': '#b7a58a', 'glyph': 't', 'capturePoints': 20, 'income': {'funds': 500},
         'upgrades': [{'to': 'city', 'cost': {'funds': 1000}}, {'to': 'fort', 'cost': {'funds': 800}, 'requires': 'armor'}]},
        {'id': 'city', 'name': 'City', 'color': '#b7b7b7', 'glyph': 'c', 'capturePoints': 20, 'income': {'funds': 1000}},
        {'id': 'fort', 'name': 'Fort', 'color': '#7a7a90', 'glyph': 'f', 'capturePoints': 20, 'props': {'defense': 3}},
        {'id': 'lab', 'name': 'Lab', 'color': '#8a6ab0', 'glyph': 'L', 'capturePoints': 20, 'research': True},
        {'id': 'factory', 'name': 'Factory', 'color': '#9a9a9a', 'glyph': 'F', 'capturePoints': 20, 'research': True,
         'produces': ['infantry', 'tank', 'truck'], 'upgrades': [{'to': 'foundry', 'cost': {'funds': 9000}}]},
        {'id': 'foundry', 'name': 'Foundry', 'color': '#808080', 'glyph': 'G', 'capturePoints': 20,
         'produces': ['infantry', 'tank', 'truck']}]})
    write_json(g + '/data/research.json', {'technologies': [
        {'id': 'armor', 'name': 'Armor', 'description': 'Heavy plating for vehicles.', 'cost': {'funds': 1000}, 'turns': 2,
         'unlocks': {'units': ['tank']}},
        {'id': 'training', 'name': 'Field Training', 'description': 'Drills make the foot soldiers tougher.',
         'cost': {'funds': 800}, 'turns': 2, 'requires': ['armor'],
         'bonus': {'types': ['infantry'], 'stats': {'hp': 40}}}]})
    # The main map: sea wall at x = 7 keeps the AI (one infantry on the far side) out of the way.
    # Units in id order: tank (5,3), truck (5,2), infantry (4,4). Structures: town (2,1), lab (2,3), factory (2,5).
    write_json(g + '/maps/eco.json', make_map(
        'Economy', 14, 7, 7,
        [unit('tank', 'blue', 5, 3), unit('truck', 'blue', 5, 2), unit('infantry', 'blue', 4, 4)],
        [unit('infantry', 'red', 12, 3)],
        [{'type': 'town', 'x': 2, 'y': 1, 'side': 'blue'}, {'type': 'lab', 'x': 2, 'y': 3, 'side': 'blue'},
         {'type': 'factory', 'x': 2, 'y': 5, 'side': 'blue'}]))
    # A fight: the tank (2 shots in its cannon) against two infantry.
    write_json(g + '/maps/fight.json', make_map(
        'Fight', 8, 5, -1, [unit('tank', 'blue', 3, 2)],
        [unit('infantry', 'red', 4, 2), unit('infantry', 'red', 4, 3)], [], funds=0))
    # Repair: an infantry with 60 HP stands on a town; Field Training (granted at the start) lifts the maximum to 140.
    write_json(g + '/maps/repair.json', make_map(
        'Repair', 8, 5, -1, [unit('infantry', 'blue', 2, 2, stats={'hp': 60})], [unit('infantry', 'red', 7, 4)],
        [{'type': 'town', 'x': 2, 'y': 2, 'side': 'blue'}],
        [{'id': 'train', 'when': {'battle': 'start'}, 'do': [{'grantResearch': 'training', 'side': 'blue'}]}]))
    # Blue owns no structure at all (and has 5000 funds).
    write_json(g + '/maps/bare.json', make_map('Bare', 8, 5, -1, [unit('infantry', 'blue', 2, 2)], [unit('infantry', 'red', 7, 4)], []))
    # The same game with "research needs a structure": the main map has a lab, 'bare' has no structure at all.
    ns = os.path.join(out, 'econeeds')
    shutil.rmtree(ns, ignore_errors=True)
    shutil.copytree(g, ns)
    game = json.load(open(ns + '/game.json'))
    game['name'] = 'Economy Player Test (needs structure)'
    game['rules']['research'] = {'needsStructure': True}
    write_json(ns + '/game.json', game)
    write_json(os.path.join(out, 'index.json'), {'games': ['eco', 'econeeds']})


# ---- running the player

def play(args, saves=None, frames=600, interval=8, inp=None, shot=None, game='eco'):
    """Runs the player windowless; returns (exit code, log text)."""
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings',
           '--frames', str(frames), '--input-interval', str(interval), '--game', game]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + args)
    return p.returncode, p.stdout


def save_of(saves, game='eco'):
    path = os.path.join(saves, game, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


def funds(save):
    return save['battle']['sides'][0]['resources'][0]


def lines(log, prefix):
    """The [info] lines that start with `prefix` (the text after the level tag)."""
    out = []
    for l in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', l)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def read_bmp(path):
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    rows = [data[offset + r * stride: offset + r * stride + width * bpp] for r in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows


# Input recipes. Squares of the main map: the cursor starts on the tank (5,3); the lab is at (2,3), the town at
# (2,1), the factory at (2,5). A token is one key press every `interval` frames; waits let the animations finish.
TO_LAB = 'left,left,left'
TO_TOWN = 'left,left,left,up,up'
TO_FACTORY = 'left,left,left,down,down'
END_TURN = 'menu,confirm,wait:60'
# The battle menu lists End Turn, Research, Save, Load, Leave battle, Enemy ranges: show, Cancel here (no objective on these maps).
RESEARCH = 'menu,down,confirm,wait:2'


def scenarios(tmp):
    def fresh(name):
        d = os.path.join(tmp, name)
        shutil.rmtree(d, ignore_errors=True)
        return d

    # 1. The structure menus and what they list.
    sv = fresh('s_menus')
    code, log = play(['--map', 'eco'], saves=sv, inp='wait:30,' + TO_FACTORY + ',confirm,wait:2,confirm,wait:2,cancel,wait:2,down,confirm,wait:2,cancel,wait:2,cancel,wait:3,' + RESEARCH + ',cancel,wait:3')
    sm = lines(log, 'structure menu:')
    check('factory: structure menu lists Build and Upgrade (no Research any more)', bool(sm) and sm[0] == 'structure menu: Build, Upgrade', log[-600:])
    bm = lines(log, 'build menu:')
    check('build menu: the tank is greyed with the technology it needs', bool(bm) and 'Tank (needs Armor) [greyed]' in bm[0]
          and 'Infantry (1000 Funds)' in bm[0] and 'Truck (1500 Funds)' in bm[0], str(bm))
    um = lines(log, 'upgrade menu:')
    check('upgrade menu: Foundry greyed (not enough funds)', bool(um) and 'Foundry (not enough Funds) [greyed]' in um[0], str(um))
    rp = lines(log, 'research panel:')
    check('research panel: both technologies with cost and turns, the second greyed with nothing else started',
          bool(rp) and rp[0].startswith('research panel: Armor (1000 Funds, 2 turns); Field Training (800 Funds, 2 turns) [greyed]'), str(rp))
    check('cancel in Build returns to the structure menu', len(sm) >= 2, str(sm))
    bt = lines(log, 'battle menu:')
    check('the battle menu lists Research after End Turn (and Cancel in the panel returns to it)',
          bool(bt) and bt[0] == 'battle menu: End Turn, Research, Save, Load, Leave battle, Enemy ranges: show, Cancel' and len(bt) >= 2, str(bt))

    # 2. A lab has nothing of its own to offer (the structure menu does not list Research): Confirm on it opens the
    # battle menu. Greyed rows cannot be confirmed.
    sv = fresh('s_grey')
    code, log = play(['--map', 'eco'], saves=sv, inp='wait:30,' + TO_LAB + ',confirm,wait:2,cancel,wait:2,' + RESEARCH + ',down,confirm,wait:5,cancel,wait:5')
    check('lab: no structure menu; the battle menu opens and Research opens the panel',
          len(lines(log, 'structure menu:')) == 0 and len(lines(log, 'research panel:')) == 1 and len(lines(log, 'battle menu:')) >= 2, log[-400:])
    check('a greyed technology cannot be confirmed', not lines(log, 'research started:'), log[-400:])
    check('greyed research row says why in the log panel', 'Field Training (800 Funds, 2 turns) [greyed]' in ''.join(lines(log, 'research panel:')))

    # 3. Start, count down, finish; the tank unlocks. Pay once.
    sv0 = fresh('s_base')
    play(['--map', 'eco'], saves=sv0, inp='wait:30', frames=120)
    base = save_of(sv0)
    sv = fresh('s_research')
    code, log = play(['--map', 'eco'], saves=sv, inp='wait:30,' + RESEARCH + ',confirm,wait:60', frames=500)
    s = save_of(sv)
    check('research started: logged', lines(log, 'research started:') == ['research started: Blue Army Armor (2 turns)'], log[-400:])
    check('research started: the cost was paid', s and base and funds(s) == funds(base) - 1000, '%s %s' % (funds(s) if s else None, funds(base) if base else None))
    check('research started: the save says it is under way with 2 turns left',
          s and s['battle']['sides'][0]['research'].get('current') == 0 and s['battle']['sides'][0]['research'].get('left') == 2, str(s and s['battle']['sides'][0].get('research')))
    # mid-research: end turn once (1 left), then the save resumes with the panel showing it.
    sv2 = fresh('s_mid')
    shutil.copytree(sv, sv2)
    code, log = play(['--continue'], saves=sv2, inp='wait:20,' + END_TURN, frames=900)  # the next own turn start writes the save
    code2, log2 = play(['--continue'], saves=sv2, inp='wait:30,' + RESEARCH + ',cancel,wait:5', frames=300)
    rp = lines(log2, 'research panel:')
    check('a save made mid-research resumes with the panel showing it (turns left, mark, rest greyed)',
          bool(rp) and rp[0].startswith('research panel: Armor (1 turn left); Field Training') and '[greyed]' in rp[0], str(rp) + log2[-300:])
    # Finish: two end turns after the start.
    sv3 = fresh('s_finish')
    code, log = play(['--map', 'eco'], saves=sv3, inp='wait:30,' + RESEARCH + ',confirm,wait:30,' + END_TURN + ',' + END_TURN + ',' + TO_FACTORY + ',confirm,wait:2,confirm,wait:5', frames=3000)
    check('research counts down and finishes after its turns', lines(log, 'research finished:') == ['research finished: Blue Army Armor'], log[-500:])
    bm = lines(log, 'build menu:')
    check('after Armor the tank is no longer greyed for its technology', bool(bm) and 'needs' not in bm[-1] and 'Tank (3000 Funds)' in bm[-1], str(bm))

    # 4. Upgrade: pays, changes the kind in the save; the greyed entry cannot be confirmed.
    sv = fresh('s_up')
    code, log = play(['--map', 'eco'], saves=sv, inp='wait:30,' + TO_TOWN + ',confirm,wait:2,down,confirm,wait:60', frames=500)
    s = save_of(sv)
    um = lines(log, 'upgrade menu:')
    check('town: Upgrade opens directly (one entry), Fort greyed with the technology it needs',
          bool(um) and um[0] == 'upgrade menu: City (1000 Funds); Fort (needs Armor) [greyed]', str(um))
    check('upgrade: moving down onto the greyed entry still upgrades to the City', lines(log, 'upgrade:') == ['upgrade: Blue Army Town at 2,1 -> City'], log[-400:])
    town = s['battle']['structures'][0] if s else {}  # the map's first structure is the town; kind 1 = City
    check('upgrade: the save has the new kind and the 1000 was paid', town.get('type') == 1 and funds(s) == funds(base) - 1000, str(town))

    # 5. Supply, fuel and ammunition.
    sv = fresh('s_supply')
    code, log = play(['--map', 'eco'], saves=sv, inp='wait:30,confirm,right,up,confirm,confirm,wait:30,next,confirm,confirm,confirm,wait:40,right,wait:10', frames=1500)
    info = lines(log, 'info:')
    check('info: the tank shows fuel and the cannon', bool(info) and info[0] == 'info: Tank: Fuel 20/20, Cannon 2/2', str(info))
    check('info: after a move of 2 the tank has 18 fuel', 'info: Tank: Fuel 18/20, Cannon 2/2' in info, str(info))
    check('Supply by the truck fills the tank', lines(log, 'resupply:') == ['resupply: Tank'] and info and info[-1] == 'info: Tank: Fuel 20/20, Cannon 2/2', str(info) + log[-300:])
    code, log = play(['--map', 'fight'], saves=fresh('s_fight'),
                     inp='wait:30,confirm,confirm,confirm,confirm,wait:60,' + END_TURN + ',wait:60,confirm,confirm,confirm,confirm,wait:60', frames=3000)
    info = lines(log, 'info:')
    check('ammunition: shot down to 1/2 and then marked empty', 'info: Tank: Fuel 20/20, Cannon 1/2' in info and 'info: Tank: Fuel 20/20, Cannon 0/2 EMPTY' in info, str(info))

    # 6. Repair: Field Training finishes at the start; the hurt infantry on the town is repaired next turn.
    code, log = play(['--map', 'repair'], saves=fresh('s_rep'), inp='wait:30,' + END_TURN + ',wait:60', frames=500)
    check('repair: the unit on a structure is repaired (HP number and bar)', lines(log, 'repair:') == ['repair: Infantry +40 HP (now 100)'], log[-400:])
    check('research granted by a story event is announced', lines(log, 'research finished:') == ['research finished: Blue Army Field Training'], log[-300:])

    # 6b. Research needs no building by default: a map with no structure at all can still research, and it counts down.
    sv = fresh('s_nobuilding')
    code, log = play(['--map', 'bare'], saves=sv, inp='wait:30,' + RESEARCH + ',confirm,wait:60', frames=500)
    s = save_of(sv)
    check('default rule: research starts on a map where blue owns no structure',
          lines(log, 'research started:') == ['research started: Blue Army Armor (2 turns)'] and not lines(log, 'research reason:'), log[-400:])
    check('default rule: it is under way in the save (nothing pauses it)',
          s and s['battle']['sides'][0]['research'].get('current') == 0, str(s and s['battle']['sides'][0].get('research')))
    code, log = play(['--map', 'bare'], saves=fresh('s_nobuilding2'), inp='wait:30,' + RESEARCH + ',confirm,wait:30,' + END_TURN + ',' + RESEARCH + ',cancel,wait:5', frames=2500)
    rp = lines(log, 'research panel:')
    check('default rule: the countdown runs without a structure (1 turn left, not paused)',
          len(rp) == 2 and rp[1].startswith('research panel: Armor (1 turn left)'), str(rp))

    # 6c. needsStructure: still offered in the battle menu; without a structure the rows are greyed with the reason and
    # nothing can be started; the structure menu of the lab does not list Research either.
    sv = fresh('s_needs')
    code, log = play(['--map', 'bare'], saves=sv, game='econeeds', inp='wait:30,' + RESEARCH + ',confirm,wait:5,cancel,wait:5')
    rp = lines(log, 'research panel:')
    check('needsStructure, no research structure: the panel opens with every row greyed and says why',
          bool(rp) and rp[0] == 'research panel: Armor (1000 Funds, 2 turns) [greyed]; Field Training (800 Funds, 2 turns) [greyed]'
          and lines(log, 'research reason:') == ['research reason: needs a structure that can research'], str(rp) + log[-300:])
    check('needsStructure: a greyed row cannot be started', not lines(log, 'research started:'), log[-300:])
    sv = fresh('s_needs2')
    code, log = play(['--map', 'eco'], saves=sv, game='econeeds', inp='wait:30,' + RESEARCH + ',confirm,wait:60')
    check('needsStructure with an own lab: research starts', lines(log, 'research started:') == ['research started: Blue Army Armor (2 turns)'], log[-300:])

    # 7. The research line under the resources.
    pa, pb = os.path.join(tmp, 'a.bmp'), os.path.join(tmp, 'b.bmp')
    play(['--map', 'eco'], saves=fresh('s_pa'), inp='wait:30', frames=120, shot=pa)
    play(['--map', 'eco'], saves=fresh('s_pb'), inp='wait:30,' + RESEARCH + ',confirm,wait:60', frames=900, shot=pb)
    wa, ha, ra = read_bmp(pa)
    wb, hb, rb = read_bmp(pb)
    # The info panel is the right 128 pixels; the research row is the line under the one resource row.
    panel_x = wa - 120
    diff = sum(1 for y in range(0, 60) for x in range(panel_x, wa) if ra[y][x * 4:x * 4 + 3] != rb[y][x * 4:x * 4 + 3]) if len(ra[0]) // wa == 4 else \
        sum(1 for y in range(0, 60) for x in range(panel_x, wa) if ra[y][x * 3:x * 3 + 3] != rb[y][x * 3:x * 3 + 3])
    check('research in progress shows under the resources in the info panel', diff > 40, 'differing pixels: %d' % diff)
    if SHOTS:
        for n in ('a', 'b'):
            shutil.copy(os.path.join(tmp, n + '.bmp'), os.path.join(SHOTS, 'research_line_%s.bmp' % n))


def main():
    global PLAYER, REPO, GAMES, SHOTS
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == '--make-games':
        make_games(os.path.abspath(args[2]), os.path.abspath(args[1]))
        return 0
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    tmp = scratch_dir()
    GAMES = os.path.join(tmp, 'games')
    try:
        make_games(REPO, GAMES)
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
