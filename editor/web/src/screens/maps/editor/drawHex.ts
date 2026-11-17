// The map canvas for hexagon games (docs/HEX.md "Top-down picture"): terrain as filled hexagons or sheet cells in the
// hexagon's box, hexagon outlines, units centred on their hexagon, structures (one hexagon or a hexagon and its ring),
// highlights shaped like the hexagons. Square maps keep drawMap.ts; both read the same Scene and DrawState.
//
// Every point is worked out in "tiles" from model/mapGeo.ts and rounded to a device pixel by the same two functions
// (px, py), so corners that two hexagons share land on the same pixel and the hexagons tile without gaps.
import { drawPiece, drawTerrainArt } from '../../../art/draw'
import { PLAIN_STYLE, structurePicture } from '../../../art/scene'
import type { Geo } from '../../../model/mapGeo'
import type { Cell } from '../../../model/mapGrid'
import { footprint } from '../../../model/mapStructures'
import { pickCell } from '../../../model/pickCell'
import type { Overlay } from '../../../model/storyOverlays'
import type { Rect } from '../../../model/storyTypes'
import type { MapStructure, MapUnit } from '../../../model/types'
import { css, FONT, mix, parseHex, readableOn, type Rgb } from './drawColors'
import { drawOrder, type StructureGhost } from './drawStructures'
import type { DrawState, Scene } from './drawTypes'
import { drawAnyUnit } from './drawUnits'

const BACKDROP = '#14161c'

/** Tile -> device pixel, for one frame. */
interface K { px: (u: number) => number; py: (v: number) => number; S: number; dpr: number; geo: Geo }

/** Adds one hexagon to the current path; `inset` pulls its corners towards its middle by that many device pixels. */
function hexPath(g: CanvasRenderingContext2D, k: K, x: number, y: number, inset = 0): void {
  const b = k.geo.box(x, y)
  const left = k.px(b.x), right = k.px(b.x + b.w), top = k.py(b.y), bottom = k.py(b.y + b.h)
  const mx = (left + right) / 2, my = (top + bottom) / 2
  const f = inset > 0 ? Math.max(0, 1 - (2 * inset) / (right - left)) : 1
  k.geo.corners.forEach(([cx, cy], i) => {
    const X = k.px(b.x + cx), Y = k.py(b.y + cy)
    const x2 = inset > 0 ? mx + (X - mx) * f : X, y2 = inset > 0 ? my + (Y - my) * f : Y
    if (i === 0) g.moveTo(x2, y2); else g.lineTo(x2, y2)
  })
  g.closePath()
}

/** One path holding all the cells (filling it once leaves no seams between neighbours). */
function cellsPath(g: CanvasRenderingContext2D, k: K, cells: Cell[], inset = 0): void {
  g.beginPath()
  for (const c of cells) hexPath(g, k, c.x, c.y, inset)
}

/** The outline around a set of cells, drawn inside them: `width` device pixels wide, clipped to the cells. */
function strokeAround(g: CanvasRenderingContext2D, k: K, cells: Cell[], color: string, width: number, dash?: number[]): void {
  g.save()
  cellsPath(g, k, cells)
  g.clip()
  g.beginPath()
  for (const [a, b] of k.geo.outline(cells)) { g.moveTo(k.px(a[0]), k.py(a[1])); g.lineTo(k.px(b[0]), k.py(b[1])) }
  g.strokeStyle = color
  g.lineWidth = width * 2
  g.lineCap = 'round'
  if (dash) g.setLineDash(dash)
  g.stroke()
  g.restore()
}

/** The box around all the given cells, in device pixels. */
function boundsOf(k: K, cells: Cell[]): { x: number; y: number; w: number; h: number } {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity
  for (const c of cells) {
    const b = k.geo.box(c.x, c.y)
    x0 = Math.min(x0, b.x); y0 = Math.min(y0, b.y); x1 = Math.max(x1, b.x + b.w); y1 = Math.max(y1, b.y + b.h)
  }
  const x = k.px(x0), y = k.py(y0)
  return { x, y, w: k.px(x1) - x, h: k.py(y1) - y }
}

