// A level's in-map events in rough order for the timeline: start of battle first, then the turn-numbered ones ascending ("every
// turn" after them), then the conditional ones grouped by what triggers them. The file's own order breaks ties.
import { triggerKind, type TriggerKind } from './storyTypes'
import { triggerText } from './storySummary'
import { countMarkers } from './timelineMarkers'
import type { EventGroup, TimelineEvent } from './timelineTypes'
import type { ProjectDoc } from './types'

/** Order of the groups; the conditional ones follow the start and the turns. */
export const EVENT_GROUPS: EventGroup[] = ['start', 'turn', 'reach', 'talk', 'defeated', 'fight', 'promoted', 'support', 'won', 'lost', 'end', 'other']

export function groupOf(kind: TriggerKind | null): EventGroup {
  switch (kind) {
    case 'start': case 'beforePrep': return 'start'
    case 'turn': case 'everyTurn': return 'turn'
    case 'reach': case 'talk': case 'defeated': case 'fight': case 'promoted': case 'support': case 'won': case 'lost': case 'end': return kind
    default: return 'other'
  }
}

export function orderedEvents(doc: ProjectDoc, chapterId: string, mapId: string): TimelineEvent[] {
  const map = doc.maps[mapId]
  if (!map?.events?.length) return []
  const ctx = { doc, map }
  const rows = map.events.map((e, i) => {
    const kind = triggerKind(e.when ?? {})
    const turn = kind === 'turn' ? Number(e.when.turn) : kind === 'everyTurn' ? Number.MAX_SAFE_INTEGER : 0
    const ev: TimelineEvent = {
      key: `${chapterId}/${i}`, chapterId, mapId, eventId: e.id, group: groupOf(kind), triggerKind: kind,
      when: triggerText(ctx, e.when ?? {}), markers: countMarkers(e.do),
    }
    return { ev, i, turn: Number.isFinite(turn) ? turn : 0 }
  })
  rows.sort((a, b) => EVENT_GROUPS.indexOf(a.ev.group) - EVENT_GROUPS.indexOf(b.ev.group) || a.turn - b.turn || a.i - b.i)
  return rows.map((r) => r.ev)
}
