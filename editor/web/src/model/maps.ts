// List-level map operations (new / rename / duplicate / delete / reorder).
// Editing a map's contents (tiles, units, sides) is the map editor's job.
import { clone, withGame } from './doc'
import { makeId } from './ids'
import { writeGrid } from './mapGrid'
import { moveItem } from './util'
import type { MapFile, ProjectDoc } from './types'

export const newMapId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.game?.maps ?? []).map((m) => m.id), { fallback: 'map' })

export function mapSize(doc: ProjectDoc, id: string): { width: number; height: number } | null {
  const m = doc.maps[id]
  return m ? { width: m.grid.width, height: m.grid.height } : null
}

/** A fresh, valid battlefield: all of the first terrain, two sides. */
export function blankMap(doc: ProjectDoc, name: string, width = 12, height = 8): MapFile {
  const terrain = doc.terrain ?? []
  const first = terrain[0]?.id ?? 'plain'
  const base: MapFile = {
    name,
    grid: { type: 'square', width, height },
    legend: {},
    tiles: [],
    sides: [
      { id: 'blue', name: 'Blue', color: '#3d85c6', controller: 'human' },
      { id: 'red', name: 'Red', color: '#cc0000', controller: 'ai' },
    ],
    units: [],
  }
  return writeGrid(base, { width, height, cells: new Array(width * height).fill(first) }, terrain.length ? terrain : [{ id: first, name: first }])
}

export function addMap(doc: ProjectDoc, name: string, id: string = newMapId(doc, name)): ProjectDoc {
  if (!doc.game) return doc
  const file = `maps/${id}.json`
  return withGame({ ...doc, maps: { ...doc.maps, [id]: blankMap(doc, name) } }, (g) => ({ ...g, maps: [...g.maps, { id, name, file }] }))
}

/** The display name lives both in game.json's map list and in the map file. */
export function renameMap(doc: ProjectDoc, id: string, name: string): ProjectDoc {
  const m = doc.maps[id]
  const next = withGame(doc, (g) => ({ ...g, maps: g.maps.map((r) => (r.id === id ? { ...r, name } : r)) }))
  return m ? { ...next, maps: { ...next.maps, [id]: { ...m, name } } } : next
}

export function duplicateMap(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  const ref = doc.game?.maps.find((m) => m.id === id)
  const src = doc.maps[id]
  if (!doc.game || !ref || !src) return doc
  const name = `${ref.name} copy`
  const nid = newId ?? newMapId(doc, name)
  const at = doc.game.maps.findIndex((m) => m.id === id)
  const maps = [...doc.game.maps]
  maps.splice(at + 1, 0, { ...ref, id: nid, name, file: `maps/${nid}.json` })
  return { ...doc, game: { ...doc.game, maps }, maps: { ...doc.maps, [nid]: { ...clone(src), name } } }
}

export function mapRemovalBlock(doc: ProjectDoc): string | null {
  return (doc.game?.maps.length ?? 0) <= 1 ? 'A game needs at least one map.' : null
}

/** The helper deletes the map's file on save once game.json no longer lists it. */
export function deleteMap(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.game || mapRemovalBlock(doc)) return doc
  const { [id]: _gone, ...rest } = doc.maps
  return { ...doc, game: { ...doc.game, maps: doc.game.maps.filter((m) => m.id !== id) }, maps: rest }
}

export function moveMap(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, maps: moveItem(g.maps, from, to) }))
}
