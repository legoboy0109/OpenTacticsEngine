// Art in the document: the list of images ("sheets" in the file format) and the section setting.
// Looks (what each terrain/unit shows) are in graphicsLooks.ts; clean-up on delete in graphicsClean.ts.
import { dropBattleSheet } from './battleArt'
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

/** "Smooth (for detailed art)": the 3D views blend this image's pixels instead of drawing them hard. Off leaves the key out. */
export const setSheetSmooth = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateSheet(doc, id, { smooth: on ? true : undefined })

export function setUnitMarker(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withGraphics(doc, (g) => patchObj(g, { unitMarker: on ? undefined : false }))
}

/** True when `look` (or one of its per-side pictures) draws from the image. */
export function lookUsesSheet(look: Look | null | undefined, sheetId: string): boolean {
  if (!look) return false
  return look.sheet === sheetId || look.walk?.sheet === sheetId || Object.values(look.sides ?? {}).some((l) => l.sheet === sheetId || l.walk?.sheet === sheetId) || look.stand?.sheet === sheetId
}

/** Keeps the entries of a looks map that do not draw from `sheetId`; a unit entry that does (its own picture) goes
 *  entirely, one that only has some per-side pictures from it loses just those. Explicit `null`s stay when asked. */
function sweepMap(m: Record<string, Look | null> | undefined, sheetId: string, keepNull: boolean): Record<string, Look | null> | undefined {
  if (!m) return m
  const out: Record<string, Look | null> = {}
  for (const [k, v] of Object.entries(m)) {
    if (v === null) { if (keepNull) out[k] = v; continue }
    if (v.sheet === sheetId) continue
    // A standing picture drawn from the image goes (the ground picture stays); a standing model stays.
    if (v.stand?.sheet === sheetId) { out[k] = patchObj(v, { stand: undefined }); continue }
    // Walking frames drawn from the image go (the standing picture stays), also those of a side's own picture.
    const entry = v.walk?.sheet === sheetId ? patchObj(v, { walk: undefined }) : v
    const sides = entry.sides
    const kept = sides ? Object.fromEntries(Object.entries(sides).filter(([, l]) => l.sheet !== sheetId).map(([s, l]) => [s, l.walk?.sheet === sheetId ? patchObj(l, { walk: undefined }) : l])) : undefined
    const lost = sides && Object.keys(kept!).length !== Object.keys(sides).length
    const walkLost = sides && Object.entries(kept!).some(([s, l]) => l !== sides[s])
    out[k] = lost || walkLost ? patchObj(entry, { sides: Object.keys(kept!).length ? kept : undefined }) : entry
  }
  return Object.keys(out).length ? out : undefined
}

function keepBackgrounds(all: Graphics['backgrounds'], sheetId: string): Graphics['backgrounds'] {
  if (!all) return all
  const kept = Object.fromEntries(Object.entries(all).filter(([, b]) => b.sheet !== sheetId))
  return Object.keys(kept).length ? kept : undefined
}

function keepPortraits(all: NonNullable<Graphics['portraits']>, sheetId: string): Graphics['portraits'] {
  const kept = Object.fromEntries(Object.entries(all).filter(([, p]) => p.sheet !== sheetId))
  return Object.keys(kept).length ? kept : undefined
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
    // Dialogue portraits drawn from the image go with it.
    portraits: g.portraits && keepPortraits(g.portraits, id),
    // ... and so do the story scene backgrounds
    backgrounds: keepBackgrounds(g.backgrounds, id),
  })
  return { ...doc, graphics: dropBattleSheet(next, id) }  // battle sets, effects and backgrounds that draw from the image go too
}
