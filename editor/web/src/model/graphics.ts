// Art in the document: the list of images ("sheets" in the file format) and the section setting.
// Looks (what each terrain/unit shows) are in graphicsLooks.ts; clean-up on delete in graphicsClean.ts.
import { makeId } from './ids'
import { patchObj, updateById } from './util'
import type { Graphics, Look, ProjectDoc, Sheet, StyleArt } from './types'

export const emptyGraphics = (): Graphics => ({ sheets: [] })

/** Applies `fn` to the graphics section; the first art edit creates it (`null` = "no art yet"). */
export function withGraphics(doc: ProjectDoc, fn: (g: Graphics) => Graphics): ProjectDoc {
  const g = doc.graphics ?? emptyGraphics()
  const next = fn(g)
  return next === doc.graphics ? doc : { ...doc, graphics: next }
}

export const sheetsOf = (doc: ProjectDoc): Sheet[] => doc.graphics?.sheets ?? []
export const findSheet = (doc: ProjectDoc, id: string): Sheet | undefined => sheetsOf(doc).find((s) => s.id === id)
/** The unit marker defaults to on when the file does not say. */
export const unitMarkerOn = (doc: ProjectDoc): boolean => doc.graphics?.unitMarker !== false

export const newSheetId = (doc: ProjectDoc, name: string): string => makeId(name, sheetsOf(doc).map((s) => s.id), { fallback: 'image' })

export interface NewSheet { file: string; name?: string; cellWidth: number; cellHeight: number }

export function addSheet(doc: ProjectDoc, s: NewSheet, id: string = newSheetId(doc, s.name || s.file)): ProjectDoc {
  const sheet: Sheet = { id, file: s.file, cellWidth: s.cellWidth, cellHeight: s.cellHeight }
  if (s.name) sheet.name = s.name
  return withGraphics(doc, (g) => ({ ...g, sheets: [...g.sheets, sheet] }))
}

export function updateSheet(doc: ProjectDoc, id: string, patch: Partial<Omit<Sheet, 'id'>>): ProjectDoc {
  if (!findSheet(doc, id)) return doc
  return withGraphics(doc, (g) => ({ ...g, sheets: updateById(g.sheets, id, (s) => patchObj(s, patch)) }))
}

/** Empty names leave the file entirely (the file name then serves as the name). */
export const setSheetName = (doc: ProjectDoc, id: string, name: string): ProjectDoc => updateSheet(doc, id, { name: name.trim() === '' ? undefined : name })

const whole = (n: number) => Math.max(1, Math.round(Number.isFinite(n) ? n : 1))
export function setSheetGrid(doc: ProjectDoc, id: string, cellWidth: number, cellHeight: number): ProjectDoc {
  return updateSheet(doc, id, { cellWidth: whole(cellWidth), cellHeight: whole(cellHeight) })
}

export function setUnitMarker(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withGraphics(doc, (g) => patchObj(g, { unitMarker: on ? undefined : false }))
}

/** True when `look` (or one of its per-side pictures) draws from the image. */
export function lookUsesSheet(look: Look | null | undefined, sheetId: string): boolean {
  if (!look) return false
  return look.sheet === sheetId || Object.values(look.sides ?? {}).some((l) => l.sheet === sheetId)
}

/** Keeps the entries of a looks map that do not draw from `sheetId`; a unit entry that does (its own picture) goes
 *  entirely, one that only has some per-side pictures from it loses just those. Explicit `null`s stay when asked. */
function sweepMap(m: Record<string, Look | null> | undefined, sheetId: string, keepNull: boolean): Record<string, Look | null> | undefined {
  if (!m) return m
  const out: Record<string, Look | null> = {}
  for (const [k, v] of Object.entries(m)) {
    if (v === null) { if (keepNull) out[k] = v; continue }
    if (v.sheet === sheetId) continue
    const kept = v.sides ? Object.fromEntries(Object.entries(v.sides).filter(([, l]) => l.sheet !== sheetId)) : undefined
    const lost = v.sides && Object.keys(kept!).length !== Object.keys(v.sides).length
    out[k] = lost ? patchObj(v, { sides: Object.keys(kept!).length ? kept : undefined }) : v
  }
  return Object.keys(out).length ? out : undefined
}

/** Removes the image and clears every picture that used it (all in one edit). */
export function removeSheet(doc: ProjectDoc, id: string): ProjectDoc {
  const g = doc.graphics
  if (!g || !g.sheets.some((s) => s.id === id)) return doc
  const styles: Record<string, StyleArt> = {}
  for (const [sid, st] of Object.entries(g.styles ?? {})) {
    const next = patchObj(st, { terrain: sweepMap(st.terrain, id, true), units: sweepMap(st.units, id, true), structures: sweepMap(st.structures, id, true) })
    if (Object.keys(next).length) styles[sid] = next
  }
  const next = patchObj({ ...g, sheets: g.sheets.filter((s) => s.id !== id) }, {
    terrain: sweepMap(g.terrain, id, false) as Graphics['terrain'],
    units: sweepMap(g.units, id, false) as Graphics['units'],
    structures: sweepMap(g.structures, id, false) as Graphics['structures'],
    styles: Object.keys(styles).length ? styles : undefined,
  })
  return { ...doc, graphics: next }
}
