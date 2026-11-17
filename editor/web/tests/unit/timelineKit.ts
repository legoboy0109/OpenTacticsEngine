// Small documents for the timeline tests: a campaign over a few bare maps (only what the graph reads is filled in).
import type { Chapter } from '../../src/model/campaignTypes'
import type { Command, StoryEvent } from '../../src/model/storyTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'

export const say = (t = 'Hello.'): Command => ({ say: t, speakerName: 'N' })

export function bareMap(events: StoryEvent[] = [], extra: Partial<MapFile> = {}): MapFile {
  return { name: 'm', grid: { width: 4, height: 3 }, legend: { '.': 'grass' }, tiles: ['....', '....', '....'], sides: [{ id: 'blue', name: 'Blue', color: '#3366cc' }], units: [], events, ...extra } as unknown as MapFile
}

/** A document with these chapters; every map id used by a chapter exists (named after its id) unless `maps` says otherwise. */
export function campaignDoc(chapters: Chapter[], maps: Record<string, MapFile | null> = {}, extra: Partial<ProjectDoc> = {}): ProjectDoc {
  const ids = [...new Set(chapters.map((c) => c.map).filter(Boolean))]
  const all: Record<string, MapFile | null> = {}
  for (const id of ids) all[id] = bareMap()
  Object.assign(all, maps)
  return {
    game: { name: 'G', maps: Object.keys(all).map((id) => ({ id, name: `Map ${id}`, file: `maps/${id}.json` })), switches: [{ id: 'crossed', name: 'Crossed' }], variables: [{ id: 'pack', name: 'Pack' }] },
    terrain: [{ id: 'grass', name: 'Grass', color: '#88aa55', glyph: '.' }], structures: null, weapons: [], items: null, research: null, units: [], maps: all,
    scripts: {}, graphics: null, campaign: { name: 'C', chapters }, audio: null, characters: null, skills: null, abilities: null, supports: null, ranks: null, caps: null,
    ...extra,
  } as unknown as ProjectDoc
}

/** `n` chapters ch1..chN on map "a", in a straight line. */
export const straight = (n: number): Chapter[] => Array.from({ length: n }, (_, i) => ({ id: `ch${i + 1}`, name: `Chapter ${i + 1}`, map: 'a' }))