/** The tile-sized square a unit is drawn in: centred on its hexagon's middle (docs/HEX.md "Units"). */
function unitSquare(k: K, c: Cell): { x: number; y: number; w: number; h: number } {
  const b = k.geo.box(c.x, c.y)
  const mx = k.px(b.x + b.w / 2), my = k.py(b.y + b.h / 2)
  const s = Math.round(k.S)
  return { x: Math.round(mx - s / 2), y: Math.round(my - s / 2), w: s, h: s }
}

export function drawHexMap(g: CanvasRenderingContext2D, scene: Scene, st: DrawState): void {
  const { map } = scene
  const geo = scene.geo!
  const { dpr, view } = st
  const mw = map.grid.width, mh = map.grid.height
  const S = view.scale * dpr, ox = view.x * dpr, oy = view.y * dpr
  const k: K = { px: (u) => Math.round(ox + u * S), py: (v) => Math.round(oy + v * S), S, dpr, geo }
  g.setTransform(1, 0, 0, 1, 0, 0)
  const style = scene.style ?? PLAIN_STYLE
  const art = scene.art ?? null
  const time = st.time ?? 0
  const W = Math.ceil(st.w * dpr), H = Math.ceil(st.h * dpr)
  g.fillStyle = style.background ?? BACKDROP
  g.fillRect(0, 0, W, H)
  const r = geo.range(-ox / S, -oy / S, (W - ox) / S, (H - oy) / S)
  if (r.x1 < r.x0 || r.y1 < r.y0) return
  const sideColor = (id: string) => parseHex(map.sides?.find((s) => s.id === id)?.color, [136, 136, 136])
  const showGlyph = S >= 14 * dpr
  const wantGlyph = showGlyph && style.showGlyphs
  const idAt = (x: number, y: number) => map.legend[map.tiles[y]?.[x] ?? ''] ?? ''
  g.textAlign = 'center'
  g.textBaseline = 'middle'

  // Terrain. Each hexagon is filled and also stroked one pixel in the same colour so no seam of the backdrop shows between two
  // hexagons of the same colour (the diagonal edges are anti-aliased); a later hexagon simply paints over its neighbour's rim.
  const noArt: Cell[] = []
  for (let y = r.y0; y <= r.y1; y++) {
    for (let x = r.x0; x <= r.x1; x++) {
      const tid = idAt(x, y)
      const t = scene.terrain.get(tid)
      const col = parseHex(style.terrainColors[tid] ?? t?.color)
      g.fillStyle = g.strokeStyle = css(col)
      g.lineWidth = 1
      g.lineJoin = 'round'
      g.beginPath(); hexPath(g, k, x, y); g.fill()
      if (!art?.terrain.has(tid)) g.stroke()
      const b = geo.box(x, y)
      const cx = k.px(b.x), cy = k.py(b.y), cw = k.px(b.x + b.w) - cx, ch = k.py(b.y + b.h) - cy
      // Art replaces the letter and the faint outline; the colour stays underneath for the art's transparent corners.
      if (art && drawTerrainArt(g, art, tid, x, y, mw, mh, idAt, time, cx, cy, cw, ch, scene.grid)) continue
      noArt.push({ x, y })
      const glyph = t?.glyph
      if (wantGlyph && glyph && glyph !== ' ') {
        g.fillStyle = css(mix(col, readableOn(col), 0.28))
        g.font = `bold ${Math.round(S * 0.45)}px ${FONT}`
        g.fillText(glyph, cx + cw / 2, cy + ch / 2 + 1)
      }
    }
  }
  if (S >= 8 * dpr && noArt.length) {
    g.strokeStyle = 'rgba(0,0,0,0.11)'
    g.lineWidth = 1
    cellsPath(g, k, noArt)
    g.stroke()
  }

  // Units and structures, bottom edge first (drawStructures.ts), culled by their picture on the screen.
  const placed = map.structures ?? []
  const glyphOn = showGlyph || S >= 9 * dpr
  const onScreen = (x: number, y: number, w: number, h: number) => x + w > 0 && y + h > 0 && x < W && y < H
  const reach = art?.tallRows ?? 0
  const paintUnit = (u: MapUnit) => {
    const q = unitSquare(k, u)
    if (!onScreen(q.x, q.y - reach * q.h, q.w, q.h * (1 + reach))) return
    drawAnyUnit(g, scene, u, q.x, q.y, q.w, q.h, sideColor(u.side), 1, glyphOn, time)
    if (scene.flagged.has(`${u.x},${u.y}`)) drawFlagHex(g, k, u)
  }
  for (const it of drawOrder(scene)) {
    if (it.kind === 1) { paintUnit(map.units![it.index]); continue }
    const s = placed[it.index]
    const cells = footprint(scene.grid, scene.structures.get(s.type), s.x, s.y)
    const bb = boundsOf(k, cells)
    const pic = structurePicture(art, s.type, s.side)
    const rise = pic ? Math.max(0, Math.round((bb.w * pic.ch) / pic.cw) - bb.h) : 0
    if (!onScreen(bb.x, bb.y - rise, bb.w, bb.h + rise)) continue
    drawStructureHex(g, scene, k, s, time, glyphOn)
  }
  if (scene.showHeights && map.heights) drawHeightsHex(g, scene, k, r)
  if (st.structureGhost) drawGhostHex(g, scene, k, st.structureGhost, time)
  if (st.ghost) {
    const u = map.units?.find((m) => m.x === st.ghost!.from.x && m.y === st.ghost!.from.y)
    const t = st.ghost.to
    if (u && t.x >= 0 && t.y >= 0 && t.x < mw && t.y < mh) {
      const q = unitSquare(k, t)
      drawAnyUnit(g, scene, u, q.x, q.y, q.w, q.h, sideColor(u.side), 0.65, true, time)
    }
  }
  if (st.fogSeen && st.fogSeen.length === mw * mh) {
    // One path for every dimmed hexagon: a single fill leaves no seams.
    const dim: Cell[] = []
    for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) if (!st.fogSeen[y * mw + x]) dim.push({ x, y })
    g.fillStyle = 'rgba(6,8,18,0.58)'
    cellsPath(g, k, dim)
    g.fill()
  }
  if (st.overlays?.length || st.slots?.length || st.pickRect) drawOverlaysHex(g, k, mw, mh, [...(st.slots ?? []), ...(st.overlays ?? [])], st.pickRect ?? null)

  const line = Math.max(1, Math.round(dpr * 1.5))
  if (st.hover && !st.structureGhost) { g.fillStyle = 'rgba(255,255,255,0.22)'; cellsPath(g, k, [st.hover]); g.fill() }
  if (st.selected) {
    const cells = st.selCells?.length ? st.selCells : [st.selected]
    strokeAround(g, k, cells, 'rgba(0,0,0,0.85)', line * 3)
    strokeAround(g, k, cells, '#ffffff', line * 2.5 - 0.5)
  }
  if (st.cursor) {
    strokeAround(g, k, [st.cursor], '#ffe45c', line * 2)
    g.strokeStyle = '#ffffff'
    g.lineWidth = line
    cellsPath(g, k, [st.cursor], line * 2 + line / 2)
    g.stroke()
  }
}

