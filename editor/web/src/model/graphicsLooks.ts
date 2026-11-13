// What each terrain and unit shows: reading, resolving and editing looks, and turning the picker's choice into one.
// A "slot" names where a look lives: a terrain or unit, optionally inside one visual style's overrides.
import { withGraphics } from './graphics'
import { DEFAULT_FRAME_MS, lookCells, lookShape } from './pickCell'
import { patchObj } from './util'
import type { CellRef, Graphics, Look, LookKind, ProjectDoc, StyleArt } from './types'

export interface Slot { kind: LookKind; id: string; style?: string }

type LookMap = Record<string, Look | null>
const mapOf = (g: Graphics | null, kind: LookKind, style?: string): LookMap | undefined =>
  (style === undefined ? g?.[kind] : g?.styles?.[style]?.[kind]) as LookMap | undefined

/** The entry stored in the slot: a look, `null` (a style says "no picture here") or undefined (no entry). */
export function getLook(doc: ProjectDoc, s: Slot): Look | null | undefined {
  const m = mapOf(doc.graphics, s.kind, s.style)
  return m && Object.prototype.hasOwnProperty.call(m, s.id) ? m[s.id] : undefined
}

/** What is actually drawn in `style` (undefined = the base): the style's entry if it has one, else the base entry. */
export function effectiveLook(g: Graphics | null, kind: LookKind, id: string, style?: string): Look | null {
  const own = mapOf(g, kind, style)
  if (style !== undefined && own && Object.prototype.hasOwnProperty.call(own, id)) return own[id]
  return mapOf(g, kind)?.[id] ?? null
}

function writeMap(g: Graphics, s: Slot, next: LookMap | undefined): Graphics {
  if (s.style === undefined) return patchObj(g, { [s.kind]: next && Object.keys(next).length ? next : undefined })
  const style: StyleArt = patchObj(g.styles?.[s.style] ?? {}, { [s.kind]: next && Object.keys(next).length ? next : undefined })
  const styles = { ...(g.styles ?? {}) }
  if (Object.keys(style).length) styles[s.style] = style; else delete styles[s.style]
  return patchObj(g, { styles: Object.keys(styles).length ? styles : undefined })
}

/** Sets what a slot shows.
 *  - base slot: a look sets it; `null`/undefined remove it (back to the placeholder).
 *  - style slot: a look overrides; `null` = "no picture in this look"; undefined = "same as the others".
 *  A unit keeps its per-side pictures when only its own picture changes. */
export function setLook(doc: ProjectDoc, s: Slot, look: Look | null | undefined): ProjectDoc {
  return writeLook(doc, s, look, true)
}

function writeLook(doc: ProjectDoc, s: Slot, look: Look | null | undefined, carrySides: boolean): ProjectDoc {
  const base = s.style === undefined
  const value = base && look === null ? undefined : look
  if (value === getLook(doc, s)) return doc
  if (value === undefined && getLook(doc, s) === undefined) return doc
  return withGraphics(doc, (g) => {
    const cur = { ...(mapOf(g, s.kind, s.style) ?? {}) }
    if (value === undefined) delete cur[s.id]
    else {
      const old = cur[s.id]
      cur[s.id] = carrySides && value && s.kind === 'units' && value.sides === undefined && old?.sides ? { ...value, sides: old.sides } : value
    }
    return writeMap(g, s, cur)
  })
}

/** A different picture for one side of a unit (inside the unit's entry in the base or in a style). */
export function setSideLook(doc: ProjectDoc, s: Slot, sideId: string, look: Look | undefined): ProjectDoc {
  const entry = getLook(doc, s)
  if (!entry) return doc
  const sides = { ...(entry.sides ?? {}) }
  if (look === undefined) delete sides[sideId]; else sides[sideId] = look
  const next = patchObj(entry, { sides: Object.keys(sides).length ? sides : undefined })
  return JSON.stringify(next) === JSON.stringify(entry) ? doc : writeLook(doc, s, next, false)
}

/** The unit's picture for a side (its own, else the unit's general one). */
export function sideLook(look: Look | null | undefined, sideId: string): Look | null {
  if (!look) return null
  return look.sides?.[sideId] ?? look
}

// ---- the picker's choice <-> a look ----

export type PickKind = 'one' | 'mix' | 'animation' | 'join'
export interface Pick {
  kind: PickKind
  sheet: string
  /** one: 1 piece; mix: 1+; animation: 1+ in order; join: the top-left piece of the 4 x 4 block. */
  cells: CellRef[]
  frameMs: number
  joinWith: string[]
  joinEdge: boolean
}

export const MAX_PIECES = 64
export const SPEEDS = { slow: 700, medium: 400, fast: 200 } as const
export const emptyPick = (sheet: string, kind: PickKind = 'one'): Pick => ({ kind, sheet, cells: [], frameMs: SPEEDS.medium, joinWith: [], joinEdge: true })

/** The look for a choice, or null while nothing is chosen. One piece in a list is stored as a plain `cell`. */
export function lookFromPick(p: Pick): Look | null {
  const cells = p.cells.slice(0, p.kind === 'one' || p.kind === 'join' ? 1 : MAX_PIECES)
  if (!cells.length) return null
  const cell = (c: CellRef): CellRef => [c[0], c[1]]
  switch (p.kind) {
    case 'one': return { sheet: p.sheet, cell: cell(cells[0]) }
    case 'mix': return cells.length === 1 ? { sheet: p.sheet, cell: cell(cells[0]) } : { sheet: p.sheet, variants: cells.map(cell) }
    case 'animation':
      return cells.length === 1 ? { sheet: p.sheet, cell: cell(cells[0]) } : { sheet: p.sheet, frames: cells.map(cell), frameMs: p.frameMs }
    case 'join': {
      const look: Look = { sheet: p.sheet, join: cell(cells[0]) }
      if (p.joinWith.length) look.joinWith = [...p.joinWith]
      if (!p.joinEdge) look.joinEdge = false
      return look
    }
  }
}

/** Starting point for the picker from an existing look (any of its keys the picker does not know are not carried). */
export function pickFromLook(look: Look | null | undefined, fallbackSheet: string): Pick {
  if (!look) return emptyPick(fallbackSheet)
  const shape = lookShape(look)
  const kind: PickKind = shape === 'join' ? 'join' : shape === 'variants' ? 'mix' : shape === 'animated' ? 'animation' : 'one'
  return {
    kind, sheet: look.sheet, cells: lookCells(look).map((c) => [c[0], c[1]] as CellRef),
    frameMs: look.frameMs ?? DEFAULT_FRAME_MS, joinWith: look.joinWith ? [...look.joinWith] : [], joinEdge: look.joinEdge !== false,
  }
}

/** Styles that have an override for this thing (used to tell people a look differs). */
export function stylesWithOverride(doc: ProjectDoc, kind: LookKind, id: string): string[] {
  return Object.entries(doc.graphics?.styles ?? {}).filter(([, st]) => st[kind] && Object.prototype.hasOwnProperty.call(st[kind], id)).map(([k]) => k)
}
