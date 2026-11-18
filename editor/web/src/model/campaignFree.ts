// The campaign's free battles (docs/EVENTS.md "Campaign"): pure functions on the document.
// "freeBattles" is "none" / "all" / a list of map ids (the old forms, which mean "when: always"), or the object form
// { maps, when, lockedNote }. An edit writes the old form whenever nothing needs the object, so a game that never uses
// "after the story is complete" keeps exactly the file it had.
import { withCampaign } from './doc'
import type { Campaign, FreeBattles, FreeBattlesObject, FreeMaps, FreeWhen } from './campaignTypes'
import { patchObj } from './util'
import type { ProjectDoc } from './types'

export type FreeMode = 'none' | 'all' | 'chosen'

const isObj = (f: FreeBattles | undefined): f is FreeBattlesObject => !!f && typeof f === 'object' && !Array.isArray(f)

/** The maps part, whichever form the file uses. */
export const freeMapsValue = (c: Campaign): FreeMaps => {
  const f = c.freeBattles
  const m = isObj(f) ? f.maps : f
  return m === 'all' || Array.isArray(m) ? m : 'none'
}
export const freeMode = (c: Campaign): FreeMode => {
  const m = freeMapsValue(c)
  return m === 'all' ? 'all' : Array.isArray(m) ? 'chosen' : 'none'
}
export const freeMaps = (c: Campaign): string[] => {
  const m = freeMapsValue(c)
  return Array.isArray(m) ? m : []
}
export const freeWhen = (c: Campaign): FreeWhen => (isObj(c.freeBattles) && c.freeBattles.when === 'storyComplete' ? 'storyComplete' : 'always')
export const freeNote = (c: Campaign): string => (isObj(c.freeBattles) ? c.freeBattles.lockedNote ?? '' : '')

/** Writes the three parts back in the simplest form that holds them. Keys the editor does not know stay in the object. */
function withFree(c: Campaign, maps: FreeMaps, when: FreeWhen, note: string): Campaign {
  const keep: FreeBattlesObject = isObj(c.freeBattles) ? c.freeBattles : {}
  const extras = Object.keys(keep).some((k) => k !== 'maps' && k !== 'when' && k !== 'lockedNote')
  if (when === 'always' && !note && !extras) return patchObj(c, { freeBattles: maps === 'none' ? undefined : maps })
  const obj: FreeBattlesObject = { ...keep, maps }
  if (when === 'storyComplete') obj.when = when
  else delete obj.when
  if (note) obj.lockedNote = note
  else delete obj.lockedNote
  return patchObj(c, { freeBattles: obj })
}

export function setFreeMode(doc: ProjectDoc, mode: FreeMode): ProjectDoc {
  return withCampaign(doc, (c) =>
    freeMode(c) === mode ? c : withFree(c, mode === 'none' ? 'none' : mode === 'all' ? 'all' : [], freeWhen(c), freeNote(c)))
}

/** Ticks or unticks one map of the chosen list; the list keeps the order of the game's map list. */
export function setFreeMap(doc: ProjectDoc, mapId: string, on: boolean): ProjectDoc {
  return withCampaign(doc, (c) => {
    if (freeMode(c) !== 'chosen') return c
    const have = new Set(freeMaps(c))
    if (on) have.add(mapId)
    else have.delete(mapId)
    const order = (doc.game?.maps ?? []).map((m) => m.id)
    const list = [...order.filter((id) => have.has(id)), ...[...have].filter((id) => !order.includes(id))]
    return withFree(c, list, freeWhen(c), freeNote(c))
  })
}

/** What the loader refuses about the object form, and what it accepts but a maker probably did not mean. */
export function freeBattleChecks(c: Campaign): Array<{ level: 'error' | 'hint'; text: string }> {
  const f = c.freeBattles
  if (!isObj(f)) return []
  const out: Array<{ level: 'error' | 'hint'; text: string }> = []
  if (f.maps === undefined) out.push({ level: 'error', text: 'the free battles need “maps” (all, or a list of maps).' })
  if (f.when !== undefined && f.when !== 'always' && f.when !== 'storyComplete') out.push({ level: 'error', text: '“when” must be “always” or “storyComplete”.' })
  if (f.lockedNote !== undefined && typeof f.lockedNote !== 'string') out.push({ level: 'error', text: 'the note must be text.' })
  if (freeWhen(c) === 'storyComplete' && freeMapsValue(c) !== 'all' && freeMaps(c).length === 0)
    out.push({ level: 'hint', text: 'the free battles unlock after the story, but no map is chosen, so nothing will ever unlock.' })
  if (freeWhen(c) === 'always' && freeNote(c)) out.push({ level: 'hint', text: 'the note is never shown: the free battles are always available.' })
  return out
}

/** When the game page lists the free battles. */
export function setFreeWhen(doc: ProjectDoc, when: FreeWhen): ProjectDoc {
  return withCampaign(doc, (c) => (freeWhen(c) === when ? c : withFree(c, freeMapsValue(c), when, freeNote(c))))
}

/** The greyed line shown while the list is locked; empty = no key. */
export function setFreeNote(doc: ProjectDoc, text: string): ProjectDoc {
  return withCampaign(doc, (c) => (freeNote(c) === text ? c : withFree(c, freeMapsValue(c), freeWhen(c), text)))
}
