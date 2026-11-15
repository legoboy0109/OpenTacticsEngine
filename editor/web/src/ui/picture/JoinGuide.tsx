// The joining pieces explained: a diagram in the picker and a template PNG to download (made in the browser).
// Squares: 16 pieces, piece n = N1 + E2 + S4 + W8 (docs/GRAPHICS.md), at column n % 4, row n / 4.
// Hexagons: 64 pieces, piece n = the six neighbour bits (docs/HEX.md "Join pieces on hexagons"), at column n % 8, row n / 8.
import { useEffect, useRef } from 'react'
import type { HexLayout } from '../../model/hex'
import { JOIN_E, JOIN_N, JOIN_S, JOIN_W } from '../../model/pickCell'
import { P } from './text'

/** The corners of a hexagon's box as fractions of it; edge i (corner i to i + 1) faces neighbour i (model/hex.ts). */
const UNIT_CORNERS: Record<HexLayout, Array<[number, number]>> = {
  pointy: [[0.5, 0], [1, 0.25], [1, 0.75], [0.5, 1], [0, 0.75], [0, 0.25]],
  flat: [[0.25, 0], [0.75, 0], [1, 0.5], [0.75, 1], [0.25, 1], [0, 0.5]],
}

/** Draws the 8 x 8 guide for hexagons: each piece shows its hexagon and a bar to every neighbour it connects to. */
export function drawHexJoinGuide(g: CanvasRenderingContext2D, pw: number, ph: number, numbers: boolean, layout: HexLayout): void {
  g.imageSmoothingEnabled = false
  const cs = UNIT_CORNERS[layout]
  for (let n = 0; n < 64; n++) {
    const x = (n % 8) * pw, y = Math.floor(n / 8) * ph
    g.fillStyle = (n % 8 + Math.floor(n / 8)) % 2 ? '#dfe5ee' : '#eef1f6'
    g.fillRect(x, y, pw, ph)
    const pt = (c: [number, number]): [number, number] => [x + c[0] * pw, y + c[1] * ph]
    g.beginPath()
    cs.forEach((c, i) => { const [a, b] = pt(c); if (i === 0) g.moveTo(a, b); else g.lineTo(a, b) })
    g.closePath()
    g.strokeStyle = 'rgba(0,0,0,0.3)'; g.lineWidth = 1
    g.stroke()
    const [mx, my] = pt([0.5, 0.5])
    g.strokeStyle = '#7d8aa0'
    g.lineWidth = Math.max(2, Math.round(Math.min(pw, ph) / 6))
    g.lineCap = 'butt'
    for (let i = 0; i < 6; i++) {
      if (!(n & (1 << i))) continue
      const a = pt(cs[i]), b = pt(cs[(i + 1) % 6])
      g.beginPath(); g.moveTo(mx, my); g.lineTo((a[0] + b[0]) / 2, (a[1] + b[1]) / 2); g.stroke()
    }
    g.fillStyle = '#7d8aa0'
    g.fillRect(Math.round(mx - g.lineWidth / 2), Math.round(my - g.lineWidth / 2), g.lineWidth, g.lineWidth)
    g.strokeStyle = 'rgba(0,0,0,0.25)'; g.lineWidth = 1
    g.strokeRect(x + 0.5, y + 0.5, pw - 1, ph - 1)
    if (numbers && pw >= 20 && ph >= 20) {
      g.fillStyle = '#1c2430'; g.font = `bold ${Math.max(8, Math.round(Math.min(pw, ph) / 3.2))}px system-ui, sans-serif`
      g.textAlign = 'left'; g.textBaseline = 'top'
      g.fillText(String(n), x + 2, y + 2)
    }
  }
}

