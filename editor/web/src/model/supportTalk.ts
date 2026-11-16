// What a support conversation's story forms see (E4 of docs/SRPG.md). The lines are spoken by CHARACTERS, wherever on the maps
// they stand, so the "map" the forms work on holds every character of the game as a named unit (its id is the character id,
// which is what `speaker` holds in the file). It is made up and never saved.
import { sceneMap } from './campaignRoster'
import type { MapFile, ProjectDoc } from './types'

export function talkBase(doc: ProjectDoc): MapFile {
  const base = sceneMap(doc)
  return { ...base, units: (doc.characters ?? []).map((c) => ({ character: c.id, side: 'cast', x: 0, y: 0 })) }
}
