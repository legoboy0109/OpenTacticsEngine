// Positions for the timeline: chapters in columns (layers) and rows (lanes) from timelineLayer.ts, boxes of one chapter side by side,
// arrows from timelineRoute.ts. A pure function of the graph and which levels are expanded; sizes are in "world" pixels (the
// view scales them). Chapters nothing leads to sit in their own band below the story, with a gap before it.
import { placeChapters } from './timelineLayer'
import { placeEdge, type RouteEnv } from './timelineRoute'
import type { PlacedEdge, Rect, TimelineGraph, TimelineLayout, TimelineNode } from './timelineTypes'

export const BOX_W = 196
export const BOX_H = 96
/** An expanded level: room under the header for each event row and each group heading. */
export const EVENT_ROW_H = 56
export const GROUP_HEAD_H = 24
export const EVENTS_PAD = 12
const BOX_GAP = 40
const LAYER_GAP = 150
const CHIP_H = 30
const LANE_GAP = 72
const MARGIN = 80
const BAND_GAP = 64

/** Height of a box; an expanded level grows with its events (and the headings of their groups). */
export function boxHeight(n: TimelineNode, expanded: boolean): number {
  if (!expanded || n.kind !== 'level') return BOX_H
  if (n.events.length === 0) return BOX_H + 40
  let h = BOX_H + EVENTS_PAD
  let last = ''
  for (const e of n.events) { if (e.group !== last) { h += GROUP_HEAD_H; last = e.group } h += EVENT_ROW_H }
  return h + EVENTS_PAD
}

export function layoutTimeline(g: TimelineGraph, expanded: ReadonlySet<string> = new Set()): TimelineLayout {
  const empty: TimelineLayout = { nodes: {}, edges: {}, strips: [], width: MARGIN * 2, height: MARGIN * 2, unreachedBand: null }
  if (g.chapters.length === 0) return empty
  const p = placeChapters(g)
  const nodeBy = new Map(g.nodes.map((n) => [n.id, n]))
  const nLayers = Math.max(...p.layer) + 1
  const nLanes = Math.max(...p.lane) + 1

  // Column x: as wide as its widest chapter.
  const layerBoxes = new Array<number>(nLayers).fill(1)
  for (const c of g.chapters) layerBoxes[p.layer[c.index]] = Math.max(layerBoxes[p.layer[c.index]], c.nodes.length)
  const layerX: number[] = []
  let x = MARGIN + LAYER_GAP / 2
  for (let l = 0; l < nLayers; l++) { layerX.push(x); x += layerBoxes[l] * BOX_W + (layerBoxes[l] - 1) * BOX_GAP + LAYER_GAP }

  // Row heights: the tallest box of the lane.
  const laneH = new Array<number>(nLanes).fill(BOX_H)
  for (const c of g.chapters) for (const id of c.nodes) laneH[p.lane[c.index]] = Math.max(laneH[p.lane[c.index]], boxHeight(nodeBy.get(id)!, expanded.has(c.id)))
  const laneTop: number[] = []
  const laneBottom: number[] = []
  let y = MARGIN
  let bandTop = 0
  for (let k = 0; k < nLanes; k++) {
    if (k === p.reachedLanes && g.unreached.length > 0) { y += BAND_GAP; bandTop = y - BAND_GAP / 2 - 6 }
    laneTop.push(y + CHIP_H)
    laneBottom.push(y + CHIP_H + laneH[k])
    y += CHIP_H + laneH[k] + LANE_GAP
  }

  const rect: Record<string, Rect> = {}
  const strips: TimelineLayout['strips'] = []
  for (const c of g.chapters) {
    const x0 = layerX[p.layer[c.index]], y0 = laneTop[p.lane[c.index]]
    c.nodes.forEach((id, i) => { rect[id] = { x: x0 + i * (BOX_W + BOX_GAP), y: y0, w: BOX_W, h: boxHeight(nodeBy.get(id)!, expanded.has(c.id)) } })
    strips.push({ chapterId: c.id, rect: { x: x0, y: y0 - CHIP_H, w: c.nodes.length * BOX_W + (c.nodes.length - 1) * BOX_GAP, h: boxHeight(nodeBy.get(c.nodes[c.nodes.length - 1])!, expanded.has(c.id)) + CHIP_H } })
  }

  const tails = new Set(g.chapters.map((c) => c.tail))
  const env: RouteEnv = {
    rect, layerX, layerGap: LAYER_GAP, laneBottom,
    layerOf: (ch) => p.layer[ch], laneOf: (ch) => p.lane[ch], isTail: (id) => tails.has(id), forward: p.forward, slots: new Map(),
  }
  const edges: Record<string, PlacedEdge> = {}
  for (const e of g.edges) edges[e.id] = placeEdge(e, env)
  separateLabels(Object.values(edges))

  let width = 0, height = 0
  for (const r of Object.values(rect)) { width = Math.max(width, r.x + r.w); height = Math.max(height, r.y + r.h) }
  for (const e of Object.values(edges)) { width = Math.max(width, e.bounds.x + e.bounds.w); height = Math.max(height, e.bounds.y + e.bounds.h) }
  width += MARGIN; height += MARGIN
  const unreachedBand = g.unreached.length > 0 ? { x: 0, y: bandTop, w: width, h: height - bandTop } : null
  return { nodes: rect, edges, strips, width, height, unreachedBand }
}

/** Labels that would overlap are moved down in steps so each can be read. */
function separateLabels(list: PlacedEdge[]): void {
  const done: Array<{ x: number; y: number }> = []
  for (const e of list) {
    if (!e.label) continue
    const l = e.label
    let tries = 0
    while (done.some((d) => Math.abs(d.x - l.x) < 140 && Math.abs(d.y - l.y) < 24) && tries++ < 12) l.y += 26
    done.push({ x: l.x, y: l.y })
  }
}
