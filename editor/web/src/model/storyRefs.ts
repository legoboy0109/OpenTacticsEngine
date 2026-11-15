// What events and objectives hold about other things, for the three moments that matter:
//  - renaming a unit's story id (every place that names it follows),
//  - asking "what uses this?" before a delete (the "used by" lines),
//  - cleaning up when something is deleted (what cannot work any more is removed, all in the same undo step).
import { definedUnitIds } from './storyIds'
import { describeFor } from './storySummary'
import { DROP, walkStory, type Ref, type Walk } from './storyWalk'
import type { Usage } from './references'
import type { MapFile, ProjectDoc } from './types'

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id

/** A win / lose names a team or a side id, so deleting either one takes it away. */
const hits = (gone: Ref[], r: Ref): boolean =>
  gone.some((g) => (g.kind === r.kind && g.id === r.id) || (r.kind === 'teamOrSide' && (g.kind === 'side' || g.kind === 'team') && g.id === r.id))

/** Rewrites every place in the map that names unit `from` so that it names `to`. */
export function renameUnitRefs(map: MapFile, from: string, to: string): MapFile {
  if (from === to || (!map.events && !map.objectives)) return map
  return walkStory(map, { ref: (r) => (r.kind === 'unit' && r.id === from ? to : r.id) })
}

/** Removes what names anything in `gone`. Removing an added unit can leave names of ITS id dangling in other events;
 *  those go too (repeated until nothing new is lost). */
export function scrubMap(doc: ProjectDoc, map: MapFile, gone: Ref[]): MapFile {
  if (!map.events && !map.objectives) return map
  let cur = map
  let all = gone
  for (let round = 0; round < 6; round++) {
    const next = walkStory(cur, { ref: (r) => (hits(all, r) ? DROP : r.id) })
    if (next === cur) break
    const still = new Set(definedUnitIds(doc, next))
    const lost = definedUnitIds(doc, cur).filter((id) => !still.has(id) && !hits(all, { kind: 'unit', id }))
    cur = next
    if (!lost.length) break
    all = [...all, ...lost.map((id): Ref => ({ kind: 'unit', id }))]
  }
  return cur
}

/** The document's maps after `scrubMap` on each (or only `mapId`); the same object when nothing changed. */
export function scrubMaps(doc: ProjectDoc, gone: Ref[], only?: string): ProjectDoc['maps'] {
  let changed = false
  const out: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) {
    const next = m && (only === undefined || only === id) ? scrubMap(doc, m, gone) : m
    if (next !== m) changed = true
    out[id] = next
  }
  return changed ? out : doc.maps
}

export const scrubDoc = (doc: ProjectDoc, gone: Ref[], only?: string): ProjectDoc => {
  const maps = scrubMaps(doc, gone, only)
  return maps === doc.maps ? doc : { ...doc, maps }
}

/** Lines for a delete confirmation: what in the events and objectives would be removed. */
export function storyUsages(doc: ProjectDoc, gone: Ref[], only?: string): Usage[] {
  const out: Usage[] = []
  for (const [id, m] of Object.entries(doc.maps)) {
    if (!m || (only !== undefined && only !== id)) continue
    const prefix = `Map “${mapName(doc, id)}”`
    walkStory(m, {
      ref: (r) => (hits(gone, r) ? DROP : r.id),
      describe: describeFor({ doc, map: m }),
      dropped: ({ what, text, where }) => {
        const line = what === 'event' ? `${prefix}: ${where} is removed (${text})`
          : what === 'command' ? `${prefix}, ${where}: “${text}” is removed`
          : what === 'rule' ? `${prefix}, ${where}: “${text}” is removed`
          : what === 'resource' ? `${prefix}, ${where}: “${text}” loses this resource`
          : `${prefix}, ${where}: “${text}” loses this item`
        out.push({ section: 'maps', text: line })
      },
    } satisfies Walk)
  }
  return out
}

/** Units of a map that events or objectives point at by their story id, among `ids`. */
export const storyIdUsages = (doc: ProjectDoc, mapId: string, id: string): Usage[] =>
  storyUsages(doc, [{ kind: 'unit', id }], mapId)