/** A structure on a hexagon map: its picture across the box around every hexagon it covers (standing on the bottom edge), or each
 *  covered hexagon filled in its colour with the letter on the middle one; the owner's frame around the outline. */
function drawStructureHex(g: CanvasRenderingContext2D, scene: Scene, k: K, s: MapStructure, time: number, glyph: boolean, alpha = 1): void {
  const def = scene.structures.get(s.type)
  const cells = footprint(scene.grid, def, s.x, s.y)
  const bb = boundsOf(k, cells)
  const pic = structurePicture(scene.art ?? null, s.type, s.side)
  let drawn = false
  if (pic) {
    const dh = Math.max(1, Math.round((bb.w * pic.ch) / pic.cw))
    g.globalAlpha = alpha
    drawn = drawPiece(g, pic, pickCell(pic.look, 0, 0, 0, time), bb.x, bb.y + bb.h - dh, bb.w, dh)
    g.globalAlpha = 1
  }
  if (!drawn) {
    const col = parseHex(scene.style?.structureColors[s.type] ?? def?.color)
    const rim = Math.max(1, Math.round(k.S / 24))
    g.fillStyle = css(col, alpha)
    cellsPath(g, k, cells)
    g.fill()
    strokeAround(g, k, cells, `rgba(0,0,0,${0.78 * alpha})`, rim)
    const letter = def?.glyph
    if (glyph && (scene.style?.showGlyphs ?? true) && letter && letter !== ' ') {
      const b = k.geo.box(cells[0].x, cells[0].y)
      g.fillStyle = css(readableOn(col), alpha)
      g.font = `bold ${Math.round(k.S * 0.45 * (cells.length > 1 ? 1.4 : 1))}px ${FONT}`
      g.textAlign = 'center'
      g.textBaseline = 'middle'
      g.fillText(letter, (k.px(b.x) + k.px(b.x + b.w)) / 2, (k.py(b.y) + k.py(b.y + b.h)) / 2 + 1)
    }
  }
  if (s.side !== undefined) {
    const col: Rgb = parseHex(scene.map.sides?.find((x) => x.id === s.side)?.color, [136, 136, 136])
    strokeAround(g, k, cells, css(col, alpha), Math.max(2, Math.round(k.S / 12)))
  }
}

