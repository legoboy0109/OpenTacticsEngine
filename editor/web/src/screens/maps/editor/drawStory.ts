// Draws the squares and areas that the open event (or the objectives) is about, and the rectangle being picked.
// Device pixels, rounded like the rest of drawMap.ts. Each tone has a colour and a short label.
import type { Overlay, OverlayTone } from '../../../model/storyOverlays'
import type { Rect } from '../../../model/storyTypes'
import { FONT } from './drawColors'

const TONES: Record<OverlayTone, string> = {
  area: '45,212,255',
  add: '124,211,90',
  move: '255,176,32',
  view: '197,140,255',
  win: '74,222,128',
  lose: '255,92,92',
  slot: '90,170,255',
  change: '255,120,200',
}

interface Grid { px: (c: number) => number; py: (c: number) => number; S: number; dpr: number; mw: number; mh: number }

function box(r: Rect, k: Grid): { x: number; y: number; w: number; h: number } | null {
  // Only the part inside the map is drawn (a wrong area is flagged in the checks).
  const x0 = Math.max(0, r.x), y0 = Math.max(0, r.y), x1 = Math.min(k.mw, r.x + r.w), y1 = Math.min(k.mh, r.y + r.h)
  if (x1 <= x0 || y1 <= y0) return null
  return { x: k.px(x0), y: k.py(y0), w: k.px(x1) - k.px(x0), h: k.py(y1) - k.py(y0) }
}

export function drawOverlays(g: CanvasRenderingContext2D, overlays: Overlay[], pickRect: Rect | null, k: Grid): void {
  const line = Math.max(2, Math.round(k.dpr * 2))
  g.textAlign = 'left'
  g.textBaseline = 'top'
  for (const o of overlays) {
    const b = box(o, k)
    if (!b) continue
    const rgb = TONES[o.tone]
    g.fillStyle = `rgba(${rgb},0.26)`
    g.fillRect(b.x, b.y, b.w, b.h)
    g.strokeStyle = 'rgba(0,0,0,0.75)'
    g.lineWidth = line + 2
    g.strokeRect(b.x + line / 2, b.y + line / 2, b.w - line, b.h - line)
    g.strokeStyle = `rgb(${rgb})`
    g.lineWidth = line
    g.strokeRect(b.x + line / 2, b.y + line / 2, b.w - line, b.h - line)
    // A deployment slot's number is short: it shows on small squares too.
    if (k.S >= 18 * k.dpr && (b.w >= 36 * k.dpr || o.tone === 'slot')) {
      const size = Math.max(9 * k.dpr, Math.min(14 * k.dpr, k.S * 0.36))
      g.font = `bold ${Math.round(size)}px ${FONT}`
      g.lineJoin = 'round'
      g.lineWidth = Math.max(2, Math.round(size / 4))
      g.strokeStyle = 'rgba(0,0,0,0.85)'
      g.strokeText(o.label, b.x + line + 2, b.y + line + 1)
      g.fillStyle = '#ffffff'
      g.fillText(o.label, b.x + line + 2, b.y + line + 1)
    }
  }
  if (pickRect) {
    const b = box(pickRect, k)
    if (b) {
      g.fillStyle = 'rgba(255,228,92,0.28)'
      g.fillRect(b.x, b.y, b.w, b.h)
      g.setLineDash([line * 3, line * 2])
      g.strokeStyle = 'rgba(0,0,0,0.8)'
      g.lineWidth = line + 2
      g.strokeRect(b.x + line / 2, b.y + line / 2, b.w - line, b.h - line)
      g.strokeStyle = '#ffe45c'
      g.lineWidth = line
      g.strokeRect(b.x + line / 2, b.y + line / 2, b.w - line, b.h - line)
      g.setLineDash([])
    }
  }
  g.textAlign = 'center'
  g.textBaseline = 'middle'
}
