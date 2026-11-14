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
