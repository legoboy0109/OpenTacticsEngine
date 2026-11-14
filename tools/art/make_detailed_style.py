#!/usr/bin/env python3
"""Builds demo-srpg's "Detailed 3D" style from public-domain (CC0) packs.

The style exists to try the 3D views with detailed art: smooth 512-pixel
ground pictures and animated character models. The source packs are not in
this repository; download them (see games/demo-srpg/CREDITS.md) and point
this script at the folders:

  make_detailed_style.py --kaykit DIR --textures DIR [repo]

  DIR for --kaykit:   holds Knight.glb, Barbarian.glb, Mage.glb, Rogue_Hooded.glb
  DIR for --textures: holds the *_Color.jpg files of the ambientCG materials below

It writes art/detailed_tiles.png, models/detailed/*.glb and
art/detailed.style.json (the piece of graphics.json for the style, which
make_starter_art.py merges in), then runs make_starter_art.py.
"""
import argparse
import json
import math
import os
import random
import subprocess
import sys

from PIL import Image, ImageFilter

import trim_glb

GAME = 'games/demo-srpg'
STYLE = 'detailed-3d'
CELL = 512

# Sheet cells, left to right, top to bottom (4 x 2). (material, which quarter of its 1024-pixel
# picture; None = the whole picture shrunk). Plain uses the four quarters of one grass picture
# as its variants, so neighbouring squares differ without changing colour.
GROUND = [('Grass004', 0), ('Grass004', 1), ('Grass004', 2), ('Grass004', 3),
          ('Ground037', None), ('Rock030', None), (None, 'water'), ('Planks012', None)]
COLUMNS = 4
TERRAIN = {
    'plain': {'variants': [[0, 0], [1, 0], [2, 0], [3, 0]]},
    'forest': {'cell': [0, 1]},
    'mountain': {'cell': [1, 1]},
    'water': {'cell': [2, 1]},
    'bridge': {'cell': [3, 1]},
}

