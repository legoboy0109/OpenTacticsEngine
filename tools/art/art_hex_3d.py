"""Starter art for the two 3D kinds of view on hexagons (docs/HEX.md "3D isometric and full 3D").

A hexagon game's top-down art already works in 3D (the top of a block is the terrain's top-down cell laid on the hexagon,
structures stand up as pictures), so a 3D style only adds what 3D can do that top-down cannot, as `demo-economy` does for
squares: standing trees and peaks on forest and mountain (the sheet `stand` of art_stand.py, 24 x 32 cells, unchanged) over
a plain ground cell of the hexagon's box (`ground3d`: the leafy floor and the rock floor of art_stand.py cut to the
hexagon), and the isometric unit sprites (art/iso_units.png, shared with the 2D isometric styles).
"""
import art_iso
import art_stand as st
from pix import Pix


def ground_sheet(g):
    """One cell per entry of art_stand.GROUND_ORDER: the 24 x 24 floor repeated over the hexagon's box, corners cut."""
    floors = [st.forest_floor(), st.rock_floor()]
    sheet = Pix(g.w * len(floors), g.h)
    for i, floor in enumerate(floors):
        cell = Pix(g.w, g.h)
        for y in range(g.h):
            for x in range(g.w):
                cell.p[y][x] = floor.p[y % st.T][x % st.T]
        sheet.paste(g.mask_out(cell), i * g.w, 0)
    return sheet


def make_3d_art(ids, g, terrain, structure_looks, units_here, sheets, art_dir, save):
    """`ids`: the styles of game.json whose view is "isometric-3d" or "3d". Writes ground3d.png, stand.png (and
    iso_units.png unless a 2D isometric style already made it); appends their sheets; returns graphics.json's "styles"."""
    if not ids:
        return {}
    have = {s['id'] for s in sheets}
    save(ground_sheet(g), 'ground3d.png')
    save(st.stand_sheet(), 'stand.png')
    sheets.append({'id': 'ground3d', 'file': 'art/ground3d.png', 'cellWidth': g.w, 'cellHeight': g.h})
    sheets.append({'id': 'stand', 'file': 'art/stand.png', 'cellWidth': st.SW, 'cellHeight': st.SH})
    looks = dict(terrain)
    looks.update(st.terrain_looks(list(terrain)))
    style = {'terrain': looks}
    if structure_looks:
        style['structures'] = structure_looks
    if units_here:
        if 'iso_units' not in have:
            save(art_iso.unit_sheet(units_here), 'iso_units.png')
            sheets.append({'id': 'iso_units', 'file': 'art/iso_units.png', 'cellWidth': art_iso.UNIT_W, 'cellHeight': art_iso.UNIT_H})
        style['units'] = {uid: {'sheet': 'iso_units', 'frames': [[0, row], [1, row]], 'frameMs': 600}
                          for row, uid in enumerate(units_here)}
    return {sid: style for sid in ids}
