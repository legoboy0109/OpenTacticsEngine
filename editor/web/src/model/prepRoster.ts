// Who can be in a campaign's party: the named units of human sides that any map places or adds (campaignRoster.ts's rule),
// as ids; and the lists the Preparation tab picks from.
import { storyIdOf } from './characterUnits'
import { rosterUnits } from './campaignRoster'
import type { ProjectDoc } from './types'

export function campaignRosterIds(doc: ProjectDoc): Set<string> {
  const ids = new Set<string>()
  for (const u of rosterUnits(doc)) {
    const id = storyIdOf(u)
    if (id) ids.add(id)
  }
  return ids
}

export interface Person { id: string; name: string }

/** Everyone a preparation can name: the party's units, then characters nobody places yet. Each id once. */
export function peopleOf(doc: ProjectDoc): Person[] {
  const out: Person[] = []
  const add = (id: string | undefined, name: string | undefined) => { if (id && !out.some((p) => p.id === id)) out.push({ id, name: name || id }) }
  for (const u of rosterUnits(doc)) {
    const id = storyIdOf(u)
    add(id, u.name ?? doc.characters?.find((c) => c.id === id)?.name)
  }
  for (const c of doc.characters ?? []) add(c.id, c.name)
  return out
}
