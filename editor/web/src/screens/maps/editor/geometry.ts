// View maths for the map canvas: `view` maps a point of the map picture, in tiles (a square's unit, see model/mapGeo.ts), to
// the CSS pixel (x + ux * scale, y + uy * scale). On square maps a tile is one square.
import type { Cell } from '../../../model/mapGrid'
import type { Geo } from '../../../model/mapGeo'

export interface View { scale: number; x: number; y: number }
export const MIN_SCALE = 2
export const MAX_SCALE = 128
export const clampScale = (s: number): number => Math.min(MAX_SCALE, Math.max(MIN_SCALE, s))

/** Whole map centred with a little air around it. */
export function fitView(w: number, h: number, mapW: number, mapH: number, pad = 12): View {
  const scale = clampScale(Math.min((w - 2 * pad) / mapW, (h - 2 * pad) / mapH))
  return { scale, x: (w - mapW * scale) / 2, y: (h - mapH * scale) / 2 }
}

export function cellAt(v: View, px: number, py: number, geo: Geo): Cell | null {
  return geo.at((px - v.x) / v.scale, (py - v.y) / v.scale)
}

/** Zoom by `factor` keeping the map point under (px, py) where it is. */
export function zoomAt(v: View, factor: number, px: number, py: number): View {
  const scale = clampScale(v.scale * factor)
  const k = scale / v.scale
  return { scale, x: px - (px - v.x) * k, y: py - (py - v.y) * k }
}

/** Keeps the map from being dragged completely out of sight. */
export function clampPan(v: View, w: number, h: number, mapW: number, mapH: number, keep = 48): View {
  const mw = mapW * v.scale, mh = mapH * v.scale
  return {
    scale: v.scale,
    x: Math.min(w - keep, Math.max(keep - mw, v.x)),
    y: Math.min(h - keep, Math.max(keep - mh, v.y)),
  }
}

/** Pans the view just enough to show cell `c`. */
export function reveal(v: View, c: Cell, w: number, h: number, geo: Geo): View {
  const m = v.scale
  const b = geo.box(c.x, c.y)
  let { x, y } = v
  if (x + b.x * m < 0) x = -b.x * m + m
  else if (x + (b.x + b.w) * m > w) x = w - (b.x + b.w) * m - m
  if (y + b.y * m < 0) y = -b.y * m + m
  else if (y + (b.y + b.h) * m > h) y = h - (b.y + b.h) * m - m
  return { scale: m, x, y }
}

/** "x,y", or "x,y,v" for the second and later unit of a stack of variants. */
export const cellKey = (c: Cell | null): string | null => (c ? `${c.x},${c.y}${c.v ? `,${c.v}` : ''}` : null)
