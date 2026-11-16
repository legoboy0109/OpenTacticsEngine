"""The small games of tests/srpg_player_promotion_test.py (docs/SRPG.md step U2), written into a temp folder.

One game, "promo", with flat-colour art so a test can tell unit types apart by pixels: every unit type has its own
sprite sheet colour (top-down and 2D isometric) and its own 3D model colour (the two 3D styles).

Unit types: recruit (level 3: offered captain or scout; the writ gives the same two), captain (brings the skill
Tough), scout (cannot wield blades; keeps its level 3, where it learns Light Step), pawn (offered rider), rider, squire
(a token turns it into a captain), cadet (becomes a captain by itself), grunt (the enemy).
Maps (8 x 4, blue = human, red = AI far away): "choose" (kell the recruit, bram the pawn, ivo the squire), "auto"
(dara the cadet), "story" (wren the recruit, promoted by a battle-start event), "plain" (a recruit with nothing on offer).
"""
import json
import os

from terrain3d_player_test import png, box_glb

# Colour of each unit type's picture (sprite and model); blue's side marker is #3060ff, red's #cc0000.
COLORS = {'recruit': (230, 120, 40), 'captain': (40, 210, 90), 'scout': (60, 200, 255), 'pawn': (200, 60, 200),
          'rider': (250, 240, 60), 'squire': (150, 150, 150), 'cadet': (120, 70, 20), 'grunt': (140, 20, 20)}
STYLES = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric', 'background': '#161a24'},
          {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d', 'background': '#161a24'},
          {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d', 'background': '#161a24'}]


def promo(who, type_, x, y, **kw):
    u = {'id': who, 'name': who.capitalize(), 'type': type_, 'side': 'blue', 'x': x, 'y': y}
    u.update(kw)
    return u


def promo_map(units, events=None):
    m = {'name': 'Field', 'grid': {'type': 'square', 'width': 8, 'height': 4}, 'legend': {'.': 'plain'}, 'tiles': ['........'] * 4,
         'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#3060ff', 'controller': 'human'},
                   {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai'}],
         'units': units + [{'type': 'grunt', 'side': 'red', 'x': 7, 'y': 3}]}
    if events:
        m['events'] = events
    return m


MAPS = {
    'choose': promo_map([promo('kell', 'recruit', 1, 1, level=3), promo('bram', 'pawn', 2, 2), promo('ivo', 'squire', 3, 1)]),
    'auto': promo_map([promo('dara', 'cadet', 1, 1)]),
    'story': promo_map([promo('wren', 'recruit', 1, 1)],
                       [{'id': 'go', 'when': {'battle': 'start'}, 'do': [{'promote': 'wren', 'to': 'captain'}]}]),
    'plain': promo_map([promo('tam', 'recruit', 1, 1)]),
}


def unit_types():
    st = lambda hp, atk, de, mov: {'hp': hp, 'atk': atk, 'def': de, 'mov': mov}
    t = lambda id_, stats, **kw: dict({'id': id_, 'name': id_.capitalize(), 'glyph': id_[0].upper(), 'moveType': 'foot', 'stats': stats}, **kw)
    return {'units': [
        t('recruit', st(10, 4, 2, 4), wields=['blade'], items=['blade', 'writ', 'longbow'], promotions=[
            {'to': 'captain', 'fromLevel': 3, 'atLevel': 'offer', 'items': ['writ']},
            {'to': 'scout', 'fromLevel': 3, 'atLevel': 'offer', 'items': ['writ'], 'levelAfter': 'keep', 'gains': {'hp': 3, 'mov': 1}}]),
        t('captain', st(16, 7, 4, 4), wields=['blade'], skills=['tough']),
        t('scout', st(12, 5, 2, 6), wields=['bow'], learns=[{'level': 3, 'skill': 'light_step'}]),
        t('pawn', st(9, 3, 1, 3), wields=['blade'], promotions=[{'to': 'rider', 'atLevel': 'offer'}]),
        t('rider', st(14, 6, 3, 5), wields=['blade']),
        t('squire', st(9, 3, 1, 3), wields=['blade'], items=['token'], promotions=[{'to': 'captain', 'items': ['token']}]),
        t('cadet', st(8, 3, 1, 3), wields=['blade'], promotions=[{'to': 'captain', 'atLevel': 'auto'}]),
        t('grunt', st(8, 3, 1, 3), wields=['blade'], items=['blade'])]}


def graphics():
    kinds = list(COLORS)
    sheets = [{'id': k, 'file': 'art/%s.png' % k, 'cellWidth': 24, 'cellHeight': 24} for k in kinds]
    units = {k: {'sheet': k, 'cell': [0, 0]} for k in kinds}
    models = {k: {'model': 'models/%s.glb' % k, 'height': 1.0} for k in kinds}
    return {'sheets': sheets, 'unitMarker': True, 'units': units,
            'styles': {'isometric': {'units': units}, 'isometric-3d': {'units': models}, 'full-3d': {'units': models}}}


def write_game(root, view=None):
    """Writes games/promo under `root`; `view` = (width, height) of the screen (a phone-sized one for the small shots)."""
    g = os.path.join(root, 'promo')
    for d in ('data', 'maps', 'art', 'models'):
        os.makedirs(os.path.join(g, d), exist_ok=True)

    def put(name, doc):
        json.dump(doc, open(os.path.join(g, name), 'w'), indent=1)
    game = {'formatVersion': 1, 'name': 'Promotion Player', 'stats': [{'id': s} for s in ('hp', 'atk', 'def', 'mov')],
            'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}], 'styles': STYLES,
            'rules': {'defeat': ['noUnits'], 'counterAttacks': True, 'items': {'weapons': 'carried', 'inventorySize': 4},
                      'progression': {'enabled': True, 'xpPerLevel': 100, 'maxLevel': 20, 'xpHit': '30', 'xpKill': '60'}},
            'formulas': {'damage': 'a.atk - b.def'},
            'maps': [{'id': k, 'name': k, 'file': 'maps/%s.json' % k} for k in MAPS]}
    if view:
        game['view'] = {'width': view[0], 'height': view[1], 'tileSize': 24}
    put('game.json', game)
    put('data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain'}]})
    put('data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword', 'group': 'blade', 'props': {'might': 1}},
                                          {'id': 'bow', 'name': 'Bow', 'group': 'bow', 'props': {'might': 1}}]})
    put('data/items.json', {'items': [
        {'id': 'writ', 'name': 'Writ', 'kind': 'usable', 'uses': 1, 'promotes': True},
        {'id': 'token', 'name': 'Token', 'kind': 'usable', 'uses': 1, 'promotes': True},
        {'id': 'blade', 'name': 'Blade', 'kind': 'weapon', 'weapon': 'sword'},
        {'id': 'longbow', 'name': 'Longbow', 'kind': 'weapon', 'weapon': 'bow'}]})
    put('data/skills.json', {'skills': [
        {'id': 'tough', 'name': 'Tough', 'blocks': [{'when': 'always', 'do': [{'stats': {'hp': 5}}]}]},
        {'id': 'light_step', 'name': 'Light Step', 'blocks': [{'when': 'always', 'do': [{'stats': {'mov': 1}}]}]}]})
    put('data/units.json', unit_types())
    put('data/graphics.json', graphics())
    for k, rgb in COLORS.items():
        open(os.path.join(g, 'art', k + '.png'), 'wb').write(png(24, 24, rgb + (255,)))
        open(os.path.join(g, 'models', k + '.glb'), 'wb').write(box_glb(rgb))
    for k, m in MAPS.items():
        put('maps/%s.json' % k, m)
    return g
