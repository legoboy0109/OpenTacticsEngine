// View maths for the map canvas: `view` maps a square (cx, cy) to the CSS pixel
// (x + cx * scale, y + cy * scale).
import type { Cell } from '../../../model/mapGrid'

export interface View { scale: number; x: number; y: number }
export const MIN_SCALE = 2
export const MAX_SCALE = 128
export const clampScale = (s: number): number => Math.min(MAX_SCALE, Math.max(MIN_SCALE, s))

/** Whole map centred with a little air around it. */
export function fitView(w: number, h: number, mapW: number, mapH: number, pad = 12): View {
  const scale = clampScale(Math.min((w - 2 * pad) / mapW, (h - 2 * pad) / mapH))
  return { scale, x: (w - mapW * scale) / 2, y: (h - mapH * scale) / 2 }
}

export function cellAt(v: View, px: number, py: number, mapW: number, mapH: number): Cell | null {
  const x = Math.floor((px - v.x) / v.scale)
  const y = Math.floor((py - v.y) / v.scale)
  return x >= 0 && y >= 0 && x < mapW && y < mapH ? { x, y } : null
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

/** Pans the view just enough to show square `c`. */
export function reveal(v: View, c: Cell, w: number, h: number): View {
  const m = v.scale
  let { x, y } = v
  if (x + c.x * m < 0) x = -c.x * m + m
  else if (x + (c.x + 1) * m > w) x = w - (c.x + 1) * m - m
  if (y + c.y * m < 0) y = -c.y * m + m
  else if (y + (c.y + 1) * m > h) y = h - (c.y + 1) * m - m
  return { scale: m, x, y }
}

export const cellKey = (c: Cell | null): string | null => (c ? `${c.x},${c.y}` : null)
