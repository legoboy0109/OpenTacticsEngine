// Positions for the timeline: chapters in columns (layers) and rows (lanes) from timelineLayer.ts, boxes of one chapter side by side,
// arrows from timelineRoute.ts. A pure function of the graph and which levels are expanded; sizes are in "world" pixels (the
// view scales them).
//  - A long story is folded: when the columns would run wider than MAX_ROW_W the next columns start a new line below, still
//    reading left to right (the arrow that crosses the fold runs under the first line and comes in from the left).
//  - Chapters nothing leads to sit in their own band below the story, with a gap before it.
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
const BAND_GAP = 110
/** A line of columns is folded once it would be wider than this. */
export const MAX_ROW_W = 3400

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

  // Column widths, then which line each column is on (a column that does not fit starts the next line) and its x on that line.
  const layerBoxes = new Array<number>(nLayers).fill(1)
  for (const c of g.chapters) layerBoxes[p.layer[c.index]] = Math.max(layerBoxes[p.layer[c.index]], c.nodes.length)
  const layerW = layerBoxes.map((b) => b * BOX_W + (b - 1) * BOX_GAP)
  const layerX: number[] = []
  const layerRow: number[] = []
  const startX = MARGIN + LAYER_GAP / 2
  let x = startX, row = 0, lineW = 0
  for (let l = 0; l < nLayers; l++) {
    if (x > startX && x + layerW[l] > MAX_ROW_W) { row++; x = startX }
    layerX.push(x); layerRow.push(row)
    x += layerW[l] + LAYER_GAP
    lineW = Math.max(lineW, x)
  }

  // Groups = a line of a band (the story, or the chapters nothing leads to). Each has its own lanes (rows of boxes) stacked down.
  const groupOf = (ch: number) => (g.chapters[ch].unreached ? 1 : 0) * 1000 + layerRow[p.layer[ch]]
  const laneRel = (ch: number) => p.lane[ch] - (g.chapters[ch].unreached ? p.reachedLanes : 0)
  const groups = [...new Set(g.chapters.map((c) => groupOf(c.index)))].sort((a, b) => a - b)
  const laneH = new Map<string, number>()
  for (const c of g.chapters) {
    const key = `${groupOf(c.index)}/${laneRel(c.index)}`
    for (const id of c.nodes) laneH.set(key, Math.max(laneH.get(key) ?? BOX_H, boxHeight(nodeBy.get(id)!, expanded.has(c.id))))
  }
  const laneTop = new Map<string, number>()
  const groupBottom = new Map<number, number>()
  let y = MARGIN
  let bandTop = 0
  let bandOpen = false
  for (const gr of groups) {
    if (gr >= 1000 && !bandOpen) { bandOpen = true; bandTop = y + 4; y += BAND_GAP }
    const lanes = [...new Set(g.chapters.filter((c) => groupOf(c.index) === gr).map((c) => laneRel(c.index)))].sort((a, b) => a - b)
    for (const ln of lanes) {
      const key = `${gr}/${ln}`
      laneTop.set(key, y + CHIP_H)
      y += CHIP_H + laneH.get(key)! + LANE_GAP
    }
    groupBottom.set(gr, y - LANE_GAP)
  }

  const rect: Record<string, Rect> = {}
  const strips: TimelineLayout['strips'] = []
  const laneBottom = (ch: number) => laneTop.get(`${groupOf(ch)}/${laneRel(ch)}`)! + laneH.get(`${groupOf(ch)}/${laneRel(ch)}`)!
  for (const c of g.chapters) {
    const x0 = layerX[p.layer[c.index]], y0 = laneTop.get(`${groupOf(c.index)}/${laneRel(c.index)}`)!
    c.nodes.forEach((id, i) => { rect[id] = { x: x0 + i * (BOX_W + BOX_GAP), y: y0, w: BOX_W, h: boxHeight(nodeBy.get(id)!, expanded.has(c.id)) } })
    strips.push({ chapterId: c.id, rect: { x: x0, y: y0 - CHIP_H, w: c.nodes.length * BOX_W + (c.nodes.length - 1) * BOX_GAP, h: Math.max(...c.nodes.map((id) => rect[id].h)) + CHIP_H } })
  }

  const tails = new Set(g.chapters.map((c) => c.tail))
  const env: RouteEnv = {
    rect, layerX, layerRight: layerX.map((lx, l) => lx + layerW[l]), layerGap: LAYER_GAP, laneBottom, groupBottom: (ch) => groupBottom.get(groupOf(ch))!,
    layerOf: (ch) => p.layer[ch], sameGroup: (a, b) => groupOf(a) === groupOf(b), isTail: (id) => tails.has(id), forward: p.forward, slots: new Map(),
    laneKey: (ch) => `${groupOf(ch)}/${laneRel(ch)}`,
  }
  const edges: Record<string, PlacedEdge> = {}
  for (const e of g.edges) edges[e.id] = placeEdge(e, env)
  separateLabels(Object.values(edges))

  let width = lineW, height = 0
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
