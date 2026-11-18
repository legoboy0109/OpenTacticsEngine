// Draws a map the way the game's own placeholder look does (engine/player/battle/battle_draw.cpp):
// terrain colour + faint letter, owner border in the side's colour, unit block in the
// side's colour with the type's letter, yellow leader marker. All coordinates are device pixels
// rounded to whole pixels so edges stay crisp at any zoom and devicePixelRatio.
import { drawTerrainArt } from '../../../art/draw'
import { PLAIN_STYLE } from '../../../art/scene'
import type { Cell } from '../../../model/mapGrid'
import type { MapFile, MapUnit } from '../../../model/types'
import { css, FONT, mix, parseHex, readableOn } from './drawColors'
import { drawFogDim } from './drawFog'
import { drawHexMap } from './drawHex'
import { drawOverlays } from './drawStory'
import { drawOrder, drawStructure, drawStructureGhost, structureRows } from './drawStructures'
import type { Scene } from './drawTypes'
import type { DrawState } from './drawTypes'
import { drawAnyUnit, drawFlag, drawStackMark, glyphOfType } from './drawUnits'

export { parseHex, readableOn, glyphOfType }
export type { Scene, DrawState }

const BACKDROP = '#14161c'

export function drawMap(g: CanvasRenderingContext2D, scene: Scene, st: DrawState): void {
  if (scene.geo?.hex) return drawHexMap(g, scene, st)
  const { map } = scene
  const { dpr, view } = st
  const mw = map.grid.width, mh = map.grid.height
  const S = view.scale * dpr, ox = view.x * dpr, oy = view.y * dpr
  g.setTransform(1, 0, 0, 1, 0, 0)
  const style = scene.style ?? PLAIN_STYLE
  const art = scene.art ?? null
  const time = st.time ?? 0
  g.fillStyle = style.background ?? BACKDROP
  g.fillRect(0, 0, Math.ceil(st.w * dpr), Math.ceil(st.h * dpr))
  const x0 = Math.max(0, Math.floor(-ox / S)), x1 = Math.min(mw - 1, Math.floor((st.w * dpr - ox) / S))
  const y0 = Math.max(0, Math.floor(-oy / S)), y1 = Math.min(mh - 1, Math.floor((st.h * dpr - oy) / S))
  if (x1 < x0 || y1 < y0) return
  const px = (cx: number) => Math.round(ox + cx * S)
  const py = (cy: number) => Math.round(oy + cy * S)
  const showGlyph = S >= 14 * dpr
  const sideColor = (id: string) => parseHex(map.sides?.find((s) => s.id === id)?.color, [136, 136, 136])

  g.textAlign = 'center'
  g.textBaseline = 'middle'
  const idAt = (x: number, y: number) => map.legend[map.tiles[y]?.[x] ?? ''] ?? ''
  const wantGlyph = showGlyph && style.showGlyphs
  for (let y = y0; y <= y1; y++) {
    const row = map.tiles[y] ?? ''
    for (let x = x0; x <= x1; x++) {
      const tid = map.legend[row[x]] ?? ''
      const t = scene.terrain.get(tid)
      const col = parseHex(style.terrainColors[tid] ?? t?.color)
      const cx = px(x), cy = py(y), cw = px(x + 1) - cx, ch = py(y + 1) - cy
      g.fillStyle = css(col)
      g.fillRect(cx, cy, cw, ch)
      // Art replaces the letter and the faint outline; the colour stays underneath for transparent pixels.
      if (art && drawTerrainArt(g, art, tid, x, y, mw, mh, idAt, time, cx, cy, cw, ch)) continue
      const glyph = t?.glyph
      if (wantGlyph && glyph && glyph !== ' ') {
        g.fillStyle = css(mix(col, readableOn(col), 0.28))
        g.font = `bold ${Math.round(S * 0.45)}px ${FONT}`
        g.fillText(glyph, cx + cw / 2, cy + ch / 2 + 1)
      }
      // With art on the map the outline is drawn per square (art squares have none); without, one pass below.
      if (art && S >= 8 * dpr) {
        g.fillStyle = 'rgba(0,0,0,0.11)'
        g.fillRect(cx, cy, cw, 1); g.fillRect(cx, cy, 1, ch)
        if (x === mw - 1) g.fillRect(cx + cw, cy, 1, ch)
        if (y === mh - 1) g.fillRect(cx, cy + ch, cw, 1)
      }
    }
  }
  if (!art && S >= 8 * dpr) {
    g.fillStyle = 'rgba(0,0,0,0.11)'
    for (let x = x0; x <= x1 + 1; x++) g.fillRect(px(x), py(y0), 1, py(y1 + 1) - py(y0))
    for (let y = y0; y <= y1 + 1; y++) g.fillRect(px(x0), py(y), px(x1 + 1) - px(x0), 1)
  }
  const grid = { px, py, S }
  const placed = map.structures ?? []
  const glyphOn = showGlyph || S >= 9 * dpr
  const reach = art?.tallRows ?? 0
  const unitVisible = (u: MapUnit) => u.x >= x0 && u.x <= x1 && u.y >= y0 && u.y <= y1 + reach
  const paintUnit = (u: MapUnit) => {
    const ux = px(u.x), uy = py(u.y), uw = px(u.x + 1) - ux, uh = py(u.y + 1) - uy
    drawAnyUnit(g, scene, u, ux, uy, uw, uh, sideColor(u.side), 1, glyphOn, time)
    if (u.y <= y1 && scene.flagged.has(`${u.x},${u.y}`)) drawFlag(g, ux, uy, uw, S)
  }
  if (placed.length === 0) {
    // Top row first, so lower units overlap higher ones (only matters for sprites taller than their square).
    const units = art && map.units ? map.units.slice().sort((a, b) => a.y - b.y) : map.units ?? []
    for (const u of units) if (unitVisible(u)) paintUnit(u)
  } else {
    // Structures and units in one pass, by the row of their bottom edge (drawStructures.ts).
    for (const it of drawOrder(scene)) {
      if (it.kind === 1) { const u = map.units![it.index]; if (unitVisible(u)) paintUnit(u); continue }
      const s = placed[it.index]
      const def = scene.structures.get(s.type)
      const w = def?.width ?? 1
      const [top, bottom] = structureRows(scene, s)
      if (s.x + w - 1 < x0 || s.x > x1 || bottom < y0 || top > y1) continue
      drawStructure(g, scene, grid, s, time, glyphOn)
    }
  }
  // Per-difficulty variants share a square: the selected one is drawn on top, and a small count marks the square (docs/DIFFICULTY.md).
  const stacks = new Map<string, number[]>()
  ;(map.units ?? []).forEach((u, i) => { const k = `${u.x},${u.y}`; stacks.set(k, [...(stacks.get(k) ?? []), i]) })
  for (const idx of stacks.values()) {
    if (idx.length < 2) continue
    const u0 = map.units![idx[0]]
    if (!unitVisible(u0)) continue
    const sel = st.selected && st.selected.x === u0.x && st.selected.y === u0.y ? idx[(st.selected.v ?? 0) % idx.length] : idx[0]
    if (sel !== idx[idx.length - 1]) paintUnit(map.units![sel])
    drawStackMark(g, px(u0.x), py(u0.y), px(u0.x + 1) - px(u0.x), idx.length)
  }
  if (scene.showHeights && map.heights) drawHeights(g, map, x0, x1, y0, y1, px, py, S, dpr)
  if (st.structureGhost) drawStructureGhost(g, scene, grid, st.structureGhost, time)
  if (st.ghost) {
    const stack = (map.units ?? []).filter((m) => m.x === st.ghost!.from.x && m.y === st.ghost!.from.y)
    const u = stack.length ? stack[(st.ghost.from.v ?? 0) % stack.length] : undefined
    const t = st.ghost.to
    if (u && t.x >= 0 && t.y >= 0 && t.x < mw && t.y < mh) {
      drawAnyUnit(g, scene, u, px(t.x), py(t.y), px(t.x + 1) - px(t.x), py(t.y + 1) - py(t.y), sideColor(u.side), 0.65, true, time)
    }
  }
  if (st.fogSeen && st.fogSeen.length === mw * mh) drawFogDim(g, st.fogSeen, mw, x0, x1, y0, y1, px, py)
  if (st.overlays?.length || st.slots?.length || st.pickRect) drawOverlays(g, [...(st.slots ?? []), ...(st.overlays ?? [])], st.pickRect ?? null, { px, py, S, dpr, mw, mh })
  const ring = (c: Cell, color: string, width: number, inset = 0, size = { w: 1, h: 1 }) => {
    g.strokeStyle = color
    g.lineWidth = width
    const h = width / 2 + inset
    g.strokeRect(px(c.x) + h, py(c.y) + h, px(c.x + size.w) - px(c.x) - 2 * h, py(c.y + size.h) - py(c.y) - 2 * h)
  }
  const line = Math.max(1, Math.round(dpr * 1.5))
  if (st.hover && !st.structureGhost) { g.fillStyle = 'rgba(255,255,255,0.22)'; g.fillRect(px(st.hover.x), py(st.hover.y), px(st.hover.x + 1) - px(st.hover.x), py(st.hover.y + 1) - py(st.hover.y)) }
  if (st.selected) { ring(st.selected, 'rgba(0,0,0,0.85)', line * 3, 0, st.selSize ?? undefined); ring(st.selected, '#ffffff', line * 2, line * 0.5, st.selSize ?? undefined) }
  if (st.cursor) { ring(st.cursor, '#ffe45c', line * 2); ring(st.cursor, '#ffffff', line, line * 2) }
}

