// Editing data/campaign.json: pure functions on the document (docs/EVENTS.md "Campaign"). A rule that is set to the
// loader's default is written as NO key, like the saving setting; an emptied `rules` block leaves its key out.
import { withCampaign } from './doc'
import { makeId } from './ids'
import { sceneEvent, sceneEventId, sceneMap } from './campaignRoster'
import { walkCampaign } from './campaignWalk'
import type { Campaign, Chapter, DefeatRule, FallenRule, SceneKey } from './campaignTypes'
import { DROP } from './storyWalk'
import type { Command } from './storyTypes'
import { moveItem, patchObj } from './util'
import type { MapFile, ProjectDoc } from './types'

export const campaignOf = (doc: ProjectDoc): Campaign | null => doc.campaign

export const newChapterId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.campaign?.chapters ?? []).map((c) => c.id), { fallback: 'chapter' })

/** A campaign with one chapter on the game's first map: valid straight away. */
export function createCampaign(doc: ProjectDoc): ProjectDoc {
  if (doc.campaign || !doc.game) return doc
  const first: Chapter = { id: 'chapter_1', name: 'Chapter 1', map: doc.game.maps[0]?.id ?? '' }
  return { ...doc, campaign: { name: doc.game.name || 'Campaign', chapters: [first] } }
}

export const removeCampaign = (doc: ProjectDoc): ProjectDoc => (doc.campaign ? { ...doc, campaign: null } : doc)

const patchCampaign = (doc: ProjectDoc, patch: Record<string, unknown>): ProjectDoc => withCampaign(doc, (c) => patchObj(c, patch))

export const setCampaignName = (doc: ProjectDoc, name: string): ProjectDoc => patchCampaign(doc, { name: name === '' ? undefined : name })

// --- rules ---
const setRule = (doc: ProjectDoc, key: 'fallen' | 'onDefeat', value: string | undefined): ProjectDoc =>
  withCampaign(doc, (c) => {
    const rules = patchObj(c.rules ?? {}, { [key]: value })
    return patchObj(c, { rules: Object.keys(rules).length ? rules : undefined })
  })

export const fallenRule = (c: Campaign): FallenRule => (c.rules?.fallen === 'return' ? 'return' : 'gone')
export const defeatRule = (c: Campaign): DefeatRule => (c.rules?.onDefeat === 'end' ? 'end' : 'retry')
export const setFallenRule = (doc: ProjectDoc, v: FallenRule): ProjectDoc => setRule(doc, 'fallen', v === 'gone' ? undefined : v)
export const setDefeatRule = (doc: ProjectDoc, v: DefeatRule): ProjectDoc => setRule(doc, 'onDefeat', v === 'retry' ? undefined : v)

// --- free battles ---
export type FreeMode = 'none' | 'all' | 'chosen'
export const freeMode = (c: Campaign): FreeMode => (c.freeBattles === 'all' ? 'all' : Array.isArray(c.freeBattles) ? 'chosen' : 'none')
export const freeMaps = (c: Campaign): string[] => (Array.isArray(c.freeBattles) ? c.freeBattles : [])

export function setFreeMode(doc: ProjectDoc, mode: FreeMode): ProjectDoc {
  return withCampaign(doc, (c) => (freeMode(c) === mode ? c : patchObj(c, { freeBattles: mode === 'none' ? undefined : mode === 'all' ? 'all' : [] })))
}

/** Ticks or unticks one map of the chosen list; the list keeps the order of the game's map list. */
export function setFreeMap(doc: ProjectDoc, mapId: string, on: boolean): ProjectDoc {
  return withCampaign(doc, (c) => {
    if (freeMode(c) !== 'chosen') return c
    const have = new Set(freeMaps(c))
    if (on) have.add(mapId); else have.delete(mapId)
    const order = (doc.game?.maps ?? []).map((m) => m.id)
    const list = [...order.filter((id) => have.has(id)), ...[...have].filter((id) => !order.includes(id))]
    return patchObj(c, { freeBattles: list })
  })
}

// --- chapters ---
export function addChapter(doc: ProjectDoc, name: string, id: string = newChapterId(doc, name)): ProjectDoc {
  const map = doc.campaign?.chapters.at(-1)?.map || doc.game?.maps[0]?.id || ''
  return withCampaign(doc, (c) => ({ ...c, chapters: [...c.chapters, { id, name, map }] }))
}

/** Deletes the chapter; other chapters' "next" and "go to" that named it are cleaned up in the same step. */
export function deleteChapter(doc: ProjectDoc, id: string): ProjectDoc {
  return withCampaign(doc, (c) => {
    if (!c.chapters.some((x) => x.id === id)) return c
    const rest: Campaign = { ...c, chapters: c.chapters.filter((x) => x.id !== id) }
    return walkCampaign(rest, { ref: (r) => (r.kind === 'chapter' && r.id === id ? DROP : r.id) })
  })
}

export const moveChapter = (doc: ProjectDoc, from: number, to: number): ProjectDoc =>
  withCampaign(doc, (c) => ({ ...c, chapters: moveItem(c.chapters, from, to) }))

/** `undefined` removes a key (name, next). */
export const patchChapter = (doc: ProjectDoc, id: string, patch: Record<string, unknown>): ProjectDoc =>
  withCampaign(doc, (c) => ({ ...c, chapters: c.chapters.map((x) => (x.id === id ? patchObj(x, patch) : x)) }))

/** Chapter names may be left empty (the loader then shows the id): that is written as no key. */
export const setChapterName = (doc: ProjectDoc, id: string, name: string): ProjectDoc => patchChapter(doc, id, { name: name === '' ? undefined : name })
export const setChapterMap = (doc: ProjectDoc, id: string, map: string): ProjectDoc => patchChapter(doc, id, { map })
export const setChapterNext = (doc: ProjectDoc, id: string, next: string | undefined): ProjectDoc => patchChapter(doc, id, { next: next || undefined })

// --- scenes, edited as a "scene map" (see campaignRoster.ts) ---
export const sceneOf = (doc: ProjectDoc, chapterId: string, which: SceneKey): Command[] =>
  doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which] ?? []

/** The scene as the one event of the scene map, so the map editor's command operations apply to it. */
export function sceneAsMap(doc: ProjectDoc, chapterId: string, which: SceneKey): MapFile {
  return { ...sceneMap(doc), events: [sceneEvent(sceneEventId(chapterId, which), sceneOf(doc, chapterId, which))] }
}

/** Writes back the commands of the scene map's event. An emptied scene is written as no key. */
export function withScene(doc: ProjectDoc, chapterId: string, which: SceneKey, edited: MapFile): ProjectDoc {
  const list = edited.events?.[0]?.do ?? []
  if (list === sceneOf(doc, chapterId, which) || (list.length === 0 && !doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which])) return doc
  return patchChapter(doc, chapterId, { [which]: list.length ? list : undefined })
}
