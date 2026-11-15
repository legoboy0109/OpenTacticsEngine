// What a campaign holds about the rest of the game, for the three moments that matter (like storyRefs.ts does for maps):
//  - the game changed under it (a switch, variable, resource, side, map or a unit's story name is gone): `reconcileCampaign`
//    removes what can no longer work. The document store runs it after EVERY edit, so no delete path can forget it, and
//    it is part of the same undo step;
//  - a unit's story name was renamed: `followUnitRename` makes the scenes follow (run before the reconcile);
//  - "used by" lines for a delete confirmation: `campaignUsages`.
// Only what was there BEFORE the edit and is gone AFTER it is removed; a name that was already dangling stays, so the
// checks can point at it instead of the editor silently deleting writing.
import type { Usage } from './references'
import { walkCampaign } from './campaignWalk'
import { allSideIds, definedStoryIds, sceneMap } from './campaignRoster'
import { commandText } from './storySummary'
import { DROP, type Dropping, type Ref } from './storyWalk'
import type { ProjectDoc } from './types'

const ids = (list: Array<{ id: string }> | undefined): Set<string> => new Set((list ?? []).map((x) => x.id))

/** Names present in `before` and missing from `after` (empty when either side's game.json could not be read). */
export function goneRefs(before: ProjectDoc, after: ProjectDoc): Ref[] {
  if (!before.game || !after.game) return []
  const out: Ref[] = []
  const lost = (kind: Ref['kind'], a: Set<string>, b: Set<string>) => { for (const id of a) if (!b.has(id)) out.push({ kind, id }) }
  lost('switch', ids(before.game.switches), ids(after.game.switches))
  lost('variable', ids(before.game.variables), ids(after.game.variables))
  lost('resource', ids(before.game.resources), ids(after.game.resources))
  lost('map', ids(before.game.maps), ids(after.game.maps))
  lost('side', allSideIds(before), allSideIds(after))
  lost('unit', definedStoryIds(before), definedStoryIds(after))
  return out
}

const hit = (gone: Ref[], r: Ref) => gone.some((g) => g.kind === r.kind && g.id === r.id)

/** Removes what the campaign names that `before` had and `after` no longer has. Same object when nothing is lost. */
export function reconcileCampaign(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (!after.campaign || before === after) return after
  if (before.maps === after.maps && before.game === after.game) return after
  const gone = goneRefs(before, after)
  if (!gone.length) return after
  const next = walkCampaign(after.campaign, { ref: (r) => (hit(gone, r) ? DROP : r.id) })
  return next === after.campaign ? after : { ...after, campaign: next }
}

/** Every scene that named unit `from` now names `to` — but only when no map has the old story name any more
 *  (another map's unit with that name is still the same roster member). */
export function followUnitRename(before: ProjectDoc, after: ProjectDoc, from: string, to: string): ProjectDoc {
  if (!after.campaign || !from || !to || from === to || before === after) return after
  if (definedStoryIds(after).has(from)) return after
  const next = walkCampaign(after.campaign, { ref: (r) => (r.kind === 'unit' && r.id === from ? to : r.id) })
  return next === after.campaign ? after : { ...after, campaign: next }
}

const sentence = (what: Dropping['what'], text: string, where: string): string => {
  switch (what) {
    case 'chapterMap': return `Campaign, ${where}: its map is cleared (the scenes are kept; pick another map)`
    case 'link': return `Campaign, ${where}: “Next chapter” goes back to “the following one”`
    case 'freeBattle': return `Campaign: the map is taken off the list of free battles`
    case 'command': return `Campaign, ${where}: “${text}” is removed`
    case 'resource': return `Campaign, ${where}: “${text}” loses this resource`
    default: return `Campaign, ${where}: ${text}`
  }
}

/** Lines for what the campaign would lose if everything in `gone` went. */
export function campaignUsagesOf(doc: ProjectDoc, gone: Ref[]): Usage[] {
  if (!doc.campaign || !gone.length) return []
  const out: Usage[] = []
  const describe = { command: (c: Parameters<typeof commandText>[1]) => commandText({ doc, map: sceneMap(doc) }, c) }
  walkCampaign(doc.campaign, {
    ref: (r) => (hit(gone, r) ? DROP : r.id),
    describe,
    dropped: ({ what, text, where }) => out.push({ section: 'campaign', text: sentence(what, text, where) }),
  })
  return out
}

/** Campaign lines for deleting a part of a map (a unit, a side) or something of the game (a switch, ...): the names of
 *  `gone` that no OTHER map still has (`inMap` = the map the delete happens in; omitted = gone everywhere). */
export function campaignUsages(doc: ProjectDoc, gone: Ref[], inMap?: string): Usage[] {
  if (!doc.campaign) return []
  const others = inMap === undefined ? null : { ...doc, maps: Object.fromEntries(Object.entries(doc.maps).filter(([k]) => k !== inMap)) }
  const sides = others ? allSideIds(others) : null
  const units = others ? definedStoryIds(others) : null
  const real = gone.filter((r) => !(r.kind === 'side' && sides?.has(r.id)) && !(r.kind === 'unit' && units?.has(r.id)))
  return campaignUsagesOf(doc, real)
}

/** What deleting map `mapId` does to the campaign: its chapters lose the map, the free-battle list loses it, and the
 *  units and sides only it had are gone for the scenes. */
export function mapDeleteUsages(doc: ProjectDoc, mapId: string): Usage[] {
  if (!doc.campaign) return []
  const gone: Ref[] = [{ kind: 'map', id: mapId }]
  const others = { ...doc, maps: Object.fromEntries(Object.entries(doc.maps).filter(([k]) => k !== mapId)) }
  const sides = allSideIds(others)
  const units = definedStoryIds(others)
  for (const s of allSideIds({ ...doc, maps: { [mapId]: doc.maps[mapId] } })) if (!sides.has(s)) gone.push({ kind: 'side', id: s })
  for (const id of definedStoryIds({ ...doc, maps: { [mapId]: doc.maps[mapId] } })) if (!units.has(id)) gone.push({ kind: 'unit', id })
  return campaignUsagesOf(doc, gone)
}
