"""A small game for tests/sound_player_test.py, written into a temp folder together with the starter audio.

  make_game(root, gid, audio_dir, **options) -> path of the game folder

Everything in the game is chosen so a sound test is deterministic: hit chances are 0 or 100 (a weapon's `hit`
number), a crit is 0 or 100, the AI never has anything to attack unless a map says so. Squares are given as x, y.

  maps:  duel     knight (1,1) hits a grunt (3,1): one blow kills it and wins the battle
         dull     recruit (1,1) with a weapon that always misses a tough grunt (3,1)
         crusher  brute (1,1) with a weapon that always crits a tough grunt (3,1)
         ride     rider (1,1) walks; a tough red unit stands far away (nothing ends the battle)
         cap      knight on a village (1,1), far red unit
         build    knight on a factory (1,1) with 500 gold, far red unit
         turn     knight (1,1) and a red unit far away that walks on its turn
         potion   knight (1,1) carrying a potion (raises Attack), far red unit
         story    battle-start event: playMusic "boss" and playSound "thunder"; turn 2: stopMusic
         quiet    map music "boss", playerTurnMusic "calm"
         fogwalk  fog on: a blue knight (0,1), a red scout walks towards it from (11,1)
         levels   a knight whose first hit levels up (progression on only with `levels=True`)
         talk     a line of dialogue at the start
"""
import json
import os
import shutil
from ote_testkit import write_json  # tests/ is on sys.path: only the scripts import this module


def _unit(type_, side, x, y, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y}
    u.update(kw)
    return u


