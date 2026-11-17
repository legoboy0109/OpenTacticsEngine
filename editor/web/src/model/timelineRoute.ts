// The drawn path of one arrow: horizontal and vertical stretches with rounded corners.
//  - Between boxes of one chapter, or into the next column: leave the right side, one vertical step in the gap between the
//    columns, enter the left side.
//  - Over several columns, or back to an earlier chapter (a loop), or out of a box that has another box on its right: leave
//    through the bottom, run along the free band between two rows, climb in the gap in front of the target, enter its left side.
// Several arrows sharing a gap or a band are spread over a few "slots" so they do not lie on top of each other.
import type { PlacedEdge, Rect, TimelineEdge } from './timelineTypes'

export interface Pt { x: number; y: number }

export interface RouteEnv {
  rect: Record<string, Rect>
  /** x of the left edge of each column (layer). */
  layerX: number[]
  /** Gap between columns: the vertical stretches go in the middle of it. */
  layerGap: number
  /** y just below the boxes of each lane: the free band starts here. */
  laneBottom: number[]
  /** Chapter's layer, lane, and whether the box is the last of its chapter's strip. */
  layerOf: (chapter: number) => number
  laneOf: (chapter: number) => number
  isTail: (nodeId: string) => boolean
  forward: Set<string>
  slots: Map<string, number>
}

const SLOT = 6

function slot(env: RouteEnv, key: string, n: number): number {
  const i = env.slots.get(key) ?? 0
  env.slots.set(key, i + 1)
  return (i % n) - Math.floor(n / 2)
}

const attachY = (r: Rect) => r.y + 48

export function routePoints(e: TimelineEdge, env: RouteEnv): Pt[] {
  const S = env.rect[e.from], T = env.rect[e.to]
  const aS = attachY(S), aT = attachY(T)
  const sameStrip = e.fromChapter === e.toChapter
  // Inside a strip, to the box on the right: a straight line.
  if (sameStrip && !e.backward && Math.abs(aS - aT) < 1) return [{ x: S.x + S.w, y: aS }, { x: T.x, y: aT }]
  const lt = env.layerOf(e.toChapter), ls = env.layerOf(e.fromChapter)
  const gapAt = (layer: number) => env.layerX[layer] - env.layerGap / 2 + slot(env, `g${layer}`, 7) * SLOT
  const gy = env.laneBottom[env.laneOf(e.fromChapter)] + 14 + slot(env, `b${env.laneOf(e.fromChapter)}`, 7) * SLOT + 3 * SLOT
  const forward = env.forward.has(e.id) && lt > ls
  const rightExit = forward && env.isTail(e.from)
  if (rightExit) {
    const gx1 = gapAt(ls + 1)
    if (lt === ls + 1) return Math.abs(aS - aT) < 1 ? [{ x: S.x + S.w, y: aS }, { x: T.x, y: aT }] : [{ x: S.x + S.w, y: aS }, { x: gx1, y: aS }, { x: gx1, y: aT }, { x: T.x, y: aT }]
    const gx2 = gapAt(lt)
    return [{ x: S.x + S.w, y: aS }, { x: gx1, y: aS }, { x: gx1, y: gy }, { x: gx2, y: gy }, { x: gx2, y: aT }, { x: T.x, y: aT }]
  }
  const gx2 = gapAt(lt)
  const cx = S.x + S.w / 2 + slot(env, `x${e.from}`, 5) * 8
  return [{ x: cx, y: S.y + S.h }, { x: cx, y: gy }, { x: gx2, y: gy }, { x: gx2, y: aT }, { x: T.x, y: aT }]
}

/** "M … Q …" with every corner rounded. */
export function roundedPath(pts: Pt[], radius = 10): string {
  if (pts.length < 2) return ''
  let d = `M${pts[0].x} ${pts[0].y}`
  for (let i = 1; i < pts.length - 1; i++) {
    const a = pts[i - 1], b = pts[i], c = pts[i + 1]
    const l1 = Math.hypot(b.x - a.x, b.y - a.y), l2 = Math.hypot(c.x - b.x, c.y - b.y)
    const r = Math.min(radius, l1 / 2, l2 / 2)
    if (r < 0.5) { d += `L${b.x} ${b.y}`; continue }
    const p1 = { x: b.x - ((b.x - a.x) / l1) * r, y: b.y - ((b.y - a.y) / l1) * r }
    const p2 = { x: b.x + ((c.x - b.x) / l2) * r, y: b.y + ((c.y - b.y) / l2) * r }
    d += `L${round(p1.x)} ${round(p1.y)}Q${b.x} ${b.y} ${round(p2.x)} ${round(p2.y)}`
  }
  const z = pts[pts.length - 1]
  return `${d}L${z.x} ${z.y}`
}
const round = (n: number) => Math.round(n * 10) / 10

export function placeEdge(e: TimelineEdge, env: RouteEnv): PlacedEdge {
  const pts = routePoints(e, env)
  // The label sits on the middle of the longest horizontal stretch (at least 100 long), else of the longest stretch.
  let best = -1, bestLen = -1
  pts.forEach((p, i) => {
    if (i === 0) return
    const q = pts[i - 1]
    const len = Math.hypot(p.x - q.x, p.y - q.y) * (p.y === q.y && Math.abs(p.x - q.x) >= 100 ? 1000 : 1)
    if (len > bestLen) { bestLen = len; best = i }
  })
  const a = pts[Math.max(1, best) - 1], b = pts[Math.max(1, best)]
  const label = e.label ? { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 } : null
  // Halfway along the path: where the "add a chapter here" button goes.
  let mid: Pt | null = null
  if (e.between) {
    const total = pts.slice(1).reduce((s, p, i) => s + Math.hypot(p.x - pts[i].x, p.y - pts[i].y), 0)
    let at = total / 2
    for (let i = 1; i < pts.length && !mid; i++) {
      const len = Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y)
      if (at <= len) mid = { x: pts[i - 1].x + ((pts[i].x - pts[i - 1].x) * at) / (len || 1), y: pts[i - 1].y + ((pts[i].y - pts[i - 1].y) * at) / (len || 1) }
      else at -= len
    }
  }
  const xs = pts.map((p) => p.x), ys = pts.map((p) => p.y)
  const x0 = Math.min(...xs) - 4, y0 = Math.min(...ys) - 4
  return { id: e.id, path: roundedPath(pts), label, mid, bounds: { x: x0, y: y0, w: Math.max(...xs) + 4 - x0, h: Math.max(...ys) + 4 - y0 } }
}
