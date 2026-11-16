#!/usr/bin/env python3
"""The unit art of the story demo and the strategy-RPG template (docs/GRAPHICS.md "Starter art", tools/art).

usage: python3 tests/srpg_art_test.py REPO_ROOT

Every unit type of demo-srpg has a picture / model of its own in every style that draws units, so the promoted and summoned
types (captain, lancer, outrider, marksman, brute, houndmaster, hound) no longer borrow their source type's art:
  - in the top-down sheet, the 2D isometric sheet, and every style's unit table, no two unit types share a look (sheet row
    or model file), and every model file exists;
  - the sheets' rows are different pictures (not copies of one another);
  - each of the seven has its own entry in the generated .glb sets (models/ and models/detailed/), with the four clips;
  - the template's Outrider has its own row, not the Cavalier's;
  - the drawing tools have no alias table left (art_units.ALIASES);
  - make_unit_models.py --check passes (structure of the generated models) and a rerun of it changes no file.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
sys.path.insert(0, os.path.join(root, 'tools', 'art'))
from PIL import Image  # noqa: E402
import art_units  # noqa: E402

failures = []
SEVEN = ['captain', 'lancer', 'outrider', 'marksman', 'brute', 'houndmaster', 'hound']
SOURCE = {'captain': 'lord', 'lancer': 'cavalier', 'outrider': 'cavalier', 'marksman': 'archer', 'brute': 'fighter',
          'houndmaster': 'soldier', 'hound': 'fighter'}


def check(name, ok, detail=''):
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else ' ' + str(detail)))
    if not ok:
        failures.append(name)


def strip_comments(text):
    """Removes // line comments that are not inside strings (the game files carry them)."""
    out, i, in_str = [], 0, False
    while i < len(text):
        ch = text[i]
        if in_str:
            out.append(ch)
            if ch == '\\':
                i += 1
                out.append(text[i])
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
            out.append(ch)
        elif text[i:i + 2] == '//':
            while i < len(text) and text[i] != '\n':
                i += 1
            continue
        else:
            out.append(ch)
        i += 1
    return ''.join(out)


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.loads(strip_comments(f.read()))


def key(look):
    """What makes a look "the same picture": a model file, or sheet + frames/cell."""
    if 'model' in look:
        return ('model', look['model'])
    return ('sheet', look.get('sheet'), json.dumps(look.get('frames', look.get('cell'))))


def glb_clips(path):
    with open(path, 'rb') as f:
        data = f.read()
    n = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + n])
    return {a['name'] for a in doc.get('animations', [])}


demo = os.path.join(root, 'games', 'demo-srpg')
g = load(os.path.join(demo, 'data', 'graphics.json'))
ids = [u['id'] for u in load(os.path.join(demo, 'data', 'units.json'))['units']]
tables = {'(base: classic, parchment)': g['units']}
tables.update({'style ' + sid: st['units'] for sid, st in g.get('styles', {}).items() if st.get('units')})
check('demo-srpg: the unit table of every style is found (base + isometric, isometric-3d, full-3d, detailed-3d)',
      len(tables) == 5, list(tables))
for what, table in tables.items():
    check('%s: every unit type has a look' % what, all(u in table for u in ids), [u for u in ids if u not in table])
    keys = {u: key(table[u]) for u in ids if u in table}
    shared = sorted(u for u in keys if sum(1 for k in keys.values() if k == keys[u]) > 1)
    check('%s: no two unit types share a look' % what, not shared, shared)
    for u in SEVEN:
        check('%s: %s does not borrow %s\'s look' % (what, u, SOURCE[u]), keys.get(u) != keys.get(SOURCE[u]))
    for u, look in table.items():
        if 'model' in look:
            check('%s: %s model file exists' % (what, u), os.path.exists(os.path.join(demo, look['model'])), look['model'])

# the sheets: every row of units.png / iso_units.png is a different picture
for name, cell in (('units.png', 24), ('iso_units.png', 22)):
    im = Image.open(os.path.join(demo, 'art', name)).convert('RGBA')
    rows = [hashlib.sha1(im.crop((0, r * cell, im.width, (r + 1) * cell)).tobytes()).hexdigest() for r in range(im.height // cell)]
    check('art/%s: %d rows, all different pictures' % (name, len(ids)), len(rows) == len(ids) and len(set(rows)) == len(rows),
          (len(rows), len(set(rows))))

# the generated models: one file per unit type, four clips each
for folder in ('models', 'models/detailed'):
    for u in SEVEN:
        path = os.path.join(demo, folder, u + '.glb')
        ok = os.path.exists(path) and glb_clips(path) == {'Idle', 'Move', 'Attack', 'Die'}
        check('%s/%s.glb exists with Idle, Move, Attack, Die' % (folder, u), ok)
    sizes = {hashlib.sha1(open(os.path.join(demo, folder, u + '.glb'), 'rb').read()).hexdigest() for u in SEVEN
             if os.path.exists(os.path.join(demo, folder, u + '.glb'))}
    check('%s: the seven files are all different' % folder, len(sizes) == 7)

# the template
tg = load(os.path.join(root, 'editor', 'templates', 'strategy-rpg', 'data', 'graphics.json'))
check('template strategy-rpg: Outrider has a row of its own (top-down and isometric)',
      key(tg['units']['outrider']) != key(tg['units']['cavalier']) and
      key(tg['styles']['isometric']['units']['outrider']) != key(tg['styles']['isometric']['units']['cavalier']))

# the tools
check('art_units has no alias table and one drawing function per type',
      not hasattr(art_units, 'ALIASES') and len(set(art_units.UNITS.values())) == len(art_units.UNITS))
check('art_units draws all of demo-srpg\'s unit types', all(u in art_units.UNITS for u in ids))
p = subprocess.run([sys.executable, os.path.join(root, 'tools', 'art', 'make_unit_models.py'), '--check', root],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
check('make_unit_models.py --check passes', p.returncode == 0 and 'all checks passed' in p.stdout, p.stdout[-300:])

sys.exit(1 if failures else 0)
