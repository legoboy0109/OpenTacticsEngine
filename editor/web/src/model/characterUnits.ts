// A map unit placed as a character (docs/SRPG.md section 1) has no `type` of its own unless the map overrides it: its
// type is the character's. Everything that needs "which unit type is this unit" asks here, so the rule is written once.
import type { Character } from './srpgTypes'
import type { MapFile, MapUnit } from './types'

/** The unit type id a placed unit has on its map ('' when a character is gone: the loader's message says so). */
export const typeIdOf = (characters: Character[] | null | undefined, u: Pick<MapUnit, 'type' | 'character'>): string =>
  u.type ?? (u.character !== undefined ? characters?.find((c) => c.id === u.character)?.type : undefined) ?? ''

/** The name events and objectives use for the unit: its own `id`, or for a character the character's id. */
export const storyIdOf = (u: Pick<MapUnit, 'id' | 'character'>): string | undefined =>
  typeof u.id === 'string' && u.id !== '' ? u.id : typeof u.character === 'string' && u.character !== '' ? u.character : undefined

/** The same map with every unit's `type` filled in (for drawing and checks that only read the type). Same object when
 *  no unit needed it. */
export function withResolvedTypes(map: MapFile, characters: Character[] | null | undefined): MapFile {
  if (!map.units?.some((u) => u.type === undefined)) return map
  return { ...map, units: map.units.map((u) => (u.type === undefined ? { ...u, type: typeIdOf(characters, u) } : u)) }
}

/** The characters a map places (a character can stand on a map once). */
export const placedCharacterIds = (map: MapFile | null | undefined): string[] =>
  (map?.units ?? []).flatMap((u) => (typeof u.character === 'string' ? [u.character] : []))
