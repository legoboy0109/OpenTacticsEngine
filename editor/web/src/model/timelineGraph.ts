// The campaign as a graph of boxes and arrows, for the Campaign screen's timeline (docs/EDITOR_UI.md "Campaign timeline").
// Each chapter is up to three boxes in play order: its before scene (if it has one), its level (the battle on its map), its after
// scene (if it has one). Arrows:
//   - "order": before -> level -> after inside a chapter, and from a chapter's last box to the next chapter's first box;
//   - "next": the same, when the chapter's "Next chapter" names a chapter that is not the following one in the list;
//   - "goto": a `goto` of a scene (bare, inside an `if`, or in an answer of a question) to the first box of the chapter it names,
//     labelled with what leads there.
// The order arrow out of a scene is dropped when every way through the scene runs a goto (timelineFlow.ts). A box the chapter's
// before scene always jumps over is marked `skipped`; a chapter nothing leads to (from chapter 1) is `unreached`.
import { sceneOf } from './campaign'
import { sceneMap } from './campaignRoster'
import { isSceneObject, sceneCommands } from './campaignScene'
import { chapterName, type Chapter, type SceneKey } from './campaignTypes'
import { orderedEvents } from './timelineEvents'
import { alwaysGoes, sceneGotos } from './timelineFlow'
import { addMarkers, countMarkers, noMarkers, shopCount } from './timelineMarkers'
import type { EdgeKind, TimelineChapter, TimelineEdge, TimelineGraph, TimelineNode } from './timelineTypes'
import type { ProjectDoc } from './types'

const hasScene = (ch: Chapter, which: SceneKey): boolean => Array.isArray(ch[which]) || isSceneObject(ch[which])
export const nodeId = (chapterId: string, part: 'before' | 'level' | 'after'): string => `${chapterId}:${part}`

export function buildTimeline(doc: ProjectDoc): TimelineGraph | null {
  const camp = doc.campaign
  if (!camp || !Array.isArray(camp.chapters)) return null
  const chapters = camp.chapters
  const ctx = { doc, map: sceneMap(doc) }
  const indexOf = new Map(chapters.map((ch, i) => [ch.id, i]))
  const nodes: TimelineNode[] = []
  const chs: TimelineChapter[] = []
  const edges: TimelineEdge[] = []

  chapters.forEach((ch, i) => {
    const name = chapterName(ch)
    const ids: string[] = []
    const scene = (which: SceneKey) => {
      const list = sceneCommands(ch[which])
      nodes.push({
        id: nodeId(ch.id, which), kind: 'scene', chapterId: ch.id, chapterIndex: i, which, chapterName: name,
        markers: countMarkers(list), commandCount: countAll(list), unreached: false, skipped: false, events: [],
      })
      ids.push(nodeId(ch.id, which))
    }
    if (hasScene(ch, 'before')) scene('before')
    const map = ch.map ? doc.maps[ch.map] ?? null : null
    const known = !!doc.game?.maps.some((m) => m.id === ch.map)
    const events = known ? orderedEvents(doc, ch.id, ch.map) : []
    const markers = events.reduce((m, e) => addMarkers(m, e.markers), noMarkers())
    markers.shop = shopCount(doc, map)
    nodes.push({
      id: nodeId(ch.id, 'level'), kind: 'level', chapterId: ch.id, chapterIndex: i, chapterName: name, mapId: ch.map,
      mapName: doc.game?.maps.find((m) => m.id === ch.map)?.name || ch.map, mapKnown: known, hasPrep: !!map?.prep, markers, commandCount: 0,
      unreached: false, skipped: false, events,
    })
    ids.push(nodeId(ch.id, 'level'))
    if (hasScene(ch, 'after')) scene('after')
    chs.push({
      id: ch.id, index: i, name, nodes: ids, head: ids[0], tail: ids[ids.length - 1], unreached: false, ends: false,
      hasBefore: hasScene(ch, 'before'), hasAfter: hasScene(ch, 'after'),
    })
  })

  const seen = new Set<string>()
  const add = (from: string, to: string, kind: EdgeKind, label: string, fromChapter: number, toChapter: number, between?: TimelineEdge['between']) => {
    const key = `${from}>${to}>${kind}>${label}`
    if (seen.has(key)) return
    seen.add(key)
    const order = (id: string) => nodes.findIndex((n) => n.id === id)
    const backward = toChapter < fromChapter || (toChapter === fromChapter && order(to) <= order(from))
    edges.push({ id: `e${edges.length}`, from, to, kind, label, fromChapter, toChapter, backward, between })
  }

  chapters.forEach((ch, i) => {
    const c = chs[i]
    const level = nodeId(ch.id, 'level')
    const gotosOf = (which: SceneKey) => {
      for (const g of sceneGotos(ctx, sceneOf(doc, ch.id, which))) {
        const t = indexOf.get(g.to)
        if (t !== undefined) add(nodeId(ch.id, which), chs[t].head, 'goto', g.label, i, t)
      }
    }
    if (c.hasBefore) {
      if (!alwaysGoes(sceneOf(doc, ch.id, 'before'))) add(nodeId(ch.id, 'before'), level, 'order', '', i, i)
      gotosOf('before')
    }
    if (c.hasAfter) {
      add(level, nodeId(ch.id, 'after'), 'order', '', i, i)
      gotosOf('after')
    }
    // Where the chapter leads when no goto ran: its "Next chapter", else the following one; the last chapter ends the story.
    const falls = c.hasAfter ? !alwaysGoes(sceneOf(doc, ch.id, 'after')) : true
    const following = chapters[i + 1]?.id
    const nextId = ch.next || following
    c.ends = !nextId
    const t = nextId ? indexOf.get(nextId) : undefined
    if (falls && t !== undefined) {
      const plain = nextId === following
      add(c.tail, chs[t].head, plain ? 'order' : 'next', '', i, t, plain ? { before: ch.id, after: nextId! } : undefined)
    }
  })

  // Reach: from the first chapter's first box along every arrow.
  const out = new Map<string, string[]>()
  for (const e of edges) out.set(e.from, [...(out.get(e.from) ?? []), e.to])
  const reached = new Set<string>()
  const todo = chs.length ? [chs[0].head] : []
  while (todo.length) {
    const id = todo.pop()!
    if (reached.has(id)) continue
    reached.add(id)
    todo.push(...(out.get(id) ?? []))
  }
  for (const c of chs) c.unreached = !reached.has(c.head)
  for (const n of nodes) {
    const c = chs[n.chapterIndex]
    n.unreached = c.unreached
    n.skipped = !c.unreached && !reached.has(n.id)
  }
  return { nodes, edges, chapters: chs, unreached: chs.filter((c) => c.unreached).map((c) => c.id) }
}

function countAll(list: ReturnType<typeof sceneCommands>): number {
  let n = 0
  const walk = (l: ReturnType<typeof sceneCommands> | undefined) => {
    for (const c of l ?? []) { n++; walk(c.then); walk(c.else); for (const a of c.answers ?? []) walk(a.do) }
  }
  walk(list)
  return n
}
