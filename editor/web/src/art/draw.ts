// Drawing art the way the player does (docs/GRAPHICS.md, "Drawing rules"). Used by the map canvas, the palettes
// and every preview, so they cannot drift apart. All rectangles are whole device pixels.
import { joinMaskAt, pickCell } from '../model/pickCell'
import type { CellRef } from '../model/types'
import type { ArtScene, Resolved } from './scene'

/** Draws one piece of an image into a rectangle, nearest-neighbour. False when the piece lies outside the image. */
export function drawPiece(g: CanvasRenderingContext2D, r: Resolved, cell: CellRef, dx: number, dy: number, dw: number, dh: number): boolean {
  const sx = cell[0] * r.cw, sy = cell[1] * r.ch
  if (cell[0] < 0 || cell[1] < 0 || sx + r.cw > r.iw || sy + r.ch > r.ih || dw < 1 || dh < 1) return false
  g.imageSmoothingEnabled = false
  g.drawImage(r.img, sx, sy, r.cw, r.ch, dx, dy, dw, dh)
  return true
}

/** The piece a terrain square shows: variants by position (same hash as the player), animation by time, join by neighbours. */
export function terrainCell(art: ArtScene, id: string, r: Resolved, x: number, y: number, mapW: number, mapH: number, idAt: (x: number, y: number) => string, timeMs: number): CellRef {
  let mask = 0
  if (r.shape === 'join') {
    const same = art.joinSame.get(id)
    mask = joinMaskAt(x, y, mapW, mapH, r.look.joinEdge !== false, (nx, ny) => same?.has(idAt(nx, ny)) ?? false)
  }
  return pickCell(r.look, x, y, mask, timeMs)
}

/** Terrain with art: the piece stretched over the square (the caller has already filled the placeholder colour underneath,
 *  so transparent pixels show it). False = no art here (not loaded, or the piece is outside the image): draw the placeholder. */
export function drawTerrainArt(g: CanvasRenderingContext2D, art: ArtScene, id: string, x: number, y: number, mapW: number, mapH: number,
  idAt: (x: number, y: number) => string, timeMs: number, dx: number, dy: number, dw: number, dh: number): boolean {
  const r = art.terrain.get(id)
  return !!r && drawPiece(g, r, terrainCell(art, id, r, x, y, mapW, mapH, idAt, timeMs), dx, dy, dw, dh)
}

/** The flat base in the side's colour, as the player draws it (engine/player/battle/sprites.cpp): a bar across the bottom of the
 *  square, inset one pixel on the sides and the bottom, a sixth of the square of fill inside a one-pixel dark outline.
 *  "One pixel" is a pixel of the player's 24 px tile, scaled to the square being drawn. */
export function drawMarker(g: CanvasRenderingContext2D, cx: number, cy: number, cw: number, ch: number, color: string, alpha: number): void {
  const px = Math.max(1, Math.round(cw / 24))
  const fill = Math.max(2 * px, Math.round(ch / 6))
  const bw = cw - 2 * px, bh = fill + 2 * px
  const x = cx + px, y = cy + ch - px - bh
  g.fillStyle = `rgba(20,20,28,${(200 / 255) * alpha})`
  g.fillRect(x, y, bw, bh)
  g.fillStyle = color
  g.globalAlpha = alpha
  g.fillRect(x + px, y + px, bw - 2 * px, bh - 2 * px)
  g.globalAlpha = 1
}

/** A unit with art: marker first, then the sprite, full square wide, as tall as its proportions say, standing on the bottom
 *  edge of the square (tall sprites stick out above). False = could not draw (draw the placeholder block). */
export function drawUnitArt(g: CanvasRenderingContext2D, r: Resolved, marker: { on: boolean; color: string }, timeMs: number,
  cx: number, cy: number, cw: number, ch: number, alpha: number): boolean {
  const cell = pickCell(r.look, 0, 0, 0, timeMs)
  const dh = Math.max(1, Math.round((cw * r.ch) / r.cw))
  if (cell[0] < 0 || cell[1] < 0 || (cell[0] + 1) * r.cw > r.iw || (cell[1] + 1) * r.ch > r.ih) return false
  if (marker.on) drawMarker(g, cx, cy, cw, ch, marker.color, alpha)
  g.globalAlpha = alpha
  const ok = drawPiece(g, r, cell, cx, cy + ch - dh, cw, dh)
  g.globalAlpha = 1
  return ok
}
