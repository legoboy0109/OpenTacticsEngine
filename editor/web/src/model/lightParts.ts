// Small edits for the fog / map change keys of the data files (docs/ECONOMY.md "Fog of war"): a structure kind's fixed `light`, a terrain's
// `blocksSight`, a torch item's `light` block and a door's `word`. Defaults are never written: a key is present only when it differs.
import { updateItem } from './items'
import { patchPlace } from './places'
import { updateStructure } from './structures'
import { updateTerrain } from './terrain'
import type { ProjectDoc } from './types'

export const LIGHT_MAX = 10
const whole = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

/** A fixed light: radius 1..10; 0 or undefined removes the key. */
export const setStructureLight = (doc: ProjectDoc, id: string, n: number | undefined): ProjectDoc =>
  updateStructure(doc, id, { light: n === undefined || n <= 0 ? undefined : whole(n, 1, LIGHT_MAX) })

export const setTerrainBlocksSight = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateTerrain(doc, id, { blocksSight: on ? true : undefined })

/** What the menu calls a breakable door; empty removes the key ("door"). */
export const setDoorWord = (doc: ProjectDoc, id: string, word: string): ProjectDoc => patchPlace(doc, id, { word: word.trim() === '' ? undefined : word.slice(0, 16) })

export const DEFAULT_TORCH = { radius: 3, turns: 3 }

/** Switches a torch's light on (a default block) or off (the key goes). */
export const setItemLightOn = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateItem(doc, id, { light: on ? { ...DEFAULT_TORCH } : undefined })

type LightKey = 'radius' | 'turns' | 'shrink'
/** One number of the light block. `shrink` 0 or undefined removes it; a radius that no longer leaves room for the shrink pulls the shrink down. */
export function setItemLight(doc: ProjectDoc, id: string, key: LightKey, n: number | undefined): ProjectDoc {
  const item = doc.items?.find((i) => i.id === id)
  if (!item?.light) return doc
  const l = { ...item.light }
  if (n === undefined || (key === 'shrink' && n <= 0)) delete l[key]
  else l[key] = whole(n, 1, LIGHT_MAX)
  if (l.radius !== undefined && l.shrink !== undefined && l.shrink >= l.radius) l.shrink = Math.max(1, l.radius - 1)
  if (l.radius !== undefined && l.radius <= 1) delete l.shrink
  return updateItem(doc, id, { light: l })
}
