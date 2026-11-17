// A small picture of a story background (the whole image, or one piece of it), drawn crisp on a canvas.
import { useEffect, useRef } from 'react'
import { useArtState } from '../../art/useArt'
import { findSheet } from '../../model/graphics'
import type { SceneBackground } from '../../model/types'
import { useDoc, useStore } from '../../store/context'

interface Props { background: SceneBackground; label: string; height?: number }

export function BackgroundThumb({ background, label, height = 56 }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState() // draw again once the image has loaded
  const ref = useRef<HTMLCanvasElement>(null)
  const def = findSheet(doc, background.sheet)
  const img = def ? store.art.image(def.file) : null
  const ready = !!def && img?.status === 'ready'
  // The source: one piece, or the whole picture (its real size is known once it has loaded).
  const sw = background.cell ? def?.cellWidth ?? 1 : img?.status === 'ready' ? img.width : 16
  const sh = background.cell ? def?.cellHeight ?? 1 : img?.status === 'ready' ? img.height : 9
  // At most 64 wide so a row of the list fits a phone (a wide picture is drawn smaller, whole).
  const w = Math.max(1, Math.min(64, Math.round((sw / sh) * height)))
  const h = Math.max(1, Math.round((w * sh) / sw))
  useEffect(() => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    g.clearRect(0, 0, c.width, c.height)
    if (!def || !img || img.status !== 'ready' || !img.img) return
    g.imageSmoothingEnabled = false
    if (background.cell) g.drawImage(img.img, background.cell[0] * def.cellWidth, background.cell[1] * def.cellHeight, def.cellWidth, def.cellHeight, 0, 0, w, h)
    else g.drawImage(img.img, 0, 0, img.width, img.height, 0, 0, w, h)
  })
  return <canvas ref={ref} className="portrait-thumb" width={w} height={h} role="img" aria-label={label} data-ready={ready || undefined} />
}
