// Which problems belong to which box of the timeline.
//  - Scene boxes: the Campaign checks that name that chapter's "Before the battle" / "After the battle" scene.
//  - Level boxes: the Campaign checks about the chapter itself (its map, "nothing leads here"), and the checks of the chapter's
//    map events and objectives (the same ones the map editor's Events tab shows).
// Those checks are live (they follow every keystroke). The helper's own Problems list (what the real game loader said at the
// last save) is used for a box only when the live checks have nothing for it, so one mistake is not counted twice.
import { campaignChecks } from './campaignChecks'
import { SCENE_TITLE } from './campaignWalk'
import { storyChecks, type StoryCheck } from './storyChecks'
import type { TimelineGraph } from './timelineTypes'
import type { Problem, ProjectDoc } from './types'

export interface BoxProblems { byNode: Map<string, string[]>; byEvent: Map<string, number> }

export function timelineProblems(doc: ProjectDoc, helper: Problem[], g: TimelineGraph): BoxProblems {
  const byNode = new Map<string, string[]>()
  const byEvent = new Map<string, number>()
  const push = (id: string, text: string) => { byNode.set(id, [...(byNode.get(id) ?? []), text]) }
  const level = (chapterId: string) => `${chapterId}:level`

  for (const c of campaignChecks(doc)) {
    if (!c.chapterId) continue
    const scene = (['before', 'after'] as const).find((w) => c.text.includes(`“${SCENE_TITLE[w]}”`))
    push(scene ? `${c.chapterId}:${scene}` : level(c.chapterId), c.text)
  }
  const checkedMaps = new Map<string, StoryCheck[]>()
  for (const n of g.nodes) {
    if (n.kind !== 'level' || !n.mapId || !n.mapKnown) continue
    if (!checkedMaps.has(n.mapId)) checkedMaps.set(n.mapId, storyChecks(doc, n.mapId))
    for (const c of checkedMaps.get(n.mapId)!) {
      push(n.id, c.text)
      if (c.eventId) byEvent.set(`${n.chapterId}/${c.eventId}`, (byEvent.get(`${n.chapterId}/${c.eventId}`) ?? 0) + 1)
    }
  }

  // The helper's list, for boxes the live checks say nothing about.
  const later = new Map<string, string[]>()
  const fill = (id: string, text: string) => { if (!byNode.has(id)) later.set(id, [...(later.get(id) ?? []), text]) }
  for (const p of helper) {
    if (p.file === 'data/campaign.json') {
      const ch = /chapter "([^"]+)"/.exec(p.message)?.[1]
      if (!ch) continue
      const which = /"(before|after)"/.exec(p.message)?.[1]
      fill(`${ch}:${which ?? 'level'}`, p.message)
    } else if (p.file.startsWith('maps/')) {
      const map = doc.game?.maps.find((m) => m.file === p.file)
      if (!map) continue
      for (const n of g.nodes) if (n.kind === 'level' && n.mapId === map.id) fill(n.id, p.message)
    }
  }
  for (const [id, list] of later) byNode.set(id, list)
  return { byNode, byEvent }
}
