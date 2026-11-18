// The palette of the Tiles tool: the tileset's image as a grid of numbered tiles, zoomable and scrollable inside its own box (so a big sheet never
// widens the page, and 16 px tiles can be shown 2 to 8 times as big). Click a tile to choose it; drag (mouse or pen) to choose a block of tiles
// for stamping. One canvas, no element per tile (a sheet can have thousands). Animated tiles carry a small mark.
import { useEffect, useMemo, useRef, useState } from 'react'
import { useArtState } from '../../../../art/useArt'
import { findSheet } from '../../../../model/graphics'
import { animationOf, findTileset, sheetTiles } from '../../../../model/tilesets'
import type { ProjectDoc } from '../../../../model/types'
import { useStore } from '../../../../store/context'
import { Icon } from '../../../../ui/Icon'
import { TL } from '../tilesText'

export interface PaletteSel { col: number; row: number; w: number; h: number }

interface Props {
  doc: ProjectDoc
  tileset: string
  sel: PaletteSel | null
  zoom: number
  onZoom: (z: number) => void
  /** A tile (w = h = 1) or a block was chosen; `tiles` are its tile numbers row by row. */
  onChoose: (sel: PaletteSel, tiles: number[]) => void
}

export const PALETTE_ZOOMS = [1, 2, 3, 4, 6, 8]
const MAX_CANVAS = 4096
const SLOP = 6

/** The zoom a palette starts with: tiles about 32 px wide on screen (a finger), within the canvas limit. */
export function startPaletteZoom(cellW: number, cols: number, rows: number, cellH: number): number {
  const fit = PALETTE_ZOOMS.filter((z) => cols * cellW * z <= MAX_CANVAS && rows * cellH * z <= MAX_CANVAS)
  const want = Math.max(1, Math.round(32 / Math.max(1, cellW)))
  return fit.filter((z) => z <= want).pop() ?? fit[0] ?? 1
}

