// Dialogue portraits: data/graphics.json "portraits": { "<id>": { "sheet": "<image id>", "cell": [x, y] } }.
// The id is what a `say` command's "portrait" names; suggestions are the story names of the game's units.
import { withGraphics, sheetsOf } from './graphics'
import { isStoryId } from './storyIds'
import { patchObj } from './util'
import type { CellRef, Portrait, ProjectDoc } from './types'

export const portraitList = (doc: ProjectDoc): Array<{ id: string; portrait: Portrait }> =>
  Object.entries(doc.graphics?.portraits ?? {}).map(([id, portrait]) => ({ id, portrait }))

const setPortraits = (doc: ProjectDoc, map: Record<string, Portrait>): ProjectDoc =>
  withGraphics(doc, (g) => patchObj(g, { portraits: Object.keys(map).length ? map : undefined }))

/** Why `id` cannot be a portrait id (null when it can). `except` = the portrait being renamed. */
export function portraitIdProblem(doc: ProjectDoc, id: string, except?: string): string | null {
  if (id === '') return 'Type a name.'
  if (!isStoryId(id)) return 'Use letters, digits, _ and - only (no spaces).'
  if (id !== except && doc.graphics?.portraits && id in doc.graphics.portraits) return 'There is already a portrait with this name.'
  return null
}

/** A new portrait starts on the first image's first piece (the maker then clicks the piece they want). */
export function addPortrait(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet || portraitIdProblem(doc, id)) return doc
  return setPortraits(doc, { ...(doc.graphics?.portraits ?? {}), [id]: { sheet, cell: [0, 0] } })
}

export function setPortraitPicture(doc: ProjectDoc, id: string, sheet: string, cell: CellRef): ProjectDoc {
  const cur = doc.graphics?.portraits?.[id]
  if (!cur) return doc
  return setPortraits(doc, { ...doc.graphics!.portraits!, [id]: patchObj(cur, { sheet, cell }) })
}

/** Renames in place (the order of the list is kept). */
export function renamePortrait(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const all = doc.graphics?.portraits
  if (!all || !(from in all) || from === to || portraitIdProblem(doc, to, from)) return doc
  return setPortraits(doc, Object.fromEntries(Object.entries(all).map(([k, v]) => [k === from ? to : k, v])))
}

export function movePortrait(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  const entries = Object.entries(doc.graphics?.portraits ?? {})
  if (from === to || from < 0 || to < 0 || from >= entries.length || to >= entries.length) return doc
  const [it] = entries.splice(from, 1)
  entries.splice(to, 0, it)
  return setPortraits(doc, Object.fromEntries(entries))
}

export function removePortrait(doc: ProjectDoc, id: string): ProjectDoc {
  const all = doc.graphics?.portraits
  if (!all || !(id in all)) return doc
  const { [id]: _gone, ...rest } = all
  return setPortraits(doc, rest)
}

/** Portraits that draw from the image (they go when the image is removed). */
export const portraitsUsingSheet = (doc: ProjectDoc, sheetId: string): string[] =>
  portraitList(doc).filter((p) => p.portrait.sheet === sheetId).map((p) => p.id)

/** Story names of units in the game's maps that have no portrait yet (with a display name), in map order. */
export function portraitSuggestions(doc: ProjectDoc): Array<{ id: string; label: string }> {
  const have = new Set(Object.keys(doc.graphics?.portraits ?? {}))
  const out: Array<{ id: string; label: string }> = []
  for (const m of Object.values(doc.maps)) {
    for (const u of m?.units ?? []) {
      if (!u.id || have.has(u.id) || out.some((o) => o.id === u.id)) continue
      out.push({ id: u.id, label: u.name?.trim() || doc.units?.find((t) => t.id === u.type)?.name || u.id })
    }
  }
  return out
}

// --- which way a face looks, and its talk / blink frames (docs/GRAPHICS.md "Portraits") ---

export type PortraitFacing = 'left' | 'right'
export const MAX_PORTRAIT_FRAMES = 8
/** A portrait's `expressions` (the faces a line's `face` picks): up to 15 (docs/EVENTS.md "Portrait expressions"). */
export const MAX_PORTRAIT_EXPRESSIONS = 15

/** Which way the picture looks, so the stage can turn it towards the middle of the screen; undefined = not said (drawn as it is). */
export function setPortraitFacing(doc: ProjectDoc, id: string, facing: PortraitFacing | undefined): ProjectDoc {
  const cur = doc.graphics?.portraits?.[id]
  if (!cur || cur.faces === facing) return doc
  return setPortraits(doc, { ...doc.graphics!.portraits!, [id]: patchObj(cur, { faces: facing }) })
}

/** The "talk", "blink" or "expressions" cells; an empty list takes the key away. */
export function setPortraitFrames(doc: ProjectDoc, id: string, key: 'talk' | 'blink' | 'expressions', cells: CellRef[]): ProjectDoc {
  const cur = doc.graphics?.portraits?.[id]
  if (!cur) return doc
  return setPortraits(doc, { ...doc.graphics!.portraits!, [id]: patchObj(cur, { [key]: cells.length ? cells : undefined }) })
}

/** "2,1  3,1" (column,row counted from 1, as the Row boxes do) -> cells, or a sentence when it cannot be read. */
export function parseFrames(text: string, most = MAX_PORTRAIT_FRAMES): { cells: CellRef[] } | { error: string } {
  const parts = text.trim() === '' ? [] : text.trim().split(/[\s;]+/)
  const cells: CellRef[] = []
  for (const p of parts) {
    const m = /^(\d+),(\d+)$/.exec(p)
    if (!m || Number(m[1]) < 1 || Number(m[2]) < 1) return { error: `“${p}” is not a piece: write the column and the row counted from 1, like 2,1.` }
    cells.push([Number(m[1]) - 1, Number(m[2]) - 1])
  }
  if (cells.length > most) return { error: `A face can have at most ${most} ${most === MAX_PORTRAIT_FRAMES ? 'frames of each kind' : 'expressions'}.` }
  return { cells }
}

export const framesText = (cells: CellRef[] | undefined): string => (cells ?? []).map(([c, r]) => `${c + 1},${r + 1}`).join('  ')
