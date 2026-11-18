// Units on the map canvas (squares and hexagons): the picture on its base, or the coloured block with the type's letter.
// `cx, cy, cw, ch` is the square a unit stands in, in device pixels (on hexagons: a tile-sized square centred on the
// hexagon's middle, docs/HEX.md "Units").
import { drawUnitArt } from '../../../art/draw'
import { unitPicture } from '../../../art/scene'
import { behaviourMark } from '../../../model/behaviour'
import type { MapUnit, Unit } from '../../../model/types'
import { css, FONT, mix, readableOn, type Rgb } from './drawColors'
import type { Scene } from './drawTypes'

export const glyphOfType = (u: Unit | undefined, fallback: string): string => (u?.glyph || u?.name?.[0]?.toUpperCase() || fallback).slice(0, 1)

/** A unit with a picture is drawn from it; every other unit keeps the coloured block with its letter. */
export function drawAnyUnit(g: CanvasRenderingContext2D, scene: Scene, u: MapUnit, cx: number, cy: number, cw: number, ch: number, side: Rgb, alpha: number, glyph: boolean, time: number): void {
  const pic = unitPicture(scene.art ?? null, u.type ?? '', u.side)
  if (pic && drawUnitArt(g, pic, { on: scene.art!.marker, color: css(side) }, time, cx, cy, cw, ch, alpha)) {
    if (u.leader) drawLeaderPip(g, cx, cy, cw, alpha)
    drawBehaviourMark(g, u, cx, cy, cw, ch, alpha)
    return
  }
  drawUnit(g, scene, u, cx, cy, cw, ch, side, alpha, glyph)
  drawBehaviourMark(g, u, cx, cy, cw, ch, alpha)
}

/** A computer unit that does not simply advance (docs/AI.md) wears a small letter in its bottom right corner: H holds, G guards, S seeks, T starts late. */
function drawBehaviourMark(g: CanvasRenderingContext2D, u: MapUnit, cx: number, cy: number, cw: number, ch: number, alpha: number): void {
  const mark = behaviourMark(u.behaviour)
  if (!mark) return
  const k = cw / 24
  const m = Math.max(7, Math.round(10 * k)), inset = Math.max(1, Math.round(2 * k))
  const bx = cx + cw - m - inset, by = cy + ch - m - inset
  g.fillStyle = `rgba(0,0,0,${0.86 * alpha})`
  g.fillRect(bx - 1, by - 1, m + 2, m + 2)
  g.fillStyle = `rgba(255,255,255,${alpha})`
  g.textAlign = 'center'
  g.textBaseline = 'middle'
  g.font = `bold ${Math.max(6, Math.round(m * 0.9))}px ${FONT}`
  g.fillText(mark, bx + m / 2, by + m / 2 + 1)
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
    g.fillText(glyphOfType(scene.types.get(u.type ?? ''), '?'), bx + bw / 2, by + bh / 2 + 1)
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
export function drawFlag(g: CanvasRenderingContext2D, cx: number, cy: number, cw: number, S: number): void {
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

/** A small count in the top right corner of a square that holds several units for different difficulties (docs/DIFFICULTY.md). */
export function drawStackMark(g: CanvasRenderingContext2D, cx: number, cy: number, cw: number, count: number): void {
  const k = cw / 24
  const m = Math.max(8, Math.round(11 * k)), inset = Math.max(1, Math.round(1 * k))
  const bx = cx + cw - m - inset, by = cy + inset
  g.fillStyle = 'rgba(20,20,20,0.92)'
  g.fillRect(bx - 1, by - 1, m + 2, m + 2)
  g.fillStyle = '#ffd43b'
  g.textAlign = 'center'
  g.textBaseline = 'middle'
  g.font = `bold ${Math.max(7, Math.round(m * 0.85))}px ${FONT}`
  g.fillText(String(count), bx + m / 2, by + m / 2 + 1)
}
