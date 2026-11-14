// A small picture of one piece of an image (a dialogue portrait), drawn crisp on a canvas.
import { useEffect, useRef } from 'react'
import { useArtState } from '../../art/useArt'
import { findSheet } from '../../model/graphics'
import type { CellRef } from '../../model/types'
import { useDoc, useStore } from '../../store/context'

interface Props { sheet: string; cell: CellRef; label: string; height?: number }

export function PortraitThumb({ sheet, cell, label, height = 56 }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState() // draw again once the image has loaded
  const ref = useRef<HTMLCanvasElement>(null)
  const def = findSheet(doc, sheet)
  const img = def ? store.art.image(def.file) : null
  const ready = !!def && img?.status === 'ready'
  const w = def ? Math.max(1, Math.round((def.cellWidth / def.cellHeight) * height)) : height
  useEffect(() => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    g.clearRect(0, 0, c.width, c.height)
    if (!def || !img || img.status !== 'ready' || !img.img) return
    g.imageSmoothingEnabled = false
    g.drawImage(img.img, cell[0] * def.cellWidth, cell[1] * def.cellHeight, def.cellWidth, def.cellHeight, 0, 0, w, height)
  })
  return <canvas ref={ref} className="portrait-thumb" width={w} height={height} role="img" aria-label={label} data-ready={ready || undefined} />
}