def _map(name, w, h, units, structures=(), events=None, fog=False, extra=None, funds=0):
    m = {'name': name, 'grid': {'type': 'square', 'width': w, 'height': h}, 'legend': {'.': 'plain'},
         'tiles': ['.' * w for _ in range(h)],
         'sides': [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human', 'resources': {'gold': funds}},
                   {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai', 'resources': {'gold': 0}}],
         'units': list(units)}
    if structures:
        m['structures'] = list(structures)
    if events:
        m['events'] = events
    if fog:
        m['fog'] = True
    if extra:
        m.update(extra)
    return m


MAPS = ['duel', 'dull', 'crusher', 'ride', 'cap', 'build', 'turn', 'potion', 'story', 'quiet', 'fogwalk', 'levels', 'talk']


def make_game(root, gid, audio_dir=None, audio_json=None, levels=False, campaign=None, sounds=True, sound_overrides=True):
    """audio_dir: a folder with audio/ made by tools/audio/make_starter_audio.py (copied in); audio_json: the text of
    data/audio.json (None = none at all). sounds=False leaves the override blocks out (a game with plain audio)."""
    g = os.path.join(root, gid)
    shutil.rmtree(g, ignore_errors=True)
    rules = {'turnOrder': 'sides', 'counterAttacks': False, 'defeat': ['noUnits'],
             'items': {'weapons': 'fixed', 'inventorySize': 3, 'storage': 'anywhere'},
             'economy': {'enabled': True}, 'production': {'enabled': True, 'builtUnitsCanAct': False},
             'capture': {'enabled': True, 'amount': 'a.hp'}}
    if levels:
        rules['progression'] = {'enabled': True, 'xpPerLevel': 10, 'maxLevel': 5, 'xpHit': '10', 'xpKill': '10'}
    game = {
        'formatVersion': 1, 'name': 'Sound Test',
        'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Attack'}, {'id': 'def', 'name': 'Defense'},
                  {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'},
        'resources': [{'id': 'gold', 'name': 'Gold'}],
        'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'switches': [{'id': 'met', 'name': 'Met'}],
        'rules': rules,
        'formulas': {'damage': 'w.might + a.atk - b.def', 'hit': 'w.hit', 'crit': 'w.crit'},
        'maps': [{'id': m, 'name': m.capitalize(), 'file': 'maps/%s.json' % m} for m in MAPS]}
    write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65', 'glyph': '.'}]})
    ov = sound_overrides
    sw = lambda **k: {'sounds': k} if ov else {}
    write_json(g + '/data/weapons.json', {'weapons': [
        {'id': 'sword', 'name': 'Sword', 'props': {'might': 5, 'hit': 100, 'crit': 0}, **sw(hit='sword_hit')},
        {'id': 'dull', 'name': 'Dull Blade', 'props': {'might': 5, 'hit': 0, 'crit': 0}},
        {'id': 'crusher', 'name': 'Crusher', 'props': {'might': 1, 'hit': 100, 'crit': 100}},
        {'id': 'rod', 'name': 'Rod', 'props': {'might': 0, 'hit': 100, 'crit': 0}}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'knight', 'name': 'Knight', 'moveType': 'foot', 'stats': {'hp': 20, 'atk': 5, 'def': 0, 'mov': 4},
         'weapons': ['sword'], 'canCapture': True, 'cost': {'gold': 100}, 'growths': {'hp': 100}},
        {'id': 'recruit', 'name': 'Recruit', 'moveType': 'foot', 'stats': {'hp': 20, 'atk': 5, 'def': 0, 'mov': 4},
         'weapons': ['dull']},
        {'id': 'brute', 'name': 'Brute', 'moveType': 'foot', 'stats': {'hp': 20, 'atk': 5, 'def': 0, 'mov': 4},
         'weapons': ['crusher']},
        {'id': 'rider', 'name': 'Rider', 'moveType': 'foot', 'stats': {'hp': 20, 'atk': 5, 'def': 0, 'mov': 5},
         'weapons': ['sword'], **sw(step='hooves', select='neigh')},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': {'hp': 5, 'atk': 5, 'def': 0, 'mov': 3},
         'weapons': ['rod']},
        {'id': 'tough', 'name': 'Tough', 'moveType': 'foot', 'stats': {'hp': 90, 'atk': 1, 'def': 0, 'mov': 3},
         'weapons': ['rod']}]})
    write_json(g + '/data/items.json', {'items': [
        {'id': 'potion', 'name': 'Potion', 'kind': 'usable', 'uses': 2, 'raise': {'atk': 1}, **sw(use='potion')}]})
    write_json(g + '/data/structures.json', {'structures': [
        {'id': 'village', 'name': 'Village', 'color': '#b7a58a', 'glyph': 'v', 'capturePoints': 20},
        {'id': 'factory', 'name': 'Factory', 'color': '#9a9a9a', 'glyph': 'F', 'capturePoints': 20, 'produces': ['knight']}]})
    far = _unit('tough', 'red', 7, 1)
    maps = {
        'duel': _map('Duel', 8, 3, [_unit('knight', 'blue', 1, 1), _unit('grunt', 'red', 3, 1)]),
        'dull': _map('Dull', 8, 3, [_unit('recruit', 'blue', 1, 1), _unit('tough', 'red', 3, 1)]),
        'crusher': _map('Crusher', 8, 3, [_unit('brute', 'blue', 1, 1), _unit('tough', 'red', 3, 1)]),
        'ride': _map('Ride', 8, 3, [_unit('rider', 'blue', 1, 1), far]),
        'cap': _map('Cap', 8, 3, [_unit('knight', 'blue', 1, 1), far], [{'type': 'village', 'x': 1, 'y': 1}]),
        'build': _map('Build', 8, 3, [_unit('knight', 'blue', 5, 1), far], [{'type': 'factory', 'x': 1, 'y': 1, 'side': 'blue'}], funds=500),
        'turn': _map('Turn', 8, 3, [_unit('knight', 'blue', 1, 1), far]),
        'potion': _map('Potion', 8, 3, [_unit('knight', 'blue', 1, 1, items=['potion']), far]),
        'story': _map('Story', 8, 3, [_unit('knight', 'blue', 1, 1), far], events=[
            {'id': 'open', 'when': {'battle': 'start'}, 'do': [{'playMusic': 'boss', 'fade': 1.0}, {'playSound': 'thunder'}]},
            {'id': 'calm', 'when': {'turn': 2}, 'do': [{'stopMusic': True, 'fade': 0.25}]}]),
        'quiet': _map('Quiet', 8, 3, [_unit('knight', 'blue', 1, 1), far],
                      extra={'music': 'boss', 'playerTurnMusic': 'calm'} if ov else None),
        'fogwalk': _map('Fogwalk', 12, 3, [_unit('knight', 'blue', 0, 1), _unit('rider', 'red', 11, 1)], fog=True),
        'talk': _map('Talk', 8, 3, [_unit('knight', 'blue', 1, 1), far], events=[
            {'id': 'hello', 'when': {'battle': 'start'}, 'do': [{'say': 'Greetings, traveller of the long road. Sit and rest.'}]}]),
        'levels': _map('Levels', 8, 3, [_unit('knight', 'blue', 1, 1), _unit('grunt', 'red', 3, 1), _unit('tough', 'red', 7, 2)]),
    }
    for name, m in maps.items():
        write_json(g + '/maps/%s.json' % name, m)
    if audio_dir:
        shutil.copytree(os.path.join(audio_dir, 'audio'), g + '/audio')
    if audio_json is not None:
        os.makedirs(g + '/data', exist_ok=True)
        with open(g + '/data/audio.json', 'w') as f:
            f.write(audio_json if isinstance(audio_json, str) else json.dumps(audio_json, indent=1))
    if campaign is not None:
        write_json(g + '/data/campaign.json', campaign)
    return g