# unit type -> (source model, parts left out, animations as state=clip)
UNITS = {
    'lord': ('Knight', ['1H_Sword_Offhand', 'Rectangle_Shield', 'Round_Shield', 'Spike_Shield', '2H_Sword', 'Knight_Helmet'],
             {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Chop', 'Die': 'Death_A'}),
    'knight': ('Knight', ['1H_Sword_Offhand', 'Badge_Shield', 'Round_Shield', 'Spike_Shield', '2H_Sword'],
               {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Stab', 'Die': 'Death_A'}),
    'cavalier': ('Knight', ['1H_Sword_Offhand', 'Badge_Shield', 'Rectangle_Shield', 'Round_Shield', 'Spike_Shield',
                            '1H_Sword', 'Knight_Cape'],
                 {'Idle': '2H_Melee_Idle', 'Move': 'Running_A', 'Attack': '2H_Melee_Attack_Slice', 'Die': 'Death_A'}),
    'fighter': ('Barbarian', ['1H_Axe_Offhand', 'Barbarian_Round_Shield', '1H_Axe', 'Mug'],
                {'Idle': '2H_Melee_Idle', 'Move': 'Walking_A', 'Attack': '2H_Melee_Attack_Chop', 'Die': 'Death_B'}),
    'soldier': ('Barbarian', ['1H_Axe_Offhand', '2H_Axe', 'Mug', 'Barbarian_Hat', 'Barbarian_Cape'],
                {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '1H_Melee_Attack_Slice_Diagonal', 'Die': 'Death_A'}),
    'archer': ('Rogue_Hooded', ['Knife_Offhand', '1H_Crossbow', 'Knife', 'Throwable'],
               {'Idle': 'Idle', 'Move': 'Walking_A', 'Attack': '2H_Ranged_Shoot', 'Die': 'Death_A'}),
    'pegasus': ('Mage', ['Spellbook', 'Spellbook_open', '1H_Wand'],
                {'Idle': 'Idle', 'Move': 'Running_A', 'Attack': 'Spellcast_Shoot', 'Die': 'Death_A'}),
}


def water(size, seed=7):
    """A calm blue surface with soft light streaks; wraps around at the edges."""
    rnd = random.Random(seed)
    small = Image.new('L', (size // 8, size // 8))
    small.putdata([rnd.randrange(256) for _ in range(small.width * small.height)])
    # Blur with wrap-around: tile 3 x 3, blur, cut the middle.
    def soft(img, radius):
        big = Image.new('L', (img.width * 3, img.height * 3))
        for y in range(3):
            for x in range(3):
                big.paste(img, (x * img.width, y * img.height))
        big = big.filter(ImageFilter.GaussianBlur(radius))
        return big.crop((img.width, img.height, img.width * 2, img.height * 2))
    noise = soft(small.resize((size, size // 4), Image.BICUBIC).resize((size, size), Image.BICUBIC), size / 40)
    px = noise.load()
    out = Image.new('RGB', (size, size))
    op = out.load()
    for y in range(size):
        for x in range(size):
            n = px[x, y] / 255.0
            streak = max(0.0, math.sin(n * 18.0)) ** 6      # thin bright lines along the noise's contours
            deep = 0.75 + 0.5 * (n - 0.5)
            op[x, y] = (int(min(255, 38 * deep + 150 * streak)), int(min(255, 108 * deep + 120 * streak)),
                        int(min(255, 178 * deep + 70 * streak)))
    return out


def ground_sheet(tex_dir):
    rows = (len(GROUND) + COLUMNS - 1) // COLUMNS
    sheet = Image.new('RGB', (CELL * COLUMNS, CELL * rows))
    for i, (material, part) in enumerate(GROUND):
        if material is None:
            cell = water(CELL)
        else:
            src = os.path.join(tex_dir, '%s_1K-JPG_Color.jpg' % material)
            if not os.path.exists(src):
                sys.exit('missing %s (see CREDITS.md for where to get it)' % src)
            img = Image.open(src).convert('RGB')
            if part is None:
                cell = img.resize((CELL, CELL), Image.LANCZOS)
            else:
                half = img.width // 2
                x, y = (part % 2) * half, (part // 2) * half
                cell = img.crop((x, y, x + half, y + half)).resize((CELL, CELL), Image.LANCZOS)
        sheet.paste(cell, ((i % COLUMNS) * CELL, (i // COLUMNS) * CELL))
    return sheet


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--kaykit', required=True)
    ap.add_argument('--textures', required=True)
    ap.add_argument('repo', nargs='?', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    args = ap.parse_args()
    game = os.path.join(args.repo, GAME)

    ground_sheet(args.textures).save(os.path.join(game, 'art', 'detailed_tiles.png'), optimize=True)

    out_dir = os.path.join(game, 'models', 'detailed')
    os.makedirs(out_dir, exist_ok=True)
    units = {}
    for uid, (source, drop, clips) in UNITS.items():
        doc, blob = trim_glb.read_glb(os.path.join(args.kaykit, source + '.glb'))
        doc, blob = trim_glb.trim(doc, blob, list(clips.items()), set(drop))
        trim_glb.write_glb(os.path.join(out_dir, uid + '.glb'), doc, blob)
        # These models have one material for everything, so nothing takes the side's colour;
        # the marker under the unit's feet shows the side.
        units[uid] = {'model': 'models/detailed/%s.glb' % uid, 'height': 1.15, 'sideMaterial': ''}

    piece = {
        'sheets': [{'id': 'detailed_tiles', 'file': 'art/detailed_tiles.png', 'cellWidth': CELL, 'cellHeight': CELL,
                    'smooth': True}],
        'styles': {STYLE: {'terrain': {tid: dict(look, sheet='detailed_tiles') for tid, look in TERRAIN.items()},
                           'units': units}},
    }
    with open(os.path.join(game, 'art', 'detailed.style.json'), 'w', encoding='utf-8') as f:
        f.write('// The "Detailed 3D" style. Generated by tools/art/make_detailed_style.py; merged into\n'
                '// data/graphics.json by tools/art/make_starter_art.py.\n' + json.dumps(piece, indent=2) + '\n')
    subprocess.check_call([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'make_starter_art.py'),
                           args.repo])


if __name__ == '__main__':
    main()
