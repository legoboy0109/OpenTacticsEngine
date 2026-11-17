// Arrow keys Up / Down on the timeline: the nearest box in the row above or below (the nearest by how far its middle is from this
// box's middle, rows counted by their top edge). Left / Right just follow the play order and need no geometry.
import type { TimelineGraph, TimelineLayout } from './timelineTypes'

export function neighbourBox(g: TimelineGraph, l: TimelineLayout, from: string, dir: 1 | -1): string | undefined {
  const a = l.nodes[from]
  if (!a) return undefined
  const cx = a.x + a.w / 2
  let best: string | undefined
  let bestRow = Infinity, bestDx = Infinity
  for (const n of g.nodes) {
    const r = l.nodes[n.id]
    const dy = (r.y - a.y) * dir
    if (dy <= 8) continue // not in a row above / below
    const dx = Math.abs(r.x + r.w / 2 - cx)
    if (dy < bestRow - 8 || (Math.abs(dy - bestRow) <= 8 && dx < bestDx)) { best = n.id; bestRow = dy; bestDx = dx }
  }
  return best
}
