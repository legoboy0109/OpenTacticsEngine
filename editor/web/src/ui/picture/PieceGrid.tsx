// An image on a checkerboard with its grid of pieces drawn over it, zoomable and scrollable. Optionally clickable:
// the picker uses it to choose pieces. Keyboard: arrow keys move a cursor, Enter or Space picks. All drawing is on
// one canvas (an image can have thousands of pieces, so no element per piece).
import { useCallback, useEffect, useRef, useState } from 'react'
import { gridInfo } from '../../model/gridGuess'
import { Icon } from '../Icon'
import { P } from './text'

export interface PieceMark { col: number; row: number; label?: string }
export interface PieceBlock { col: number; row: number; size: number }

interface Props {
  image: CanvasImageSource | null
  width: number
  height: number
  cellWidth: number
  cellHeight: number
  /** Chosen pieces (outlined, optionally numbered). */
  marks?: PieceMark[]
  /** The chosen blocks of pieces (joining pictures: 4 x 4), outlined. */
  blocks?: PieceBlock[]
  /** Hovering/the cursor shows a block of this size from the piece under it (joining pictures); the block must fit. */
  hoverBlock?: number
  onPick?: (col: number, row: number) => void
  label: string
}

export const ZOOMS = [0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16]
const MAX_CANVAS = 4096

/** Largest zoom that keeps the canvas a sane size. */
const maxZoomFor = (w: number, h: number) => ZOOMS.filter((z) => w * z <= MAX_CANVAS && h * z <= MAX_CANVAS).pop() ?? ZOOMS[0]
/** Start zoom: the whole image across the available width when that is at least 1x (whole steps, at most 8x), else 1x. */
export function startZoom(imageW: number, imageH: number, availableW: number, touchPiece?: number): number {
  const cap = maxZoomFor(imageW, imageH)
  let want = Math.min(Math.max(1, Math.floor(availableW / Math.max(1, imageW))), 8, cap)
  // Pieces to tap should be a finger wide (about 32 px): zoom in and let the box scroll rather than make them tiny.
  if (touchPiece && touchPiece * want < 32) want = Math.min(Math.max(want, Math.ceil(32 / touchPiece)), 4, cap)
  return ZOOMS.filter((z) => z <= want).pop() ?? 1
}

