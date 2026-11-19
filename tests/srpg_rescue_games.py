"""Test games for tests/srpg_player_rescue_test.py: a small square-grid game with `rules.rescue` switched on (carry or
pair up), four visual styles (top-down, 2D isometric, 3D isometric, full 3D) and one map with three blue units and one red.

Written here, byte for byte, so the player test depends on no shipped game (games/demo-srpg is somebody else's).
"""
import json
import os
import shutil
from ote_testkit import write_json  # tests/ is on sys.path: only the scripts import this module

TILE = 24
MAP_W, MAP_H = 7, 5
# Unit ids follow this order: blue Ann 0 (2,2), Bram 1 (3,2), Cleo 2 (2,3); red Rex 3 (6,4).
UNITS = [('Ann', 'blue', 2, 2), ('Bram', 'blue', 3, 2), ('Cleo', 'blue', 2, 3), ('Rex', 'red', 6, 4)]
STYLES = [{'id': 'classic', 'name': 'Classic'},
          {'id': 'iso', 'name': 'Isometric', 'view': 'isometric', 'background': '#161a24'},
          {'id': 'iso3d', 'name': '3D Isometric', 'view': 'isometric-3d', 'background': '#161a24'},
          {'id': 'full3d', 'name': 'Full 3D', 'view': '3d', 'background': '#161a24'}]


def map_doc(units=UNITS, width=MAP_W, height=MAP_H, hexes=False):
    return {'name': 'Open', 'grid': {'type': 'hex' if hexes else 'square', 'width': width, 'height': height},
            'legend': {'.': 'plain'}, 'tiles': ['.' * width] * height,
            'sides': [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
                      {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}],
            'structures': [],
            'units': [{'type': 'infantry' if s == 'blue' else 'sentry', 'side': s, 'x': x, 'y': y, 'name': n}
                      for n, s, x, y in units]}


def all_maps(hexes=False):
    """'open': the usual four units. 'noann': the same without Ann (the baseline of the marker pictures). 'tight': a corner
    where a carrier has nowhere to put its unit down (blue Ann, Bram, Cleo, Dan; red Rex)."""
    tight = [('Ann', 'blue', 0, 0), ('Bram', 'blue', 1, 0), ('Cleo', 'blue', 0, 1), ('Dan', 'blue', 1, 1), ('Rex', 'red', 2, 1)]
    return {'open': map_doc(hexes=hexes), 'noann': map_doc(UNITS[1:], hexes=hexes),
            'tight': map_doc(tight, 3, 2, hexes=hexes)}


def write_game(root, gid, mode, view=(480, 270), maps=None, hexes=False):
    """`mode` = 'carry' or 'pairUp'; `maps` = {id: map dict} (default: all_maps()); `hexes`: pointy hexagons."""
    g = os.path.join(root, gid)
    shutil.rmtree(g, ignore_errors=True)
    rescue = {'mode': mode}
    if mode == 'pairUp':
        rescue['pairBonus'] = {'str': 2}
    maps = maps or all_maps(hexes)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Rescue Player Test ' + mode, 'grid': {'type': 'hex', 'layout': 'pointy'} if hexes else {'type': 'square'},
        'view': {'width': view[0], 'height': view[1], 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'mov', 'name': 'Move'}, {'id': 'str', 'name': 'Str'}],
        'roles': {'hp': 'hp', 'move': 'mov'},
        'resources': [{'id': 'funds', 'name': 'Funds'}],
        'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'], 'rescue': rescue},
        'formulas': {'damage': 'vs * (a.hp / a.maxhp)'},
        'styles': STYLES[:2] if hexes else STYLES,  # hexagon games have no 3D kinds yet
        'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in maps]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#96be5a', 'glyph': ' '}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'infantry', 'name': 'Infantry', 'glyph': 'i', 'moveType': 'foot',
         'stats': {'hp': 100, 'mov': 4, 'str': 5}, 'weapons': ['rifle'], 'vision': 3},
        # red's unit cannot walk anywhere near the blue ones: the enemy turn changes nothing on the map
        {'id': 'sentry', 'name': 'Sentry', 'glyph': 's', 'moveType': 'foot',
         'stats': {'hp': 100, 'mov': 1, 'str': 5}, 'weapons': ['rifle'], 'vision': 3}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'rifle', 'name': 'Rifle', 'versus': {'infantry': 55, 'sentry': 55}}]})
    write_json(g + '/data/structures.json', {'structures': []})
    for mid, m in maps.items():
        write_json(g + '/maps/%s.json' % mid, m)
    return g
