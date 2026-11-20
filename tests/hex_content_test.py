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
- the isometric starter art (tools/art/art_hex_iso.py, geometry hex_iso_geom.py): the squashed hexagons tile exactly for many
  tile widths, every style of view "isometric" has a complete set, terrain cells are the documented size with the top face
  opaque exactly on the squashed hexagon and `step` rows of side faces under it, all 64 join pieces, structure pictures;
- the 3D starter art (tools/art/art_hex_3d.py): every style of view "isometric-3d" / "3d" has a complete set (top-down
  terrain, structures, isometric units), the ground cells under forest and mountain are the hexagon's box and opaque
  exactly inside the hexagon, the standing pictures are 24 x 32 and drawn;
- both templates and the demo load and their sims finish without a stall (needs ote-sim, default build/ote-sim);
"""
import json
import os
import shutil
import subprocess
import sys
from ote_testkit import scratch_dir  # noqa: E402

root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
sim = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(root, 'build', 'ote-sim')
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(root, 'tools', 'art'))
from PIL import Image  # noqa: E402

import hex_geom  # noqa: E402
import hex_iso_geom  # noqa: E402
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


def same_content(name, a, b):
    """Byte equality; for a PNG that differs only in how it was compressed (Pillow's bundled zlib differs between
    platforms), equal pictures count as equal: same size, mode and pixels."""
    if a == b:
        return True
    if a is None or b is None or not name.endswith('.png'):
        return False
    import io
    ia, ib = Image.open(io.BytesIO(a)), Image.open(io.BytesIO(b))
    return ia.size == ib.size and ia.mode == ib.mode and ia.tobytes() == ib.tobytes()


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
    tmp = scratch_dir()
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
            check(same_content(name, runs[0].get(name), committed.get(name)), '%s is not what make_hex_art.py generates' % name)
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


def iso_geometry_checks():
    """The squashed hexagons (hex_iso_geom.IsoGeom) tile exactly, whatever the tile width."""
    for layout in ('pointy', 'flat'):
        for tile in (16, 24, 32):
            for tw in list(range(8, 42, 2)) + [48, 56, 64, 72, 96]:
                ig = hex_iso_geom.IsoGeom(layout, tile, tw)
                owner = {}
                for y in range(6):
                    for x in range(6):
                        ox, oy = ig.box_origin(x, y)
                        for py in range(ig.h):
                            l, r = ig.row_span(py)
                            check(0 <= l < r <= ig.w, '%s %d/%d: row span' % (layout, tile, tw))
                            for px in range(ox + l, ox + r):
                                owner[(px, oy + py)] = owner.get((px, oy + py), 0) + 1
                check(all(v == 1 for v in owner.values()), '%s tile %d width %d: squashed hexagons overlap' % (layout, tile, tw))
                if layout == 'pointy':
                    x0, x1, y0, y1 = ig.w, 5 * ig.w, ig.h, 5 * ig.pitch
                else:
                    x0, x1, y0, y1 = ig.w, 5 * ig.pitch, ig.h, 5 * ig.h
                gaps = sum(1 for y in range(y0, y1) for x in range(x0, x1) if (x, y) not in owner)
                check(gaps == 0, '%s tile %d width %d: %d gaps between squashed hexagons' % (layout, tile, tw, gaps))
    p, f = hex_iso_geom.IsoGeom('pointy'), hex_iso_geom.IsoGeom('flat')
    check((p.w, p.h) == (48, 28) and (f.w, f.h) == (56, 24), 'default squashed boxes are 48x28 (pointy) and 56x24 (flat)')
    check(p.cell == (48, 60) and f.cell == (56, 56), 'terrain art cells are 48x60 and 56x56')


def iso_art_checks(rel, layout):
    path = os.path.join(root, rel)
    game = gen.load_json(os.path.join(path, 'game.json'))
    styles = [s for s in game.get('styles', []) if s.get('view') == 'isometric']
    check(len(styles) == 1, '%s: wants exactly one "isometric" style' % rel)
    if not styles:
        return
    check('tileHeight' not in styles[0].get('isometric', {}), '%s: tileHeight is refused on hexagon games' % rel)
    ig = hex_iso_geom.IsoGeom(layout, game['view']['tileSize'], styles[0].get('isometric', {}).get('tileWidth'))
    doc = gen.load_json(os.path.join(path, 'data/graphics.json'))
    look_set = doc.get('styles', {}).get(styles[0]['id'])
    check(look_set is not None, '%s: graphics.json has no art for the isometric style' % rel)
    if look_set is None:
        return
    terrain_ids = [t['id'] for t in gen.load_json(os.path.join(path, 'data/terrain.json'))['terrain']]
    structs = gen.load_json(os.path.join(path, 'data/structures.json'))['structures']
    units = [u['id'] for u in gen.load_json(os.path.join(path, 'data/units.json'))['units']]
    sheets = {}
    for s in doc['sheets']:
        sheets[s['id']] = (Image.open(os.path.join(path, s['file'])).convert('RGBA'), s['cellWidth'], s['cellHeight'])
    for t in terrain_ids:
        check(t in look_set['terrain'], '%s: isometric terrain %s has no look' % (rel, t))
    for s in structs:
        check(s['id'] in look_set.get('structures', {}), '%s: isometric structure %s has no look' % (rel, s['id']))
    for u in units:
        check(u in look_set.get('units', {}), '%s: isometric unit %s has no look' % (rel, u))

    def cell(sheet, col, row):
        im, cw, ch = sheets[sheet]
        return im.crop((col * cw, row * ch, (col + 1) * cw, (row + 1) * ch))

    def terrain_cell_ok(im, what):
        bad_top = bad_face = bad_out = 0
        for x in range(ig.w):
            last = ig.last_row(x)
            for y in range(im.height):
                a = im.getpixel((x, y))[3]
                ty = y - ig.rise
                if 0 <= ty < ig.h and ig.inside(x, ty):
                    bad_top += a != 255
                elif last >= 0 and ig.rise + last < y <= ig.rise + last + ig.step:
                    bad_face += a != 255
                elif last >= 0 and y > ig.rise + last + ig.step:
                    bad_out += a != 0          # nothing under the side faces
        check(bad_top == 0, '%s: %s has %d see-through pixels on its top face' % (rel, what, bad_top))
        check(bad_face == 0, '%s: %s has %d see-through pixels on its side faces' % (rel, what, bad_face))
        check(bad_out == 0, '%s: %s has %d painted pixels under its side faces' % (rel, what, bad_out))

    for tid, look in look_set['terrain'].items():
        sheet = look['sheet']
        check(sheet in sheets and sheets[sheet][1:] == ig.cell, '%s: isometric %s sheet cell size %s, want %s' % (
            rel, tid, sheets.get(sheet, (0, 0, 0))[1:], ig.cell))
        cells = [tuple(c) for c in look['variants']] if 'variants' in look else [tuple(look['cell'])] if 'cell' in look else []
        for c, r in cells:
            terrain_cell_ok(cell(sheet, c, r), 'iso %s %s' % (tid, (c, r)))
        if 'join' in look:
            blocks = look['join'] if isinstance(look['join'][0], list) else [look['join']]
            for c0, r0 in blocks:
                for n in range(64):
                    terrain_cell_ok(cell(sheet, c0 + n % 8, r0 + n // 8), 'iso %s piece %d' % (tid, n))
    for s in structs:
        look = look_set['structures'][s['id']]
        im, cw, ch = sheets[look['sheet']]
        want = ig.ring_size() if s.get('rings') else ig.single_size()
        check((cw, ch) == want, '%s: isometric structure %s picture is %dx%d, want %s' % (rel, s['id'], cw, ch, want))
        pic = cell(look['sheet'], *look['cell'])
        check(pic.getchannel('A').getbbox() is not None, '%s: isometric structure %s is empty' % (rel, s['id']))
        # stands on the front edge: something is painted in the last rows of the picture
        check(pic.getchannel('A').crop((0, ch - 3, cw, ch)).getbbox() is not None,
              '%s: isometric structure %s does not reach the bottom' % (rel, s['id']))
    for u in units:
        look = look_set['units'][u]
        check(look['sheet'] in sheets and sheets[look['sheet']][1] == 24, '%s: isometric unit %s cell width' % (rel, u))


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


def art3d_checks(rel, layout):
    """The 3D styles of a hexagon game: what art_hex_3d.py writes (the templates have no 3D style: nothing to check)."""
    path = os.path.join(root, rel)
    game = gen.load_json(os.path.join(path, 'game.json'))
    styles = [s for s in game.get('styles', []) if s.get('view') in ('isometric-3d', '3d')]
    if not styles:
        return
    doc = gen.load_json(os.path.join(path, 'data/graphics.json'))
    terrain_ids = [t['id'] for t in gen.load_json(os.path.join(path, 'data/terrain.json'))['terrain']]
    structs = gen.load_json(os.path.join(path, 'data/structures.json'))['structures']
    units = [u['id'] for u in gen.load_json(os.path.join(path, 'data/units.json'))['units']]
    sheets = {}
    for sh in doc['sheets']:
        sheets[sh['id']] = (Image.open(os.path.join(path, sh['file'])).convert('RGBA'), sh['cellWidth'], sh['cellHeight'])
    g = hex_geom.Geom(layout, game['view']['tileSize'])
    for st in styles:
        look_set = doc.get('styles', {}).get(st['id'])
        check(look_set is not None, '%s: graphics.json has no art for the 3D style %s' % (rel, st['id']))
        if look_set is None:
            continue
        for t in terrain_ids:
            check(t in look_set['terrain'], '%s: 3D terrain %s has no look' % (rel, t))
        for sd in structs:
            check(sd['id'] in look_set.get('structures', {}), '%s: 3D structure %s has no look' % (rel, sd['id']))
        for u in units:
            check(u in look_set.get('units', {}), '%s: 3D unit %s has no look' % (rel, u))

        def in_sheet(look):
            im, cw, ch = sheets[look['sheet']]
            if 'join' in look:  # the far corner of each 8 x 8 block of pieces
                j = look['join']
                cells = [(c + 7, r + 7) for c, r in ([j] if isinstance(j[0], int) else j)]
            else:
                cells = look.get('variants') or look.get('frames') or [look['cell']]
            return all((c + 1) * cw <= im.width and (r + 1) * ch <= im.height for c, r in cells)
        for t, look in look_set['terrain'].items():
            check(in_sheet(look), '%s: 3D terrain %s cell outside its sheet' % (rel, t))
            if 'stand' in look:
                check(in_sheet(look['stand']) and sheets[look['stand']['sheet']][1:] == (24, 32), '%s: standing picture of %s' % (rel, t))
                gim, gw, gh = sheets[look['sheet']]
                check((gw, gh) == (g.w, g.h), '%s: ground cell of %s is %dx%d, want the hexagon box %dx%d' % (rel, t, gw, gh, g.w, g.h))
                c, r = look['cell']
                cellim = gim.crop((c * gw, r * gh, (c + 1) * gw, (r + 1) * gh))
                bad = [(x, y) for y in range(gh) for x in range(gw) if (cellim.getpixel((x, y))[3] == 255) != g.inside(x, y)]
                check(not bad, '%s: ground cell of %s is not opaque exactly inside the hexagon: %s' % (rel, t, bad[:3]))
                for sc, sr in look['stand'].get('variants', []):
                    sim, sw, sh_ = sheets[look['stand']['sheet']]
                    check(sim.crop((sc * sw, sr * sh_, (sc + 1) * sw, (sr + 1) * sh_)).getchannel('A').getbbox() is not None,
                          '%s: standing picture %s of %s is empty' % (rel, (sc, sr), t))
        for sd in structs:
            sl = look_set['structures'].get(sd['id'])
            check(sl is not None and in_sheet(sl), '%s: 3D structure %s cell outside its sheet' % (rel, sd['id']))
        for u, look in look_set.get('units', {}).items():
            check(in_sheet(look) and sheets[look['sheet']][1] == 24, '%s: 3D unit %s' % (rel, u))


def game_checks(rel):
    path = os.path.join(root, rel)
    game = gen.load_json(os.path.join(path, 'game.json'))
    check(game.get('name') and game.get('description'), '%s: name and description' % rel)
    check(game['grid']['type'] == 'hex', '%s: not a hexagon game' % rel)
    check(all(s.get('view', 'top-down') in ('top-down', 'isometric', 'isometric-3d', '3d') for s in game.get('styles', [])),
          '%s: a style with an unknown kind of view' % rel)
    if rel == 'games/demo-hex':  # the demo has a style of every kind of view
        check({s.get('view', 'top-down') for s in game.get('styles', [])} == {'top-down', 'isometric', 'isometric-3d', '3d'},
              '%s: wants a style of each of the four kinds of view' % rel)
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
    iso_geometry_checks()
    for rel, layout in GAMES.items():
        art_checks(rel, layout)
        iso_art_checks(rel, layout)
        art3d_checks(rel, layout)
        composite_checks(rel, layout)
        game_checks(rel)
    for m in failures:
        print('FAIL:', m)
    print('hex_content_test: %d checks, %d failed' % (checks, len(failures)))
    sys.exit(1 if failures else 0)


main()