/** The footprint a structure would take: translucent, dashed outline, red and crossed out where it cannot go. */
function drawGhostHex(g: CanvasRenderingContext2D, scene: Scene, k: K, gh: StructureGhost, time: number): void {
  const s: MapStructure = { type: gh.type, x: gh.x, y: gh.y, ...(gh.side ? { side: gh.side } : {}) }
  drawStructureHex(g, scene, k, s, time, true, 0.62)
  const cells = footprint(scene.grid, scene.structures.get(gh.type), gh.x, gh.y)
  const lw = Math.max(2, Math.round(k.S / 12))
  if (!gh.ok) {
    g.fillStyle = 'rgba(229,50,45,0.38)'
    cellsPath(g, k, cells)
    g.fill()
  }
  strokeAround(g, k, cells, gh.ok ? 'rgba(255,255,255,0.95)' : 'rgba(229,50,45,1)', lw, [lw * 3, lw * 2])
  if (!gh.ok) {
    const b = k.geo.box(cells[0].x, cells[0].y)
    const x0 = k.px(b.x), x1 = k.px(b.x + b.w), y0 = k.py(b.y), y1 = k.py(b.y + b.h)
    const m = (x1 - x0) * 0.22
    g.setLineDash([])
    g.strokeStyle = 'rgba(229,50,45,1)'
    g.lineWidth = lw
    g.beginPath(); g.moveTo(x0 + m, y0 + m); g.lineTo(x1 - m, y1 - m); g.moveTo(x1 - m, y0 + m); g.lineTo(x0 + m, y1 - m); g.stroke()
  }
}

/** A unit that stands where it cannot be: its hexagon gets a red rim. */
function drawFlagHex(g: CanvasRenderingContext2D, k: K, c: Cell): void {
  const lw = Math.max(2, Math.round(k.S / 14))
  g.strokeStyle = '#e5322d'
  g.lineWidth = lw
  g.beginPath(); hexPath(g, k, c.x, c.y, lw / 2); g.stroke()
}

