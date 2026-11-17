// The live preview of walking pictures: the unit going down, right, up and left in turn (a direction the walk lacks is skipped),
// beside its standing picture, drawn at the same size per pixel and on the same foot point as the game draws them. Crisp, on a canvas.
import { useEffect, useRef } from 'react'
import { clockNow, useAnimationClock, useArtState, usePageVisible, useReducedMotion } from '../../../art/useArt'
import { findSheet } from '../../../model/graphics'
import { DEFAULT_WALK_MS, shownDirs } from '../../../model/walking'
import type { Look } from '../../../model/types'
import { useDoc, useStore } from '../../../store/context'
import { W } from './text'

const WIDTH = 300, HEIGHT = 150, GROUND = 130, DIR_MS = 1600

export function WalkPreview({ look }: { look: Look }) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const ref = useRef<HTMLCanvasElement>(null)
  const reduced = useReducedMotion()
  const visible = usePageVisible()
  const walk = look.walk

  const draw = () => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g || !walk) return
    g.imageSmoothingEnabled = false
    g.clearRect(0, 0, WIDTH, HEIGHT)
    g.fillStyle = '#26324f'
    g.fillRect(0, 0, WIDTH, GROUND)
    g.fillStyle = '#3f5a36'
    g.fillRect(0, GROUND, WIDTH, HEIGHT - GROUND)
    const stand = findSheet(doc, look.sheet), standImg = stand ? store.art.image(stand.file) : null
    const ws = findSheet(doc, walk.sheet ?? look.sheet), walkImg = ws ? store.art.image(ws.file) : null
    if (!stand || !ws) return
    // One scale for both, so the walking pictures keep their size next to the standing one (the game's rule).
    const scale = Math.max(1, Math.min(6, Math.floor(110 / Math.max(stand.cellHeight, ws.cellHeight))))
    g.fillStyle = '#e8ecff'
    g.font = '11px sans-serif'
    g.textAlign = 'center'
    // the standing picture (the first piece of its look)
    const first = look.cell ?? look.frames?.[0] ?? look.variants?.[0]
    if (first && standImg?.status === 'ready' && standImg.img) {
      g.drawImage(standImg.img, first[0] * stand.cellWidth, first[1] * stand.cellHeight, stand.cellWidth, stand.cellHeight, 60 - (stand.cellWidth * scale) / 2, GROUND - stand.cellHeight * scale, stand.cellWidth * scale, stand.cellHeight * scale)
    }
    g.fillText(W.previewStanding, 60, HEIGHT - 4)
    const dirs = shownDirs(walk)
    if (!dirs.length || !walkImg || walkImg.status !== 'ready' || !walkImg.img) return
    const t = clockNow(reduced)
    const which = dirs[Math.floor(t / DIR_MS) % dirs.length]
    const frames = which.cells
    const ms = walk.frameMs ?? DEFAULT_WALK_MS
    const cell = frames[Math.floor(t / ms) % frames.length]
    const cx = 200
    g.save()
    g.translate(cx, GROUND)
    if (which.mirror) g.scale(-1, 1)
    g.drawImage(walkImg.img, cell[0] * ws.cellWidth, cell[1] * ws.cellHeight, ws.cellWidth, ws.cellHeight, -(ws.cellWidth * scale) / 2, -ws.cellHeight * scale, ws.cellWidth * scale, ws.cellHeight * scale)
    g.restore()
    g.fillText(W.previewShows(W.dirNames[which.dir]), cx, HEIGHT - 4)
  }
  useEffect(draw)
  useAnimationClock(!reduced && visible, draw)

  return <canvas ref={ref} className="walk-preview" width={WIDTH} height={HEIGHT} role="img" aria-label={W.previewLabel} data-testid="walk-preview" />
}
