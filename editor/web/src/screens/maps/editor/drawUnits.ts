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
    drawBadges(g, scene, u, cx, cy, cw, ch, alpha)
    if (u.leader) drawLeaderPip(g, cx, cy, cw, alpha)
    drawBehaviourMark(g, u, cx, cy, cw, ch, alpha)
    return
  }
  drawUnit(g, scene, u, cx, cy, cw, ch, side, alpha, glyph)
  drawBadges(g, scene, u, cx, cy, cw, ch, alpha)
  drawBehaviourMark(g, u, cx, cy, cw, ch, alpha)
}

/** Unit badges (docs/GRAPHICS.md "Unit badges"), as the player draws them: the picture at its own size times (the square on screen / the game's tile size), the same
 *  corner of it on the unit's square, then moved by the offset (scaled the same). */
function drawBadges(g: CanvasRenderingContext2D, scene: Scene, u: MapUnit, cx: number, cy: number, cw: number, ch: number, alpha: number): void {
  const art = scene.art
  if (!art || art.badges.length === 0) return
  const k = cw / Math.max(1, art.tileSize)
  const sideDef = scene.map.sides?.find((s) => s.id === u.side)
  const computer = sideDef?.controller === 'ai'
  for (const rb of art.badges) {
    const b = rb.badge
    if (!(rb.types.has(u.type ?? '') || (u.character !== undefined && rb.characters.has(u.character)))) continue
    const rule = b.sides ?? 'all'
    if (rule === 'computer' && !computer) continue
    if (rule === 'player' && computer) continue
    if (Array.isArray(rule) && !rule.includes(u.side)) continue
    const dw = Math.max(1, Math.round(rb.sw * k)), dh = Math.max(1, Math.round(rb.sh * k))
    let x = cx, y = cy
    switch (b.anchor ?? 'bottomRight') {
      case 'topLeft': break
      case 'topRight': x = cx + cw - dw; break
      case 'bottomLeft': y = cy + ch - dh; break
      case 'bottomRight': x = cx + cw - dw; y = cy + ch - dh; break
      case 'center': x = cx + Math.trunc((cw - dw) / 2); y = cy + Math.trunc((ch - dh) / 2); break
    }
    x += Math.round((b.offset?.[0] ?? 0) * k)
    y += Math.round((b.offset?.[1] ?? 0) * k)
    g.globalAlpha = alpha
    g.imageSmoothingEnabled = false
    g.drawImage(rb.img, rb.sx, rb.sy, rb.sw, rb.sh, x, y, dw, dh)
    g.globalAlpha = 1
  }
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