/** Squares above ground are lightened, squares below darkened, and (when big enough) the number is written in the corner. */
function drawHeights(g: CanvasRenderingContext2D, map: MapFile, x0: number, x1: number, y0: number, y1: number, px: (c: number) => number, py: (c: number) => number, S: number, dpr: number): void {
  const size = Math.max(9 * dpr, Math.round(S * 0.34))
  g.font = `bold ${size}px ${FONT}`
  g.textAlign = 'left'
  g.textBaseline = 'top'
  g.lineJoin = 'round'
  g.lineWidth = Math.max(2, Math.round(size / 4))
  for (let y = y0; y <= y1; y++) {
    for (let x = x0; x <= x1; x++) {
      const v = map.heights?.[y]?.[x]
      if (typeof v !== 'number' || v === 0) continue
      const cx = px(x), cy = py(y)
      g.fillStyle = v > 0 ? `rgba(255,255,255,${Math.min(0.5, 0.1 + v * 0.025)})` : `rgba(0,0,0,${Math.min(0.5, 0.15 + -v * 0.05)})`
      g.fillRect(cx, cy, px(x + 1) - cx, py(y + 1) - cy)
      if (S < 10 * dpr) continue
      const label = String(v)
      g.strokeStyle = 'rgba(0,0,0,0.85)'
      g.strokeText(label, cx + 2 * dpr, cy + 1 * dpr)
      g.fillStyle = '#ffffff'
      g.fillText(label, cx + 2 * dpr, cy + 1 * dpr)
    }
  }
  g.textAlign = 'center'
  g.textBaseline = 'middle'
}
