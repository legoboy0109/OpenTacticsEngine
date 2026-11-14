// 3D models of units and structures (docs/GRAPHICS.md "Models"): reading a .glb's names in the browser and editing an
// entry `{ model, height, turn, sideMaterial, animations }` of a 3D style. Mirrors engine/core/data/loader_graphics.cpp
// (`readModelLook`), which stays the source of truth for the ranges. Pure: no browser APIs.
import { getLook, setLook, type Slot } from './graphicsLooks'
import { withGraphics } from './graphics'
import { slugify, uniqueId } from './ids'
import { patchObj } from './util'
import type { Look, ProjectDoc } from './types'

export const MAX_MODEL_BYTES = 8 * 1024 * 1024
export const HEIGHT_RANGE = { min: 0.1, max: 16 }
export const TURN_RANGE = { min: -360, max: 360 }
export const DEFAULT_SIDE_MATERIAL = 'side'
export const ANIM_STATES = ['idle', 'move', 'attack', 'die'] as const
export type AnimState = (typeof ANIM_STATES)[number]
/** The clip names the player looks for when the entry does not say. */
export const DEFAULT_ANIM: Record<AnimState, string> = { idle: 'Idle', move: 'Move', attack: 'Attack', die: 'Die' }

/** What the editor shows of a model file: its animation and material names. */
export interface ModelFacts { animations: string[]; materials: string[] }

/** Names from a .glb's JSON chunk; null when the bytes are not a readable glTF binary. */
export function readGlbFacts(bytes: Uint8Array): ModelFacts | null {
  if (bytes.length < 20) return null
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  if (v.getUint32(0, true) !== 0x46546c67 /* "glTF" */ || v.getUint32(4, true) !== 2) return null
  const len = v.getUint32(12, true)
  if (v.getUint32(16, true) !== 0x4e4f534a /* "JSON" */ || 20 + len > bytes.length) return null
  let doc: unknown
  try { doc = JSON.parse(new TextDecoder().decode(bytes.subarray(20, 20 + len))) } catch { return null }
  const names = (list: unknown): string[] => {
    const out: string[] = []
    if (Array.isArray(list)) for (const x of list) if (x && typeof x.name === 'string' && x.name && !out.includes(x.name)) out.push(x.name)
    return out
  }
  const d = (doc ?? {}) as { animations?: unknown; materials?: unknown }
  return { animations: names(d.animations), materials: names(d.materials) }
}

/** "models/knight.glb" -> "knight". */
export const modelBase = (file: string): string => file.replace(/^models\//, '').replace(/\.(glb|gltf)$/i, '')
export const modelPath = (base: string): string => `models/${base}.glb`

/** Name on disk for an uploaded file: slug of the original name, unique among `existing` file paths. */
export function modelNameFor(originalName: string, existing: Iterable<string>): string {
  const stem = originalName.replace(/^.*[\\/]/, '').replace(/\.[A-Za-z0-9]{1,5}$/, '')
  return uniqueId(slugify(stem, { sep: '_', fallback: 'model', max: 40 }), [...existing].map(modelBase), '_')
}

/** A model entry is a look without a picture; it has no `sheet`. The casts are contained in this file. */
export const isModelEntry = (l: Look | null | undefined): boolean => !!l && typeof (l as { model?: unknown }).model === 'string'
export const modelFile = (l: Look | null | undefined): string | undefined => (isModelEntry(l) ? (l as unknown as { model: string }).model : undefined)

type Entry = Record<string, unknown>
const asEntry = (l: Look): Entry => l as unknown as Entry
const asLook = (e: Entry): Look => e as unknown as Look

/** Writes one model entry in a slot. A picture entry in the slot is replaced; keys of an existing model entry stay. */
function writeEntry(doc: ProjectDoc, s: Slot, make: (cur: Entry | null) => Entry): ProjectDoc {
  const cur = getLook(doc, s)
  const old = isModelEntry(cur) ? asEntry(cur as Look) : null
  const next = make(old)
  if (JSON.stringify(next) === JSON.stringify(old)) return doc
  // `setLook` would carry old per-side pictures into the entry; a model cannot have them, so clear the slot first.
  const cleared = cur && !old ? setLook(doc, s, undefined) : doc
  return withGraphics(cleared, (g) => {
    const m = (s.style === undefined ? g[s.kind] : g.styles?.[s.style]?.[s.kind]) as Record<string, Look | null> | undefined
    const map = { ...(m ?? {}), [s.id]: asLook(next) }
    if (s.style === undefined) return patchObj(g, { [s.kind]: map })
    return { ...g, styles: { ...(g.styles ?? {}), [s.style]: patchObj(g.styles?.[s.style] ?? {}, { [s.kind]: map }) } }
  })
}

/** Chooses the model file (a new entry, or another file for the existing one). */
export const setModelFile = (doc: ProjectDoc, s: Slot, file: string): ProjectDoc => writeEntry(doc, s, (cur) => ({ ...(cur ?? {}), model: file }))

/** Sets one key of the entry; undefined removes it. Does nothing without a model entry. */
function setKey(doc: ProjectDoc, s: Slot, key: string, value: unknown): ProjectDoc {
  if (!isModelEntry(getLook(doc, s))) return doc
  return writeEntry(doc, s, (cur) => {
    const next = { ...cur! }
    if (value === undefined) delete next[key]; else next[key] = value
    return next
  })
}

export const setModelHeight = (doc: ProjectDoc, s: Slot, v: number | undefined): ProjectDoc => setKey(doc, s, 'height', v)
/** 0 is the default, so it is not written. */
export const setModelTurn = (doc: ProjectDoc, s: Slot, v: number | undefined): ProjectDoc => setKey(doc, s, 'turn', v === 0 ? undefined : v)
/** "side" is the default and is not written; "" = no side colour. */
export const setSideMaterial = (doc: ProjectDoc, s: Slot, name: string): ProjectDoc => setKey(doc, s, 'sideMaterial', name === DEFAULT_SIDE_MATERIAL ? undefined : name)

/** The clip for a state; the default name is not written, "" = none. Other states' names stay. */
export function setAnimation(doc: ProjectDoc, s: Slot, state: AnimState, name: string): ProjectDoc {
  const cur = getLook(doc, s)
  if (!isModelEntry(cur)) return doc
  const anims = { ...((asEntry(cur as Look).animations as Record<string, unknown> | undefined) ?? {}) }
  if (name === DEFAULT_ANIM[state]) delete anims[state]; else anims[state] = name
  return setKey(doc, s, 'animations', Object.keys(anims).length ? anims : undefined)
}

/** What the fields show for an entry: the stored value, else the default. */
export interface ModelFields { height: number | undefined; turn: number | undefined; sideMaterial: string; animations: Record<AnimState, string> }
export function modelFields(entry: Look | null | undefined): ModelFields {
  const e = isModelEntry(entry) ? asEntry(entry as Look) : {}
  const a = (e.animations ?? {}) as Record<string, unknown>
  const text = (v: unknown, d: string) => (typeof v === 'string' ? v : d)
  return {
    height: typeof e.height === 'number' ? e.height : undefined,
    turn: typeof e.turn === 'number' ? e.turn : undefined,
    sideMaterial: text(e.sideMaterial, DEFAULT_SIDE_MATERIAL),
    animations: { idle: text(a.idle, DEFAULT_ANIM.idle), move: text(a.move, DEFAULT_ANIM.move), attack: text(a.attack, DEFAULT_ANIM.attack), die: text(a.die, DEFAULT_ANIM.die) },
  }
}
