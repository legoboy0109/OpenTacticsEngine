import { gridOfGame, layoutOf } from '../../model/gridSpec'
import { boxSize, corners } from '../../model/hex'
import { ringsOf } from '../../model/structures'
import type { Structure } from '../../model/types'
import { useDoc } from '../../store/context'
import { T } from './text'

/** The squares a structure covers, drawn as a little grid in its colour (up to 8 x 8). */
export function Footprint({ w, h, color }: { w: number; h: number; color: string | undefined }) {
  const cell = Math.max(10, Math.min(28, Math.floor(200 / Math.max(w, h))))
  return (
    <div className="footprint" role="img" aria-label={T.footprintLabel(w, h)} data-testid="footprint"
      style={{ gridTemplateColumns: `repeat(${w}, ${cell}px)`, gridAutoRows: `${cell}px` }}>
      {Array.from({ length: w * h }, (_, i) => <span key={i} style={{ background: color || '#888' }} />)}
    </div>
  )
}

/** Hexagon games: the hexagon, or the hexagon with its ring of six, as little hexagons in the structure's colour. */
export function HexFootprint({ s }: { s: Structure }) {
  const layout = layoutOf(gridOfGame(useDoc().game))
  const rings = ringsOf(s)
  const tile = 24
  const box = boxSize(layout, tile)
  const cs = corners(layout, tile)
  // Positions of the cells in pixels: the middle, then (for rings) the six around it. Pointy: neighbours sit half a box to the side
  // on the rows above and below; flat: half a box up or down on the columns beside.
  const step = layout === 'pointy' ? { x: box.w, y: (3 * box.h) / 4 } : { x: (3 * box.w) / 4, y: box.h }
  const around: Array<[number, number]> = layout === 'pointy'
    ? [[0.5, -1], [1, 0], [0.5, 1], [-0.5, 1], [-1, 0], [-0.5, -1]]
    : [[0, -1], [1, -0.5], [1, 0.5], [0, 1], [-1, 0.5], [-1, -0.5]]
  const cells: Array<[number, number]> = rings === 1 ? [[0, 0], ...around] : [[0, 0]]
  const pos = cells.map(([a, b]) => [a * step.x, b * step.y] as [number, number])
  const pad = 2
  const minX = Math.min(...pos.map((p) => p[0])) - pad, minY = Math.min(...pos.map((p) => p[1])) - pad
  const w = Math.max(...pos.map((p) => p[0] + box.w)) + pad - minX, h = Math.max(...pos.map((p) => p[1] + box.h)) + pad - minY
  const k = rings === 1 ? 0.9 : 1.8
  return (
    <svg className="footprint hex" role="img" aria-label={T.footprintHexLabel(rings)} data-testid="footprint" width={w * k} height={h * k} viewBox={`${minX} ${minY} ${w} ${h}`}>
      {pos.map((p, i) => (
        <polygon key={i} points={cs.map(([cx, cy]) => `${p[0] + cx},${p[1] + cy}`).join(' ')} fill={s.color || '#888'} stroke="rgba(0,0,0,0.35)" strokeWidth={1} />
      ))}
    </svg>
  )
}
