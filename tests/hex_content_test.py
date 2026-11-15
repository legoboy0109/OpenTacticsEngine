#!/usr/bin/env python3
"""Checks the hexagon starter content (docs/HEX.md "Starter content"): art generator, templates, demo.

usage: python3 tests/hex_content_test.py REPO_ROOT [OTE_SIM]

- the generator (tools/art/make_hex_art.py) is deterministic and what is committed is what it makes
  (run twice into temp copies of the three game folders; compared byte for byte with the committed files);
- sheet sizes and cell grids; every file graphics.json names exists; every terrain, structure and unit has a look;
- every terrain cell is opaque inside the hexagon and transparent outside it (both layouts); the 64 join pieces exist
  for the road and for both frames of water and bridge; neighbouring hexagons tile seamlessly (the hexagons partition
  the pixel grid; a composited map has no unpainted pixel inside the map's hexagons);
- the structure pictures have the contract's sizes; the ring picture is opaque on the seven hexagons only;
- both templates and the demo load and their sims finish without a stall (needs ote-sim, default build/ote-sim);
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
sim = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(root, 'build', 'ote-sim')
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(root, 'tools', 'art'))
from PIL import Image  # noqa: E402

import hex_geom  # noqa: E402
import make_hex_art as gen  # noqa: E402
import preview_hex  # noqa: E402

GAMES = {'games/demo-hex': 'pointy', 'editor/templates/hex-tactics-pointy': 'pointy',
         'editor/templates/hex-tactics-flat': 'flat'}
checks = 0
failures = []


def check(cond, msg):
    global checks
    checks += 1
    if not cond:
        failures.append(msg)


def read(path):
    with open(path, 'rb') as f:
        return f.read()


def generated_files(base):
    out = {}
    for rel in GAMES:
        for sub in ('art', 'data/graphics.json'):
            p = os.path.join(base, rel, sub)
            if os.path.isdir(p):
                for fn in sorted(os.listdir(p)):
                    out[os.path.join(rel, sub, fn)] = read(os.path.join(p, fn))
            elif os.path.isfile(p):
                out[os.path.join(rel, sub)] = read(p)
    return out


def generator_checks():
    tmp = tempfile.mkdtemp(prefix='ote_hexgen_')
    try:
        runs = []
        for n in range(2):
            copy = os.path.join(tmp, 'run%d' % n)
            for rel in GAMES:   # only what the generator reads (the data) and writes
                shutil.copytree(os.path.join(root, rel), os.path.join(copy, rel),
                                ignore=shutil.ignore_patterns('audio', 'maps', 'art'))
            subprocess.run([sys.executable, '-B', os.path.join(root, 'tools', 'art', 'make_hex_art.py'), copy],
                           check=True, stdout=subprocess.DEVNULL)
            runs.append(generated_files(copy))
        check(runs[0] == runs[1], 'the generator is not deterministic')
        committed = generated_files(root)
        for name in sorted(set(runs[0]) | set(committed)):
            check(runs[0].get(name) == committed.get(name), '%s is not what make_hex_art.py generates' % name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def geometry_checks():
    for layout in ('pointy', 'flat'):
        g = hex_geom.Geom(layout)
        check((g.w, g.h) == ((24, 28) if layout == 'pointy' else (28, 24)), '%s: box size' % layout)
        # The hexagons of a map partition the pixel grid: every pixel of a 6 x 6 patch is inside exactly one.
        owner = {}
        for y in range(6):
            for x in range(6):
                ox, oy = g.box_origin(x, y)
                for py in range(g.h):
                    for px in range(g.w):
                        if g.inside(px, py):
                            owner[(ox + px, oy + py)] = owner.get((ox + px, oy + py), 0) + 1
        check(all(v == 1 for v in owner.values()), '%s: hexagons overlap' % layout)
        # inside the part of the patch that is covered on all sides there is no gap
        x0, x1 = (g.w, 6 * g.w - g.w) if layout == 'pointy' else (g.w, 5 * g.pitch)
        y0, y1 = (g.h, 5 * g.pitch) if layout == 'pointy' else (g.h, 6 * g.h - g.h)
        gaps = [(x, y) for y in range(y0, y1) for x in range(x0, x1) if (x, y) not in owner]
        check(not gaps, '%s: %d unpainted pixels inside the patch, e.g. %s' % (layout, len(gaps), gaps[:3]))
        w, h = g.ring_size()
        check((w, h) == ((72, 70) if layout == 'pointy' else (70, 72)), '%s: ring picture size %dx%d' % (layout, w, h))
        # neighbour order: neighbour d of the middle hexagon sits where dirs[d] says
        mx, my = (4, 4) if layout == 'pointy' else (4, 4)
        bx, by = g.box_origin(mx, my)
        for d in range(6):
            nx, ny = g.neighbor(mx, my, d)
            nbx, nby = g.box_origin(nx, ny)
            check((nbx - bx, nby - by) == tuple(int(v) for v in g.dirs[d]), '%s: neighbour %d offset' % (layout, d))


def art_checks(rel, layout):
    path = os.path.join(root, rel)
    g = hex_geom.Geom(layout)
    doc = gen.load_json(os.path.join(path, 'data/graphics.json'))
    terrain_ids = [t['id'] for t in gen.load_json(os.path.join(path, 'data/terrain.json'))['terrain']]
    structs = gen.load_json(os.path.join(path, 'data/structures.json'))['structures']
    units = [u['id'] for u in gen.load_json(os.path.join(path, 'data/units.json'))['units']]
    sheets = {}
    for s in doc['sheets']:
        f = os.path.join(path, s['file'])
        check(os.path.isfile(f), '%s: %s missing' % (rel, s['file']))
        if not os.path.isfile(f):
            continue
        im = Image.open(f).convert('RGBA')
        sheets[s['id']] = (im, s['cellWidth'], s['cellHeight'])
        check(im.width % s['cellWidth'] == 0 and im.height % s['cellHeight'] == 0, '%s: sheet %s is not a whole number of cells' % (rel, s['id']))
    for t in terrain_ids:
        check(t in doc['terrain'], '%s: terrain %s has no look' % (rel, t))
    for s in structs:
        check(s['id'] in doc.get('structures', {}), '%s: structure %s has no look' % (rel, s['id']))
    for u in units:
        check(u in doc['units'], '%s: unit %s has no look' % (rel, u))

    def cell(sheet, col, row):
        im, cw, ch = sheets[sheet]
        return im.crop((col * cw, row * ch, (col + 1) * cw, (row + 1) * ch))

    def hex_ok(im, what):
        bad_in = bad_out = 0
        for y in range(g.h):
            for x in range(g.w):
                a = im.getpixel((x, y))[3]
                if g.inside(x, y) and a != 255:
                    bad_in += 1
                if not g.inside(x, y) and a != 0:
                    bad_out += 1
        check(bad_in == 0, '%s: %s has %d see-through pixels inside the hexagon' % (rel, what, bad_in))
        check(bad_out == 0, '%s: %s has %d painted pixels outside the hexagon' % (rel, what, bad_out))

    for tid, look in doc['terrain'].items():
        check(look['sheet'] in sheets and sheets[look['sheet']][1:] == (g.w, g.h), '%s: %s sheet cell size' % (rel, tid))
        cells = []
        if 'variants' in look:
            cells = [tuple(c) for c in look['variants']]
        elif 'cell' in look:
            cells = [tuple(look['cell'])]
        for c, r in cells:
            hex_ok(cell(look['sheet'], c, r), '%s %s' % (tid, (c, r)))
        if 'join' in look:
            blocks = look['join'] if isinstance(look['join'][0], list) else [look['join']]
            check(tid != 'water' or len(blocks) == 2, '%s: water must be an animated join of two frames' % rel)
            im, cw, ch = sheets[look['sheet']]
            for c0, r0 in blocks:
                check(c0 + 8 <= im.width // cw and r0 + 8 <= im.height // ch, '%s: %s join block outside its sheet' % (rel, tid))
                for n in range(64):   # all 64 pieces: present (hexagon-shaped, opaque inside)
                    hex_ok(cell(look['sheet'], c0 + n % 8, r0 + n // 8), '%s piece %d' % (tid, n))
    # structures: one hexagon = box width by box height + 4; rings = the seven-hexagon box
    for s in structs:
        look = doc['structures'][s['id']]
        im, cw, ch = sheets[look['sheet']]
        want = g.ring_size() if s.get('rings') else (g.w, g.h + 4)
        check((cw, ch) == want, '%s: structure %s picture is %dx%d, want %s' % (rel, s['id'], cw, ch, want))
        pic = cell(look['sheet'], *look['cell'])
        check(pic.getchannel('A').getbbox() is not None, '%s: structure %s picture is empty' % (rel, s['id']))
        if s.get('rings'):
            (mx, my), outer = g.ring_centres()
            boxes = [(int(round(cx - g.cx)), int(round(cy - g.cy))) for cx, cy in [(mx, my)] + outer]
            outside = sum(1 for y in range(ch) for x in range(cw)
                          if pic.getpixel((x, y))[3] and not any(g.inside(x - ox, y - oy) for ox, oy in boxes))
            # the outline may add a one pixel rim outside the seven hexagons, nothing more
            check(outside < 4 * (cw + ch), '%s: ring picture paints %d pixels away from the seven hexagons' % (rel, outside))
            # its bottom edge is on the box's bottom: something is painted in the last two rows
            check(pic.getchannel('A').crop((0, ch - 2, cw, ch)).getbbox() is not None, '%s: ring picture does not reach the bottom' % rel)


def composite_checks(rel, layout):
    """A map made only of terrain (every terrain id in turn) leaves no unpainted pixel inside its hexagons."""
    path = os.path.join(root, rel)
    game = gen.load_json(os.path.join(path, 'game.json'))
    for entry in game['maps']:
        mp = gen.load_json(os.path.join(path, entry['file']))
        img = preview_hex.render(path, entry['id'], 0, units=False)
        g = hex_geom.Geom(layout, game['view']['tileSize'])
        w, h = mp['grid']['width'], mp['grid']['height']
        holes = 0
        for y in range(h):
            for x in range(w):
                ox, oy = g.box_origin(x, y)
                for py in range(g.h):
                    for px in range(g.w):
                        if g.inside(px, py) and img.getpixel((ox + px, oy + py)) == (24, 26, 36, 255):
                            holes += 1
        check(holes == 0, '%s map %s: %d background pixels showing inside the hexagons' % (rel, entry['id'], holes))


def game_checks(rel):
    path = os.path.join(root, rel)
    game = gen.load_json(os.path.join(path, 'game.json'))
    check(game.get('name') and game.get('description'), '%s: name and description' % rel)
    check(game['grid']['type'] == 'hex', '%s: not a hexagon game' % rel)
    check(all(s.get('view', 'top-down') == 'top-down' for s in game.get('styles', [])), '%s: only top-down styles' % rel)
    if not os.path.isfile(sim):
        print('no ote-sim at %s: skipping the sim checks' % sim)
        return
    for entry in game['maps']:
        r = subprocess.run([sim, path, entry['id'], '--runs', '5'], capture_output=True, text=True)
        last = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ''
        check(r.returncode == 0 and 'summary: 5 runs' in last, '%s %s: sim failed: %s %s' % (rel, entry['id'], last, r.stderr[-300:]))
        check('stalls 0' in last, '%s %s: the sim stalled: %s' % (rel, entry['id'], last))


def main():
    generator_checks()
    geometry_checks()
    for rel, layout in GAMES.items():
        art_checks(rel, layout)
        composite_checks(rel, layout)
        game_checks(rel)
    for m in failures:
        print('FAIL:', m)
    print('hex_content_test: %d checks, %d failed' % (checks, len(failures)))
    sys.exit(1 if failures else 0)


main()
