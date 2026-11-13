// Draws a map the way the game's own placeholder look does (engine/player/battle/battle_draw.cpp):
// terrain colour + faint letter, owner border in the side's colour, unit block in the
// side's colour with the type's letter, yellow leader marker. All coordinates are device pixels
// rounded to whole pixels so edges stay crisp at any zoom and devicePixelRatio.
import { drawTerrainArt, drawUnitArt } from '../../../art/draw'
import { PLAIN_STYLE, unitPicture, type ArtScene, type StyleView } from '../../../art/scene'
import type { Cell } from '../../../model/mapGrid'
import type { MapFile, MapUnit, Terrain, Unit } from '../../../model/types'
import type { View } from './geometry'

export interface Scene {
  map: MapFile
  terrain: Map<string, Terrain>
  types: Map<string, Unit>
  /** "x,y" of units standing where they cannot be. */
  flagged: Set<string>
  /** Pictures (null = the game has none: everything keeps the placeholder look). */
  art?: ArtScene | null
  /** The visual style being previewed: background, terrain colours, letters on/off. */
  style?: StyleView
}

export interface DrawState {
  dpr: number
  w: number
  h: number
  view: View
  hover: Cell | null
  cursor: Cell | null
  selected: Cell | null
  ghost: { from: Cell; to: Cell } | null
  /** Running time in ms for animated pictures (0 under reduced motion). */
  time?: number
}

const BACKDROP = '#14161c'
const FONT = 'system-ui, -apple-system, "Segoe UI", Roboto, sans-serif'

type Rgb = [number, number, number]
export function parseHex(c: string | undefined, fallback: Rgb = [128, 128, 128]): Rgb {
  const m = /^#?([0-9a-f]{6})$/i.exec(c ?? '')
  if (!m) return fallback
  const n = parseInt(m[1], 16)
  return [n >> 16, (n >> 8) & 255, n & 255]
}
const css = (c: Rgb, a = 1) => `rgba(${c[0]},${c[1]},${c[2]},${a})`
const luma = (c: Rgb) => 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
/** Black or white, whichever reads on `c` (the game's readableOn). */
export const readableOn = (c: Rgb): Rgb => (luma(c) > 140 ? [18, 18, 24] : [245, 245, 250])
const mix = (a: Rgb, b: Rgb, t: number): Rgb => [0, 1, 2].map((i) => Math.round(a[i] + (b[i] - a[i]) * t)) as Rgb

export const glyphOfType = (u: Unit | undefined, fallback: string): string => (u?.glyph || u?.name?.[0]?.toUpperCase() || fallback).slice(0, 1)

export function drawMap(g: CanvasRenderingContext2D, scene: Scene, st: DrawState): void {
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
  const bw = Math.max(2, Math.round(S / 12))
  for (const o of map.owners ?? []) {
    if (o.x < x0 || o.x > x1 || o.y < y0 || o.y > y1) continue
    const cx = px(o.x), cy = py(o.y), cw = px(o.x + 1) - cx, ch = py(o.y + 1) - cy
    g.fillStyle = css(sideColor(o.side))
    g.fillRect(cx, cy, cw, bw); g.fillRect(cx, cy + ch - bw, cw, bw)
    g.fillRect(cx, cy + bw, bw, ch - 2 * bw); g.fillRect(cx + cw - bw, cy + bw, bw, ch - 2 * bw)
  }
  // Top row first, so lower units overlap higher ones (only matters for sprites taller than their square).
  const units = art && map.units ? map.units.slice().sort((a, b) => a.y - b.y) : map.units ?? []
  const reach = art?.tallRows ?? 0
  for (const u of units) {
    if (u.x < x0 || u.x > x1 || u.y < y0 || u.y > y1 + reach) continue
    const ux = px(u.x), uy = py(u.y), uw = px(u.x + 1) - ux, uh = py(u.y + 1) - uy
    drawAnyUnit(g, scene, u, ux, uy, uw, uh, sideColor(u.side), 1, showGlyph || S >= 9 * dpr, time)
    if (u.y <= y1 && scene.flagged.has(`${u.x},${u.y}`)) drawFlag(g, ux, uy, uw, S)
  }
  if (st.ghost) {
    const u = map.units?.find((m) => m.x === st.ghost!.from.x && m.y === st.ghost!.from.y)
    const t = st.ghost.to
    if (u && t.x >= 0 && t.y >= 0 && t.x < mw && t.y < mh) {
      drawAnyUnit(g, scene, u, px(t.x), py(t.y), px(t.x + 1) - px(t.x), py(t.y + 1) - py(t.y), sideColor(u.side), 0.65, true, time)
    }
  }
  const ring = (c: Cell, color: string, width: number, inset = 0) => {
    g.strokeStyle = color
    g.lineWidth = width
    const h = width / 2 + inset
    g.strokeRect(px(c.x) + h, py(c.y) + h, px(c.x + 1) - px(c.x) - 2 * h, py(c.y + 1) - py(c.y) - 2 * h)
  }
  const line = Math.max(1, Math.round(dpr * 1.5))
  if (st.hover) { g.fillStyle = 'rgba(255,255,255,0.22)'; g.fillRect(px(st.hover.x), py(st.hover.y), px(st.hover.x + 1) - px(st.hover.x), py(st.hover.y + 1) - py(st.hover.y)) }
  if (st.selected) { ring(st.selected, 'rgba(0,0,0,0.85)', line * 3); ring(st.selected, '#ffffff', line * 2, line * 0.5) }
  if (st.cursor) { ring(st.cursor, '#ffe45c', line * 2); ring(st.cursor, '#ffffff', line, line * 2) }
}

