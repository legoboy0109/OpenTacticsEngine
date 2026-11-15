// Structures on the map canvas, drawn the way the player draws them (engine/player/battle/structure_draw.cpp and
// battle_draw.cpp; docs/STRUCTURES.md "Drawing"):
//   - structures and units share one pass sorted by the row of their bottom edge, structures first on a tie; a unit
//     standing on a structure sorts with that structure's bottom edge, so it is drawn over the structure it stands on;
//   - without a picture: a block over the whole footprint (inset one pixel, one-pixel dark outline, a soft highlight on
//     top) with its letter centred, in the structure's colour (or the style's `structureColors` entry);
//   - with a picture: one piece across the whole footprint's width, height keeping the piece's proportions, standing on
//     the footprint's bottom edge (it may stick out above); the owner's own picture when it has one;
//   - the owner's colour as a two-pixel frame (of a 24 px square) around the whole footprint.
import { drawPiece } from '../../../art/draw'
import { pickCell } from '../../../model/pickCell'
import { structurePicture } from '../../../art/scene'
import { footprint, rectOf, type Rect } from '../../../model/mapStructures'
import type { MapStructure } from '../../../model/types'
import { css, FONT, mix, parseHex, readableOn, type Rgb } from './drawColors'
import type { Scene } from './drawTypes'

/** Where a square (or edge) is on the canvas, in device pixels. */
export interface Grid { px: (cx: number) => number; py: (cy: number) => number; S: number }

export interface DrawItem { key: number; kind: 0 | 1; index: number }

const rectOfPlaced = (scene: Scene, s: MapStructure): Rect => rectOf(scene.structures.get(s.type), s.x, s.y)

/** Structures and units in drawing order. Keys count whole squares: a structure's bottom edge, a unit's bottom edge
 *  (raised to the bottom edge of the structure it stands on). On hexagon maps the keys are the bottom edges of the boxes
 *  in tiles, so a lower row of hexagons is drawn over the one above it. */
export function drawOrder(scene: Scene): DrawItem[] {
  const placed = scene.map.structures ?? []
  const units = scene.map.units ?? []
  const geo = scene.geo
  let items: DrawItem[]
  if (geo?.hex) {
    const bottom = (c: { x: number; y: number }) => { const b = geo.box(c.x, c.y); return b.y + b.h }
    const cells = placed.map((s) => footprint(scene.grid, scene.structures.get(s.type), s.x, s.y))
    const keys = cells.map((cs) => Math.max(...cs.map(bottom)))
    items = placed.map((_, i) => ({ key: keys[i], kind: 0 as const, index: i }))
    units.forEach((u, i) => {
      let key = bottom(u)
      cells.forEach((cs, k) => { if (cs.some((c) => c.x === u.x && c.y === u.y)) key = Math.max(key, keys[k]) })
      items.push({ key, kind: 1, index: i })
    })
  } else {
    const rects = placed.map((s) => rectOfPlaced(scene, s))
    items = placed.map((_, i) => ({ key: rects[i].y + rects[i].h, kind: 0 as const, index: i }))
    units.forEach((u, i) => {
      let key = u.y + 1
      for (const r of rects) if (u.x >= r.x && u.y >= r.y && u.x < r.x + r.w && u.y < r.y + r.h) key = Math.max(key, r.y + r.h)
      items.push({ key, kind: 1, index: i })
    })
  }
  // Array.prototype.sort is stable: structures keep list order, units keep file order.
  return items.sort((a, b) => (a.key !== b.key ? a.key - b.key : a.kind - b.kind))
}

/** The squares of the picture of structure `s` that are on screen, as [top, bottom] rows (a tall picture sticks out above). */
export function structureRows(scene: Scene, s: MapStructure): [number, number] {
  const r = rectOfPlaced(scene, s)
  const pic = structurePicture(scene.art ?? null, s.type, s.side)
  const rows = pic ? Math.max(r.h, Math.ceil((r.w * pic.ch) / pic.cw)) : r.h
  return [r.y + r.h - rows, r.y + r.h - 1]
}

