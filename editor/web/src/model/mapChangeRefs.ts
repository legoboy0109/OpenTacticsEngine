// What deleting a terrain or a structure kind leaves behind in the maps' named changes and events (docs/EVENTS.md "Changing the map"): the
// squares of a change that gave the terrain, a structure swap into the kind, and the setTerrain commands that name the terrain go with it.
import { cellsOf, changesOf, swapsOf } from './mapChanges'
import { scrubMap } from './storyRefs'
import type { MapFile, ProjectDoc } from './types'

function mapEach(doc: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc {
  let changed = false
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) {
    const next = m ? fn(m) : m
    if (next !== m) changed = true
    maps[id] = next
  }
  return changed ? { ...doc, maps } : doc
}

/** After a terrain is removed: change squares with it are dropped, and so are the commands that turn squares into it. */
export function dropTerrainFromChanges(doc: ProjectDoc, id: string): ProjectDoc {
  return mapEach(doc, (m) => {
    let next = m
    const have = changesOf(m)
    if (Object.values(have).some((c) => cellsOf(c).some((k) => k.terrain === id))) {
      next = { ...m, changes: Object.fromEntries(Object.entries(have).map(([k, c]) => [k, cellsOf(c).some((x) => x.terrain === id) ? { ...c, cells: cellsOf(c).filter((x) => x.terrain !== id) } : c])) } as MapFile
    }
    return scrubMap(doc, next, [{ kind: 'terrain', id }])
  })
}

/** After a structure kind is removed: swaps into it are dropped. */
export function dropKindFromChanges(doc: ProjectDoc, id: string): ProjectDoc {
  return mapEach(doc, (m) => {
    const have = changesOf(m)
    if (!Object.values(have).some((c) => swapsOf(c).some((s) => s.kind === id))) return m
    return { ...m, changes: Object.fromEntries(Object.entries(have).map(([k, c]) => {
      if (!swapsOf(c).some((s) => s.kind === id)) return [k, c]
      const list = swapsOf(c).filter((s) => s.kind !== id)
      const { structures: _gone, ...rest } = c
      return [k, list.length ? { ...rest, structures: list } : rest]
    })) } as MapFile
  })
}