/** Draws the 4 x 4 guide: each piece shows bars towards the neighbours it connects to, and (when there is room) its number. */
export function drawJoinGuide(g: CanvasRenderingContext2D, pw: number, ph: number, numbers: boolean): void {
  g.imageSmoothingEnabled = false
  for (let n = 0; n < 16; n++) {
    const x = (n % 4) * pw, y = Math.floor(n / 4) * ph
    g.fillStyle = (n % 4 + Math.floor(n / 4)) % 2 ? '#dfe5ee' : '#eef1f6'
    g.fillRect(x, y, pw, ph)
    const bw = Math.max(2, Math.round(pw / 4)), bh = Math.max(2, Math.round(ph / 4))
    const cx = Math.round((pw - bw) / 2), cy = Math.round((ph - bh) / 2)
    g.fillStyle = '#7d8aa0'
    g.fillRect(x + cx, y + cy, bw, bh) // the middle, always there
    if (n & JOIN_N) g.fillRect(x + cx, y, bw, cy + bh)
    if (n & JOIN_S) g.fillRect(x + cx, y + cy, bw, ph - cy)
    if (n & JOIN_W) g.fillRect(x, y + cy, cx + bw, bh)
    if (n & JOIN_E) g.fillRect(x + cx, y + cy, pw - cx, bh)
    g.strokeStyle = 'rgba(0,0,0,0.25)'; g.lineWidth = 1
    g.strokeRect(x + 0.5, y + 0.5, pw - 1, ph - 1)
    if (numbers && pw >= 20 && ph >= 20) {
      g.fillStyle = '#1c2430'; g.font = `bold ${Math.max(8, Math.round(Math.min(pw, ph) / 3.2))}px system-ui, sans-serif`
      g.textAlign = 'left'; g.textBaseline = 'top'
      g.fillText(String(n), x + 2, y + 2)
    }
  }
}

/** Saves the template at the image's piece size. */
export async function downloadJoinGuide(pw: number, ph: number, hex?: HexLayout): Promise<void> {
  const c = document.createElement('canvas')
  const side = hex ? 8 : 4
  c.width = pw * side; c.height = ph * side
  if (hex) drawHexJoinGuide(c.getContext('2d')!, pw, ph, true, hex); else drawJoinGuide(c.getContext('2d')!, pw, ph, true)
  const blob = await new Promise<Blob | null>((r) => c.toBlob(r, 'image/png'))
  if (!blob) return
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url; a.download = `join-guide-${pw}x${ph}.png`
  document.body.appendChild(a); a.click(); a.remove()
  setTimeout(() => URL.revokeObjectURL(url), 2000)
}

/** `hex` = the layout of a hexagon game (64 pieces); missing = the 16 pieces of squares. */
export function JoinGuide({ pieceW, pieceH, hex }: { pieceW: number; pieceH: number; hex?: HexLayout }) {
  const ref = useRef<HTMLCanvasElement>(null)
  useEffect(() => {
    const c = ref.current!
    const dpr = window.devicePixelRatio || 1
    const side = hex ? 8 : 4
    const pw = hex ? 32 : 40, ph = Math.max(hex ? 24 : 20, Math.round(pw * pieceH / Math.max(1, pieceW)))
    c.width = pw * side * dpr; c.height = ph * side * dpr
    const g = c.getContext('2d')!
    g.scale(dpr, dpr)
    if (hex) drawHexJoinGuide(g, pw, ph, true, hex); else drawJoinGuide(g, pw, ph, true)
    c.style.width = `${pw * side}px`; c.style.height = `${ph * side}px`
  }, [pieceW, pieceH, hex])
  return (
    <div className="join-guide">
      <h4>{P.guideTitle}</h4>
      <p className="help">{hex ? P.guideHelpHex(hex === 'flat') : P.guideHelp}</p>
      <canvas ref={ref} role="img" aria-label={P.guideTitle} className="join-guide-canvas" />
      <div>
        <button type="button" className="btn sm" onClick={() => void downloadJoinGuide(pieceW, pieceH, hex)}>{P.guideDownload}</button>
        <p className="help">{hex ? P.guideNoteHex : P.guideNote}</p>
      </div>
    </div>
  )
}