export function TilePalette({ doc, tileset, sel, zoom, onZoom, onChoose }: Props) {
  const store = useStore()
  useArtState()  // a picture finishing loading redraws the palette
  const cv = useRef<HTMLCanvasElement>(null)
  const [drag, setDrag] = useState<{ a: { col: number; row: number }; b: { col: number; row: number } } | null>(null)
  const [hover, setHover] = useState<number | null>(null)
  const ts = findTileset(doc, tileset)
  const sheet = ts ? findSheet(doc, ts.sheet) : undefined
  const img = sheet ? store.art.image(sheet.file) : null
  const ready = !!sheet && img?.status === 'ready' && !!img.img
  const { cols, rows, count } = sheetTiles(sheet, img?.width ?? 0, img?.height ?? 0)
  const z = Math.min(zoom, PALETTE_ZOOMS.filter((q) => cols * (sheet?.cellWidth ?? 1) * q <= MAX_CANVAS && rows * (sheet?.cellHeight ?? 1) * q <= MAX_CANVAS).pop() ?? 1)
  const cw = (sheet?.cellWidth ?? 16) * z, ch = (sheet?.cellHeight ?? 16) * z
  const animated = useMemo(() => new Set(Object.keys(ts?.animations ?? {}).filter((k) => animationOf(ts, Number(k))).map(Number)), [ts])

  useEffect(() => {
    const c = cv.current
    const g = c?.getContext('2d')
    if (!c || !g || !ready || !sheet || !img?.img) return
    c.width = cols * cw
    c.height = rows * ch
    // A checkerboard shows through see-through tiles.
    const sq = Math.max(4, Math.round(cw / 4))
    for (let y = 0; y < c.height; y += sq) for (let x = 0; x < c.width; x += sq) { g.fillStyle = ((x + y) / sq) % 2 === 0 ? '#2a2d36' : '#20232b'; g.fillRect(x, y, sq, sq) }
    g.imageSmoothingEnabled = false
    g.drawImage(img.img, 0, 0, cols * sheet.cellWidth, rows * sheet.cellHeight, 0, 0, c.width, c.height)
    if (cw >= 8) {
      g.fillStyle = 'rgba(0,0,0,0.35)'
      for (let x = 0; x <= cols; x++) g.fillRect(Math.min(c.width - 1, x * cw), 0, 1, c.height)
      for (let y = 0; y <= rows; y++) g.fillRect(0, Math.min(c.height - 1, y * ch), c.width, 1)
    }
    // Numbers when the tiles are big enough to hold them.
    if (cw >= 28) {
      g.font = `bold ${Math.max(9, Math.round(cw / 4))}px system-ui, sans-serif`
      g.textBaseline = 'top'
      g.lineJoin = 'round'
      g.lineWidth = 3
      for (let n = 0; n < count; n++) {
        const x = (n % cols) * cw + 2, y = Math.floor(n / cols) * ch + 1
        g.strokeStyle = 'rgba(0,0,0,0.8)'; g.strokeText(String(n), x, y)
        g.fillStyle = '#fff'; g.fillText(String(n), x, y)
      }
    }
    // Animated tiles: a small yellow triangle in the bottom-right corner.
    g.fillStyle = '#ffe45c'
    for (const n of animated) {
      if (n >= count) continue
      const x = ((n % cols) + 1) * cw, y = (Math.floor(n / cols) + 1) * ch, s = Math.max(5, Math.round(cw / 4))
      g.beginPath(); g.moveTo(x, y); g.lineTo(x - s, y); g.lineTo(x, y - s); g.closePath(); g.fill()
    }
    const box = drag ? { col: Math.min(drag.a.col, drag.b.col), row: Math.min(drag.a.row, drag.b.row), w: Math.abs(drag.a.col - drag.b.col) + 1, h: Math.abs(drag.a.row - drag.b.row) + 1 } : sel
    if (box) {
      g.lineWidth = 2
      g.strokeStyle = '#000'; g.strokeRect(box.col * cw + 1, box.row * ch + 1, box.w * cw - 2, box.h * ch - 2)
      g.strokeStyle = '#ffe45c'; g.strokeRect(box.col * cw + 2, box.row * ch + 2, box.w * cw - 4, box.h * ch - 4)
    }
  }, [ready, sheet, img, cols, rows, cw, ch, count, animated, sel, drag])

  const at = (e: React.PointerEvent): { col: number; row: number } | null => {
    const r = cv.current!.getBoundingClientRect()
    const col = Math.floor(((e.clientX - r.left) / r.width) * cols), row = Math.floor(((e.clientY - r.top) / r.height) * rows)
    return col >= 0 && row >= 0 && col < cols && row < rows ? { col, row } : null
  }
  const start = useRef<{ x: number; y: number } | null>(null)
  const finish = (a: { col: number; row: number }, b: { col: number; row: number }) => {
    const box = { col: Math.min(a.col, b.col), row: Math.min(a.row, b.row), w: Math.abs(a.col - b.col) + 1, h: Math.abs(a.row - b.row) + 1 }
    const tiles: number[] = []
    for (let j = 0; j < box.h; j++) for (let i = 0; i < box.w; i++) tiles.push((box.row + j) * cols + box.col + i)
    onChoose(box, tiles)
  }

  if (!sheet) return <p className="help">{TL.paletteNoImage}</p>
  if (img?.status === 'loading') return <p className="help" role="status">{TL.paletteLoading}</p>
  if (!ready || count === 0) return <p className="help">{TL.paletteNoImage}</p>
  const zi = PALETTE_ZOOMS.indexOf(z)
  return (
    <div className="tile-palette" data-testid="tile-palette">
      <div className="row tile-palette-bar">
        <button type="button" className="btn ghost sm icon" aria-label={TL.zoomOut} disabled={zi <= 0} onClick={() => onZoom(PALETTE_ZOOMS[Math.max(0, zi - 1)])}><Icon name="minus" /></button>
        <span className="help tile-palette-zoom" data-testid="tile-palette-zoom">{z}×</span>
        <button type="button" className="btn ghost sm icon" aria-label={TL.zoomIn} disabled={zi < 0 || zi >= PALETTE_ZOOMS.length - 1 || cols * (sheet.cellWidth) * PALETTE_ZOOMS[zi + 1] > MAX_CANVAS || rows * sheet.cellHeight * PALETTE_ZOOMS[zi + 1] > MAX_CANVAS} onClick={() => onZoom(PALETTE_ZOOMS[zi + 1])}><Icon name="plus" /></button>
        <span className="help" aria-live="polite">{hover !== null ? TL.chosen(hover, hover % cols, Math.floor(hover / cols)) : ''}</span>
      </div>
      <div className="tile-palette-scroll">
        <canvas ref={cv} className="tile-palette-canvas" role="application" tabIndex={0} aria-label={TL.paletteLabel(tileset)} data-testid="tile-palette-canvas"
          data-cols={cols} data-rows={rows} data-cell={cw} data-cellh={ch} data-zoom={z} data-sel={sel ? `${sel.col},${sel.row},${sel.w},${sel.h}` : ''}
          style={{ width: cols * cw, height: rows * ch }}
          onPointerDown={(e) => {
            const p = at(e)
            if (!p) return
            start.current = { x: e.clientX, y: e.clientY }
            if (e.pointerType !== 'touch') { try { cv.current!.setPointerCapture(e.pointerId) } catch { /* gone */ } setDrag({ a: p, b: p }) }
          }}
          onPointerMove={(e) => {
            const p = at(e)
            setHover(p ? p.row * cols + p.col : null)
            if (drag && p && (e.buttons & 1)) setDrag({ a: drag.a, b: p })
          }}
          onPointerLeave={() => setHover(null)}
          onPointerUp={(e) => {
            const p = at(e)
            const s = start.current
            start.current = null
            const d = drag
            setDrag(null)
            if (!p || !s) return
            if (e.pointerType === 'touch' && Math.hypot(e.clientX - s.x, e.clientY - s.y) > SLOP) return  // a scroll, not a tap
            finish(d ? d.a : p, p)
          }}
          onPointerCancel={() => { start.current = null; setDrag(null) }}
          onKeyDown={(e) => {
            const cur = sel ?? { col: 0, row: 0, w: 1, h: 1 }
            const d: Record<string, [number, number]> = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }
            if (!d[e.key]) return
            e.preventDefault()
            const col = Math.min(cols - 1, Math.max(0, cur.col + d[e.key][0])), row = Math.min(rows - 1, Math.max(0, cur.row + d[e.key][1]))
            finish({ col, row }, { col, row })
          }} />
      </div>
    </div>
  )
}