/** One structure: picture or block, then the owner's frame. `side` = the owner's colour (undefined = nobody). */
export function drawStructure(g: CanvasRenderingContext2D, scene: Scene, g2: Grid, s: MapStructure, time: number, glyph: boolean, alpha = 1): void {
  const r = rectOfPlaced(scene, s)
  const fx = g2.px(r.x), fy = g2.py(r.y), fw = g2.px(r.x + r.w) - fx, fh = g2.py(r.y + r.h) - fy
  const def = scene.structures.get(s.type)
  const pic = structurePicture(scene.art ?? null, s.type, s.side)
  let drawn = false
  if (pic) {
    const dh = Math.max(1, Math.round((fw * pic.ch) / pic.cw))
    g.globalAlpha = alpha
    drawn = drawPiece(g, pic, pickCell(pic.look, 0, 0, 0, time), fx, fy + fh - dh, fw, dh)
    g.globalAlpha = 1
  }
  if (!drawn) {
    const col = parseHex(scene.style?.structureColors[s.type] ?? def?.color)
    const k = Math.max(1, Math.round(g2.S / 24))
    if (fw > 2 * k + 2 && fh > 2 * k + 2) {
      g.fillStyle = `rgba(0,0,0,${0.78 * alpha})`
      g.fillRect(fx + k, fy + k, fw - 2 * k, fh - 2 * k)
      g.fillStyle = css(col, alpha)
      g.fillRect(fx + 2 * k, fy + 2 * k, fw - 4 * k, fh - 4 * k)
      g.fillStyle = css(mix(col, [255, 255, 255], 0.35), alpha)
      g.fillRect(fx + 2 * k, fy + 2 * k, fw - 4 * k, k)
      const letter = def?.glyph
      if (glyph && (scene.style?.showGlyphs ?? true) && letter && letter !== ' ') {
        g.fillStyle = css(readableOn(col), alpha)
        g.font = `bold ${Math.round(g2.S * 0.45 * Math.min(r.w, r.h))}px ${FONT}`
        g.textAlign = 'center'
        g.textBaseline = 'middle'
        g.fillText(letter, fx + fw / 2, fy + fh / 2 + 1)
      }
    }
  }
  if (s.side !== undefined) frame(g, scene, s.side, fx, fy, fw, fh, g2.S, alpha)
}

function frame(g: CanvasRenderingContext2D, scene: Scene, side: string, fx: number, fy: number, fw: number, fh: number, S: number, alpha: number): void {
  const col: Rgb = parseHex(scene.map.sides?.find((x) => x.id === side)?.color, [136, 136, 136])
  const bw = Math.max(2, Math.round(S / 12))
  g.fillStyle = css(col, alpha)
  g.fillRect(fx, fy, fw, bw); g.fillRect(fx, fy + fh - bw, fw, bw)
  g.fillRect(fx, fy + bw, bw, fh - 2 * bw); g.fillRect(fx + fw - bw, fy + bw, bw, fh - 2 * bw)
}

export interface StructureGhost { type: string; x: number; y: number; side?: string; ok: boolean }

/** The footprint a structure would take (placing it, or dragging one): translucent, red and crossed out where it cannot go. */
export function drawStructureGhost(g: CanvasRenderingContext2D, scene: Scene, g2: Grid, gh: StructureGhost, time: number): void {
  const s: MapStructure = { type: gh.type, x: gh.x, y: gh.y, ...(gh.side ? { side: gh.side } : {}) }
  drawStructure(g, scene, g2, s, time, true, 0.62)
  const r = rectOfPlaced(scene, s)
  const fx = g2.px(r.x), fy = g2.py(r.y), fw = g2.px(r.x + r.w) - fx, fh = g2.py(r.y + r.h) - fy
  const lw = Math.max(2, Math.round(g2.S / 12))
  g.lineWidth = lw
  g.setLineDash([lw * 3, lw * 2])
  g.strokeStyle = gh.ok ? 'rgba(255,255,255,0.95)' : 'rgba(229,50,45,1)'
  g.strokeRect(fx + lw / 2, fy + lw / 2, fw - lw, fh - lw)
  g.setLineDash([])
  if (!gh.ok) {
    g.fillStyle = 'rgba(229,50,45,0.38)'
    g.fillRect(fx, fy, fw, fh)
    g.strokeStyle = 'rgba(229,50,45,1)'
    g.beginPath(); g.moveTo(fx + lw, fy + lw); g.lineTo(fx + fw - lw, fy + fh - lw); g.moveTo(fx + fw - lw, fy + lw); g.lineTo(fx + lw, fy + fh - lw); g.stroke()
  }
}
