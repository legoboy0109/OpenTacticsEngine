// The 16 joining pieces explained: a diagram in the picker and a template PNG to download (made in the browser).
// Piece n = N1 + E2 + S4 + W8 (docs/GRAPHICS.md), at column n % 4, row n / 4.
import { useEffect, useRef } from 'react'
import { JOIN_E, JOIN_N, JOIN_S, JOIN_W } from '../../model/pickCell'
import { P } from './text'

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
export async function downloadJoinGuide(pw: number, ph: number): Promise<void> {
  const c = document.createElement('canvas')
  c.width = pw * 4; c.height = ph * 4
  drawJoinGuide(c.getContext('2d')!, pw, ph, true)
  const blob = await new Promise<Blob | null>((r) => c.toBlob(r, 'image/png'))
  if (!blob) return
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url; a.download = `join-guide-${pw}x${ph}.png`
  document.body.appendChild(a); a.click(); a.remove()
  setTimeout(() => URL.revokeObjectURL(url), 2000)
}

export function JoinGuide({ pieceW, pieceH }: { pieceW: number; pieceH: number }) {
  const ref = useRef<HTMLCanvasElement>(null)
  useEffect(() => {
    const c = ref.current!
    const dpr = window.devicePixelRatio || 1
    const pw = 40, ph = Math.max(20, Math.round(40 * pieceH / Math.max(1, pieceW)))
    c.width = pw * 4 * dpr; c.height = ph * 4 * dpr
    const g = c.getContext('2d')!
    g.scale(dpr, dpr)
    drawJoinGuide(g, pw, ph, true)
    c.style.width = `${pw * 4}px`; c.style.height = `${ph * 4}px`
  }, [pieceW, pieceH])
  return (
    <div className="join-guide">
      <h4>{P.guideTitle}</h4>
      <p className="help">{P.guideHelp}</p>
      <canvas ref={ref} role="img" aria-label={P.guideTitle} className="join-guide-canvas" />
      <div>
        <button type="button" className="btn sm" onClick={() => void downloadJoinGuide(pieceW, pieceH)}>{P.guideDownload}</button>
        <p className="help">{P.guideNote}</p>
      </div>
    </div>
  )
}
