// A terrain's standing part in a 3D style (docs/GRAPHICS.md "Standing terrain"): the `stand` key of its look, either an upright
// picture ({ sheet, cell | variants | frames, frameMs, width }) or models ({ model | variants: [files], height, turn }), and
// `scatter` for both. Mirrors engine/core/data/loader_graphics.cpp (`readStand`), which stays the source of truth for the
// ranges and the words of its refusals. Pure: no browser APIs.
import { getLook, setLookExact, type Slot } from './graphicsLooks'
import { patchObj } from './util'
import type { CellRef, Look, ProjectDoc, Stand } from './types'

export const STAND_WIDTH = { min: 1, max: 512 }
export const STAND_HEIGHT = { min: 0.1, max: 16 }
export const STAND_TURN = { min: -360, max: 360 }
/** The height a standing model has when the entry does not say (squares). */
export const STAND_DEFAULT_HEIGHT = 1
export const MAX_STAND_MODELS = 64

export type StandKind = 'none' | 'picture' | 'model'

/** A model stand lists files in `model` (one) or in `variants` (several, as text); a picture stand has a `sheet`. */
export function standKind(s: Stand | null | undefined): StandKind {
  if (!s) return 'none'
  if (typeof s.model === 'string' || (Array.isArray(s.variants) && s.variants.length > 0 && typeof s.variants[0] === 'string')) return 'model'
  return typeof s.sheet === 'string' ? 'picture' : 'none'
}

/** The model files of a stand, in order (one or several). */
export function standModels(s: Stand | null | undefined): string[] {
  if (!s) return []
  if (typeof s.model === 'string') return [s.model]
  return Array.isArray(s.variants) ? (s.variants as unknown[]).filter((v): v is string => typeof v === 'string') : []
}

/** The picture part of a stand as an ordinary look (so the preview and the piece picker can use it), or null. */
export function standPictureLook(s: Stand | null | undefined): Look | null {
  if (!s || standKind(s) !== 'picture') return null
  const look: Look = { sheet: s.sheet as string }
  if (s.cell) look.cell = s.cell
  if (s.variants) look.variants = s.variants as CellRef[]
  if (s.frames) look.frames = s.frames
  if (s.frameMs !== undefined) look.frameMs = s.frameMs
  return look
}

/** The stand stored in a terrain's slot, if the slot has an entry of its own with one. */
export function standOf(doc: ProjectDoc, s: Slot): Stand | undefined {
  const entry = getLook(doc, s)
  return entry ? entry.stand : undefined
}

/** Writes the stand into the terrain's entry (undefined removes it). Does nothing without an entry: the standing part stands on
 *  the ground picture, so a terrain with no picture in the style cannot have one. */
export function setStand(doc: ProjectDoc, s: Slot, next: Stand | undefined): ProjectDoc {
  const cur = getLook(doc, s)
  if (!cur) return doc
  if (JSON.stringify(cur.stand) === JSON.stringify(next)) return doc
  return setLookExact(doc, s, patchObj(cur, { stand: next }))
}

const keep = (cur: Stand | undefined, keys: Array<keyof Stand>): Partial<Stand> => {
  const out: Record<string, unknown> = {}
  for (const k of keys) if (cur && cur[k] !== undefined) out[k] = cur[k]
  return out as Partial<Stand>
}

/** Sets the picture of the standing part from a look the piece picker made (cell, variants or frames). Width and scatter stay. */
export function setStandPicture(doc: ProjectDoc, s: Slot, look: Look): ProjectDoc {
  const cur = standOf(doc, s)
  const same = standKind(cur) === 'picture'
  const next: Stand = { sheet: look.sheet }
  if (look.cell) next.cell = look.cell
  else if (look.variants) next.variants = look.variants
  else if (look.frames) { next.frames = look.frames; if (look.frameMs !== undefined) next.frameMs = look.frameMs }
  return setStand(doc, s, { ...next, ...keep(same ? cur : undefined, ['width', 'scatter']) })
}

/** Sets the model files (none = no standing part). One file is written as `model`, several as `variants`. Height, turn and scatter stay. */
export function setStandModels(doc: ProjectDoc, s: Slot, files: string[]): ProjectDoc {
  const list = files.slice(0, MAX_STAND_MODELS)
  if (list.length === 0) return setStand(doc, s, undefined)
  const cur = standOf(doc, s)
  const base: Stand = list.length === 1 ? { model: list[0] } : { variants: list }
  return setStand(doc, s, { ...base, ...keep(standKind(cur) === 'model' ? cur : undefined, ['height', 'turn', 'scatter']) })
}

export const addStandModel = (doc: ProjectDoc, s: Slot, file: string): ProjectDoc => {
  const have = standModels(standOf(doc, s))
  return have.includes(file) ? doc : setStandModels(doc, s, [...have, file])
}
export const removeStandModel = (doc: ProjectDoc, s: Slot, file: string): ProjectDoc =>
  setStandModels(doc, s, standModels(standOf(doc, s)).filter((f) => f !== file))

function setKey(doc: ProjectDoc, s: Slot, key: 'width' | 'height' | 'turn' | 'scatter', value: number | boolean | undefined): ProjectDoc {
  const cur = standOf(doc, s)
  return cur ? setStand(doc, s, patchObj(cur, { [key]: value })) : doc
}
/** Empty = the width of one square (not written). */
export const setStandWidth = (doc: ProjectDoc, s: Slot, v: number | undefined): ProjectDoc => setKey(doc, s, 'width', v)
/** Empty = the standard height (not written). */
export const setStandHeight = (doc: ProjectDoc, s: Slot, v: number | undefined): ProjectDoc => setKey(doc, s, 'height', v)
/** 0 is the default, so it is not written. */
export const setStandTurn = (doc: ProjectDoc, s: Slot, v: number | undefined): ProjectDoc => setKey(doc, s, 'turn', v === 0 ? undefined : v)
/** Off is the default, so it is not written. */
export const setStandScatter = (doc: ProjectDoc, s: Slot, on: boolean): ProjectDoc => setKey(doc, s, 'scatter', on ? true : undefined)

/** What the fields show for a stand. */
export interface StandFields { width: number | undefined; height: number | undefined; turn: number | undefined; scatter: boolean }
export function standFields(s: Stand | null | undefined): StandFields {
  return {
    width: typeof s?.width === 'number' ? s.width : undefined,
    height: typeof s?.height === 'number' ? s.height : undefined,
    turn: typeof s?.turn === 'number' ? s.turn : undefined,
    scatter: s?.scatter === true,
  }
}
