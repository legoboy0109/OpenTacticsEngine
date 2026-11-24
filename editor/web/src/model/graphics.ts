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

/** A sheet's unit offset (docs/GRAPHICS.md "Unit offset"): whole numbers -256..256; [0, 0] is the default so the key goes. */
export function setSheetUnitOffset(doc: ProjectDoc, id: string, x: number, y: number): ProjectDoc {
  const fit = (n: number) => Math.max(-256, Math.min(256, Math.round(Number.isFinite(n) ? n : 0)))
  return updateSheet(doc, id, { unitOffset: fit(x) === 0 && fit(y) === 0 ? undefined : [fit(x), fit(y)] })
}

/** The portrait plate of the dialogue box (docs/GRAPHICS.md "Portraits"): a number (square) or [width, height], 24..160 each.
 *  Square sizes are stored as one number; undefined takes the key away (the box's own 48). */
export function setPortraitSize(doc: ProjectDoc, w: number | undefined, h: number | undefined): ProjectDoc {
  const fit = (n: number) => Math.max(24, Math.min(160, Math.round(n)))
  return withGraphics(doc, (g) => patchObj(g, { portraitSize: w === undefined || h === undefined ? undefined : fit(w) === fit(h) ? fit(w) : [fit(w), fit(h)] }))
}

/** The portrait plate as [width, height] (undefined = not set); anything unreadable is also undefined (the checks say so). */
export function portraitSizeOf(doc: ProjectDoc): [number, number] | undefined {
  const p = doc.graphics?.portraitSize as unknown
  if (typeof p === 'number') return [p, p]
  return Array.isArray(p) && p.length === 2 && typeof p[0] === 'number' && typeof p[1] === 'number' ? [p[0], p[1]] : undefined
}

export function setUnitMarker(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withGraphics(doc, (g) => patchObj(g, { unitMarker: on ? undefined : false }))
}

const usesFramesOf = (look: Look, sheetId: string): boolean => FRAME_KEYS.some((k) => look[k]?.sheet === sheetId)

/** True when `look` (or one of its per-side pictures) draws from the image. */
export function lookUsesSheet(look: Look | null | undefined, sheetId: string): boolean {
  if (!look) return false
  return look.sheet === sheetId || usesFramesOf(look, sheetId) || Object.values(look.sides ?? {}).some((l) => l.sheet === sheetId || usesFramesOf(l, sheetId)) || look.stand?.sheet === sheetId
}

/** Keeps the entries of a looks map that do not draw from `sheetId`; a unit entry that does (its own picture) goes
 *  entirely, one that only has some per-side pictures from it loses just those. Explicit `null`s stay when asked. */
/** The keys of a unit picture that hold frames of their own (walking and the poses; docs/GRAPHICS.md "Poses and facing"). */
export const FRAME_KEYS = ['walk', 'facing', 'attack', 'hurt', 'ko'] as const

/** The picture without the walking frames and poses that draw from the image (the same object when none does). */
function withoutFramesOf(look: Look, sheetId: string): Look {
  const gone = FRAME_KEYS.filter((k) => look[k]?.sheet === sheetId)
  return gone.length ? patchObj(look, Object.fromEntries(gone.map((k) => [k, undefined]))) : look
}

function sweepMap(m: Record<string, Look | null> | undefined, sheetId: string, keepNull: boolean): Record<string, Look | null> | undefined {
  if (!m) return m
  const out: Record<string, Look | null> = {}
  for (const [k, v] of Object.entries(m)) {
    if (v === null) { if (keepNull) out[k] = v; continue }
    if (v.sheet === sheetId) continue
    // A standing picture drawn from the image goes (the ground picture stays); a standing model stays.
    if (v.stand?.sheet === sheetId) { out[k] = patchObj(v, { stand: undefined }); continue }
    // Walking frames and poses drawn from the image go (the standing picture stays), also those of a side's own picture.
    const entry = withoutFramesOf(v, sheetId)
    const sides = entry.sides
    const kept = sides ? Object.fromEntries(Object.entries(sides).filter(([, l]) => l.sheet !== sheetId).map(([s, l]) => [s, withoutFramesOf(l, sheetId)])) : undefined
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

function keepBadges(all: Graphics['badges'], sheetId: string): Graphics['badges'] {
  if (!all) return all
  const kept = all.filter((b) => b.sheet !== sheetId)
  return kept.length ? kept : undefined
}

function keepMarkers(all: Graphics['markers'], sheetId: string): Graphics['markers'] {
  if (!all) return all
  const kept = all.filter((m) => m.sheet !== sheetId)
  return kept.length ? kept : undefined
}

function keepPortraits(all: NonNullable<Graphics['portraits']>, sheetId: string): Graphics['portraits'] {
  const kept = Object.fromEntries(Object.entries(all).filter(([, p]) => p.sheet !== sheetId))
  return Object.keys(kept).length ? kept : undefined
}

function keepIcons(all: NonNullable<Graphics['icons']>, sheetId: string): Graphics['icons'] {
  const kept = Object.fromEntries(Object.entries(all).filter(([, p]) => p.sheet !== sheetId))
  return Object.keys(kept).length ? kept : undefined
}

function dropBackdropSheet(maps: ProjectDoc['maps'], sheetId: string): ProjectDoc['maps'] {
  let out = maps
  for (const [mid, m] of Object.entries(maps)) {
    const b = m?.backdrop
    if (!m || !b || (b.sheet !== sheetId && b.over !== sheetId)) continue
    const next = b.sheet === sheetId ? undefined : patchObj(b, { over: undefined, pieces: undefined })
    out = { ...out, [mid]: patchObj(m, { backdrop: next }) }
  }
  return out
}

/** Removes the image and clears every picture that used it (all in one edit). */
export function removeSheet(doc: ProjectDoc, id: string): ProjectDoc {
  const g = doc.graphics
  if (!g || !g.sheets.some((s) => s.id === id)) return doc
  const styles: Record<string, StyleArt> = {}
  for (const [sid, st] of Object.entries(g.styles ?? {})) {
    const next = patchObj(st, { terrain: sweepMap(st.terrain, id, true), units: sweepMap(st.units, id, true), characters: sweepMap(st.characters, id, true), structures: sweepMap(st.structures, id, true) })
    if (Object.keys(next).length) styles[sid] = next
  }
  const next = patchObj({ ...g, sheets: g.sheets.filter((s) => s.id !== id) }, {
    terrain: sweepMap(g.terrain, id, false) as Graphics['terrain'],
    units: sweepMap(g.units, id, false) as Graphics['units'],
    characters: sweepMap(g.characters, id, false) as Graphics['characters'],
    structures: sweepMap(g.structures, id, false) as Graphics['structures'],
    styles: Object.keys(styles).length ? styles : undefined,
    // Dialogue portraits drawn from the image go with it.
    portraits: g.portraits && keepPortraits(g.portraits, id),
    // ... and the item icons
    icons: g.icons && keepIcons(g.icons, id),
    // ... and so do the story scene backgrounds
    backgrounds: keepBackgrounds(g.backgrounds, id),
    // ... and the unit badges
    badges: keepBadges(g.badges, id),
    markers: keepMarkers(g.markers, id),
  })
  // A map's backdrop drawn from the image goes (its over picture alone: the pieces cut from it go with it).
  const maps = doc.maps ? dropBackdropSheet(doc.maps, id) : doc.maps
  return { ...doc, ...(maps === doc.maps ? {} : { maps }), graphics: dropBattleSheet(next, id) }  // battle sets, effects and backgrounds that draw from the image go too
}