export function PieceGrid({ image, width, height, cellWidth, cellHeight, marks = [], blocks = [], hoverBlock, onPick, label }: Props) {
  const wrap = useRef<HTMLDivElement>(null)
  const cv = useRef<HTMLCanvasElement>(null)
  const [zoom, setZoom] = useState<number | null>(null)
  const [hover, setHover] = useState<{ col: number; row: number } | null>(null)
  const [cursor, setCursor] = useState<{ col: number; row: number } | null>(null)
  const z = zoom ?? startZoom(width, height, 320)
  const info = gridInfo(width, height, cellWidth, cellHeight)
  const interactive = !!onPick

  // The first zoom needs the real width of the box, which is only known once it is on screen (a dialog opens a moment
  // after it is built): measure with the box and fit the image across it, until the person picks a zoom themselves.
  const userZoom = useRef(false)
  useEffect(() => {
    const el = wrap.current
    if (!el || width <= 0) return
    const fit = () => { if (!userZoom.current && el.clientWidth > 0) setZoom(startZoom(width, height, el.clientWidth - 2, interactive ? Math.min(cellWidth, cellHeight) : undefined)) }
    fit()
    const ro = new ResizeObserver(fit)
    ro.observe(el)
    return () => ro.disconnect()
  }, [width, height, cellWidth, cellHeight, interactive])

  const draw = useCallback(() => {
    const c = cv.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    const W = Math.round(width * z), H = Math.round(height * z)
    if (c.width !== W || c.height !== H) { c.width = W; c.height = H }
    g.clearRect(0, 0, W, H)
    g.imageSmoothingEnabled = false
    if (image) g.drawImage(image, 0, 0, width, height, 0, 0, W, H)
    const pw = cellWidth * z, ph = cellHeight * z
    const gridW = info.cols * pw, gridH = info.rows * ph
    // Pixels that do not make a whole piece are ignored by the game: shade them.
    g.fillStyle = 'rgba(20,20,30,0.55)'
    if (info.extraRight > 0) g.fillRect(gridW, 0, W - gridW, H)
    if (info.extraBottom > 0) g.fillRect(0, gridH, gridW, H - gridH)
    // Grid lines: a dark and a light line side by side so they show on any art. Skipped when pieces are tiny on screen.
    if (pw >= 6 && ph >= 6 && (info.cols > 1 || info.rows > 1)) {
      for (let i = 0; i <= info.cols; i++) { const x = Math.round(i * pw); g.fillStyle = 'rgba(0,0,0,0.55)'; g.fillRect(x - 1, 0, 1, gridH); g.fillStyle = 'rgba(255,255,255,0.5)'; g.fillRect(x, 0, 1, gridH) }
      for (let j = 0; j <= info.rows; j++) { const y = Math.round(j * ph); g.fillStyle = 'rgba(0,0,0,0.55)'; g.fillRect(0, y - 1, gridW, 1); g.fillStyle = 'rgba(255,255,255,0.5)'; g.fillRect(0, y, gridW, 1) }
    }
    const rect = (col: number, row: number, w: number, h: number, color: string, thick: number) => {
      g.strokeStyle = color; g.lineWidth = thick
      g.strokeRect(Math.round(col * pw) + thick / 2, Math.round(row * ph) + thick / 2, Math.round(w * pw) - thick, Math.round(h * ph) - thick)
    }
    const fits = (c: { col: number; row: number }, n: number) => c.col >= 0 && c.row >= 0 && c.col + n <= info.cols && c.row + n <= info.rows
    for (const m of marks) {
      if (m.col >= info.cols || m.row >= info.rows) continue
      g.fillStyle = 'rgba(43,98,214,0.28)'; g.fillRect(Math.round(m.col * pw), Math.round(m.row * ph), Math.round(pw), Math.round(ph))
      rect(m.col, m.row, 1, 1, '#ffffff', 3); rect(m.col, m.row, 1, 1, '#2b62d6', 2)
      if (m.label) {
        const r = Math.min(Math.max(9, Math.min(pw, ph) * 0.3), 14)
        const bx = Math.round(m.col * pw) + r + 2, by = Math.round(m.row * ph) + r + 2
        g.fillStyle = '#2b62d6'; g.beginPath(); g.arc(bx, by, r, 0, Math.PI * 2); g.fill()
        g.strokeStyle = '#fff'; g.lineWidth = 1.5; g.stroke()
        g.fillStyle = '#fff'; g.font = `bold ${Math.round(r * 1.15)}px system-ui, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle'
        g.fillText(m.label, bx, by + 1)
      }
    }
    for (const block of blocks) if (fits(block, block.size)) { rect(block.col, block.row, block.size, block.size, '#ffffff', 4); rect(block.col, block.row, block.size, block.size, '#2b62d6', 2) }
    const at = hover ?? cursor
    if (at && at.col < info.cols && at.row < info.rows && interactive) {
      if (hoverBlock) {
        const ok = fits(at, hoverBlock)
        g.fillStyle = ok ? 'rgba(255,255,255,0.22)' : 'rgba(220,40,40,0.25)'
        g.fillRect(Math.round(at.col * pw), Math.round(at.row * ph), Math.round(hoverBlock * pw), Math.round(hoverBlock * ph))
        rect(at.col, at.row, hoverBlock, hoverBlock, ok ? '#ffe45c' : '#e5322d', 2)
      } else {
        g.fillStyle = 'rgba(255,255,255,0.28)'; g.fillRect(Math.round(at.col * pw), Math.round(at.row * ph), Math.round(pw), Math.round(ph))
        rect(at.col, at.row, 1, 1, '#ffe45c', 2)
      }
    }
  }, [image, width, height, cellWidth, cellHeight, z, marks, blocks, hover, cursor, hoverBlock, info.cols, info.rows, info.extraRight, info.extraBottom, interactive])
  useEffect(draw, [draw])

  const cellOf = (e: { clientX: number; clientY: number }) => {
    const r = cv.current!.getBoundingClientRect()
    const col = Math.floor(((e.clientX - r.left) / r.width) * width / cellWidth)
    const row = Math.floor(((e.clientY - r.top) / r.height) * height / cellHeight)
    return col >= 0 && row >= 0 && col < info.cols && row < info.rows ? { col, row } : null
  }
  const step = (d: number) => { userZoom.current = true; setZoom(ZOOMS[Math.min(Math.max(ZOOMS.indexOf(z) + d, 0), ZOOMS.indexOf(maxZoomFor(width, height)))] ?? z) }
  const onKey = (e: React.KeyboardEvent) => {
    if (!interactive) return
    const c = cursor ?? { col: 0, row: 0 }
    const move = (dc: number, dr: number) => { e.preventDefault(); setCursor({ col: Math.min(info.cols - 1, Math.max(0, c.col + dc)), row: Math.min(info.rows - 1, Math.max(0, c.row + dr)) }); setHover(null) }
    if (e.key === 'ArrowLeft') move(-1, 0)
    else if (e.key === 'ArrowRight') move(1, 0)
    else if (e.key === 'ArrowUp') move(0, -1)
    else if (e.key === 'ArrowDown') move(0, 1)
    else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); if (cursor) onPick!(cursor.col, cursor.row); else setCursor(c) }
  }

  return (
    <div className="piece-grid">
      <div className="pg-bar" role="group" aria-label={P.zoom}>
        <button type="button" className="btn ghost sm icon" aria-label={P.zoomOut} disabled={z <= ZOOMS[0]} onClick={() => step(-1)}><Icon name="minus" /></button>
        <span className="pg-zoom" aria-live="polite">{Math.round(z * 100)}%</span>
        <button type="button" className="btn ghost sm icon" aria-label={P.zoomIn} disabled={z >= maxZoomFor(width, height)} onClick={() => step(1)}><Icon name="plus" /></button>
        <button type="button" className="btn ghost sm" onClick={() => { userZoom.current = false; setZoom(startZoom(width, height, (wrap.current?.clientWidth ?? 320) - 2, interactive ? Math.min(cellWidth, cellHeight) : undefined)) }}>{P.zoomFit}</button>
      </div>
      <div className="pg-scroll checker" ref={wrap}>
        <canvas ref={cv} className="pg-canvas" data-testid="piece-grid" data-zoom={z} data-cell={`${cellWidth},${cellHeight}`}
          style={{ width: Math.round(width * z), height: Math.round(height * z), cursor: interactive ? 'pointer' : 'default' }}
          tabIndex={interactive ? 0 : -1} role={interactive ? 'application' : 'img'} aria-label={label}
          onPointerMove={interactive ? (e) => { setHover(cellOf(e)); setCursor(null) } : undefined}
          onPointerLeave={interactive ? () => setHover(null) : undefined}
          onClick={interactive ? (e) => { const c = cellOf(e); if (c) { setCursor(null); onPick!(c.col, c.row) } } : undefined}
          onKeyDown={onKey} onBlur={() => setCursor(null)} />
      </div>
    </div>
  )
}