/** A unit with a picture is drawn from it; every other unit keeps the coloured block with its letter. */
function drawAnyUnit(g: CanvasRenderingContext2D, scene: Scene, u: MapUnit, cx: number, cy: number, cw: number, ch: number, side: Rgb, alpha: number, glyph: boolean, time: number): void {
  const pic = unitPicture(scene.art ?? null, u.type, u.side)
  if (pic && drawUnitArt(g, pic, { on: scene.art!.marker, color: css(side) }, time, cx, cy, cw, ch, alpha)) {
    if (u.leader) drawLeaderPip(g, cx, cy, cw, alpha)
    return
  }
  drawUnit(g, scene, u, cx, cy, cw, ch, side, alpha, glyph)
}

function drawLeaderPip(g: CanvasRenderingContext2D, cx: number, cy: number, cw: number, alpha: number): void {
  const k = cw / 24
  const m = Math.max(3, Math.round(5 * k)), inset = Math.max(1, Math.round(3 * k))
  g.fillStyle = `rgba(0,0,0,${0.86 * alpha})`
  g.fillRect(cx + inset - 1, cy + inset - 1, m, m)
  g.fillStyle = `rgba(255,210,40,${alpha})`
  g.fillRect(cx + inset, cy + inset, m - 2, m - 2)
}

function drawUnit(g: CanvasRenderingContext2D, scene: Scene, u: MapUnit, cx: number, cy: number, cw: number, ch: number, side: Rgb, alpha: number, glyph: boolean): void {
  const k = cw / 24
  const inset = Math.max(1, Math.round(3 * k))
  const bx = cx + inset, by = cy + inset, bw = cw - 2 * inset, bh = ch - 2 * inset - Math.round(3 * k)
  if (bw < 2 || bh < 2) return
  g.fillStyle = `rgba(0,0,0,${0.78 * alpha})`
  g.fillRect(bx - 1, by - 1, bw + 2, bh + 2)
  g.fillStyle = css(side, alpha)
  g.fillRect(bx, by, bw, bh)
  g.fillStyle = css(mix(side, [255, 255, 255], 0.35), alpha)
  g.fillRect(bx, by, bw, 1)
  if (glyph) {
    g.fillStyle = css(readableOn(side), alpha)
    g.font = `bold ${Math.round(Math.min(bw, bh) * 0.75)}px ${FONT}`
    g.fillText(glyphOfType(scene.types.get(u.type), '?'), bx + bw / 2, by + bh / 2 + 1)
  }
  if (u.leader) {
    const m = Math.max(3, Math.round(5 * k))
    g.fillStyle = `rgba(0,0,0,${0.86 * alpha})`
    g.fillRect(bx - 1, by - 1, m, m)
    g.fillStyle = `rgba(255,210,40,${alpha})`
    g.fillRect(bx, by, m - 2, m - 2)
  }
}

/** Red corner mark on a unit that stands on terrain it cannot enter. */
function drawFlag(g: CanvasRenderingContext2D, cx: number, cy: number, cw: number, S: number): void {
  const m = Math.max(6, Math.round(S * 0.36))
  g.fillStyle = '#e5322d'
  g.beginPath()
  g.moveTo(cx + cw - m, cy); g.lineTo(cx + cw, cy); g.lineTo(cx + cw, cy + m); g.closePath()
  g.fill()
  const lw = Math.max(2, Math.round(S / 14))
  g.strokeStyle = '#e5322d'
  g.lineWidth = lw
  g.strokeRect(cx + lw / 2, cy + lw / 2, cw - lw, cw - lw)
}