/** Cells above ground are lightened, cells below darkened (one fill per height, so no seams); the number sits in the middle. */
function drawHeightsHex(g: CanvasRenderingContext2D, scene: Scene, k: K, r: { x0: number; x1: number; y0: number; y1: number }): void {
  const map = scene.map
  const byValue = new Map<number, Cell[]>()
  for (let y = r.y0; y <= r.y1; y++) {
    for (let x = r.x0; x <= r.x1; x++) {
      const v = map.heights?.[y]?.[x]
      if (typeof v !== 'number' || v === 0) continue
      byValue.set(v, [...(byValue.get(v) ?? []), { x, y }])
    }
  }
  const size = Math.max(9 * k.dpr, Math.round(k.S * 0.34))
  g.font = `bold ${size}px ${FONT}`
  g.textAlign = 'center'
  g.textBaseline = 'middle'
  g.lineJoin = 'round'
  g.lineWidth = Math.max(2, Math.round(size / 4))
  for (const [v, cells] of byValue) {
    g.fillStyle = v > 0 ? `rgba(255,255,255,${Math.min(0.5, 0.1 + v * 0.025)})` : `rgba(0,0,0,${Math.min(0.5, 0.15 + -v * 0.05)})`
    cellsPath(g, k, cells)
    g.fill()
    if (k.S < 10 * k.dpr) continue
    for (const c of cells) {
      const b = k.geo.box(c.x, c.y)
      const mx = (k.px(b.x) + k.px(b.x + b.w)) / 2, my = (k.py(b.y) + k.py(b.y + b.h)) / 2
      g.strokeStyle = 'rgba(0,0,0,0.85)'
      g.strokeText(String(v), mx, my)
      g.fillStyle = '#ffffff'
      g.fillText(String(v), mx, my)
    }
  }
}

const TONES: Record<Overlay['tone'], string> = { area: '45,212,255', add: '124,211,90', move: '255,176,32', view: '197,140,255', win: '74,222,128', lose: '255,92,92', slot: '90,170,255', change: '255,120,200' }

/** The hexagons a rectangle of columns and rows contains (story areas stay rectangles of columns and rows, docs/HEX.md). */
export function rectCells(r: Rect, w: number, h: number): Cell[] {
  const out: Cell[] = []
  for (let y = Math.max(0, r.y); y < Math.min(h, r.y + r.h); y++) for (let x = Math.max(0, r.x); x < Math.min(w, r.x + r.w); x++) out.push({ x, y })
  return out
}

/** Story areas and the rectangle being picked, drawn as the set of hexagons they contain. */
function drawOverlaysHex(g: CanvasRenderingContext2D, k: K, mw: number, mh: number, overlays: Overlay[], pickRect: Rect | null): void {
  const line = Math.max(2, Math.round(k.dpr * 2))
  g.textAlign = 'left'
  g.textBaseline = 'top'
  for (const o of overlays) {
    const cells = rectCells(o, mw, mh)
    if (!cells.length) continue
    const rgb = TONES[o.tone]
    g.fillStyle = `rgba(${rgb},0.26)`
    cellsPath(g, k, cells)
    g.fill()
    strokeAround(g, k, cells, 'rgba(0,0,0,0.75)', line + 2)
    strokeAround(g, k, cells, `rgb(${rgb})`, line)
    if (k.S >= 18 * k.dpr) {  // (a deployment slot's number is short enough for any hexagon)
      const b = k.geo.box(cells[0].x, cells[0].y)
      const size = Math.max(9 * k.dpr, Math.min(14 * k.dpr, k.S * 0.36))
      g.font = `bold ${Math.round(size)}px ${FONT}`
      g.lineJoin = 'round'
      g.lineWidth = Math.max(2, Math.round(size / 4))
      g.strokeStyle = 'rgba(0,0,0,0.85)'
      g.strokeText(o.label, k.px(b.x) + line + 2, k.py(b.y) + line + 1)
      g.fillStyle = '#ffffff'
      g.fillText(o.label, k.px(b.x) + line + 2, k.py(b.y) + line + 1)
    }
  }
  const cells = pickRect ? rectCells(pickRect, mw, mh) : []
  if (cells.length) {
    g.fillStyle = 'rgba(255,228,92,0.28)'
    cellsPath(g, k, cells)
    g.fill()
    strokeAround(g, k, cells, 'rgba(0,0,0,0.8)', line + 2, [line * 3, line * 2])
    strokeAround(g, k, cells, '#ffe45c', line, [line * 3, line * 2])
  }
  g.setLineDash([])
  g.textAlign = 'center'
  g.textBaseline = 'middle'
}
