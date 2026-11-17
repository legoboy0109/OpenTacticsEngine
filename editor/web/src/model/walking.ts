// Walking frames of a unit picture (docs/GRAPHICS.md "Walking"): the form of the "Walking" card (an image, rows per direction, a number of
// frames, a time per frame) and the checks that mirror the game's loader (engine/core/data/loader_graphics.cpp readWalk). Pure functions.
import { getLook, setLookExact, setSideLook, type Slot } from './graphicsLooks'
import { findSheet } from './graphics'
import { patchObj } from './util'
import type { CellRef, Look, ProjectDoc, WalkDef } from './types'

export const WALK_MAX_FRAMES = 64
export const DEFAULT_WALK_MS = 150
/** The directions of the form, in the order the card lists and the preview cycles through (the game's down, up, right, left). */
export const WALK_DIRS = ['down', 'up', 'side', 'left', 'right'] as const
export type WalkDir = (typeof WALK_DIRS)[number]
/** What the preview shows one after the other (a direction the walk does not have is left out). */
export const PREVIEW_ORDER = ['down', 'right', 'up', 'left'] as const
export type ShownDir = (typeof PREVIEW_ORDER)[number]

/** The cells of one row of an image, from the left: [0, row] .. [count - 1, row]. */
export const runOfRow = (row: number, count: number): CellRef[] =>
  Array.from({ length: Math.max(1, Math.min(WALK_MAX_FRAMES, Math.round(count))) }, (_, i) => [i, Math.max(0, Math.round(row))] as CellRef)

/** The row of a list that is one run along a row from the left (what the Row boxes can say), else undefined. */
export function rowOfRun(cells: CellRef[] | undefined): number | undefined {
  if (!cells || cells.length === 0) return undefined
  return cells.every(([c, r], i) => c === i && r === cells[0][1]) ? cells[0][1] : undefined
}

export const walkOf = (look: Look | null | undefined): WalkDef | undefined => look?.walk

/** True when `left` / `right` are the form's own rows (instead of one `side` row that the game turns round). */
export const hasOwnSides = (w: WalkDef): boolean => !!(w.left || w.right) && !w.side

/** How many frames the walk has per direction (the longest list; the card edits them all together). */
export const frameCount = (w: WalkDef): number => Math.max(1, ...WALK_DIRS.map((d) => w[d]?.length ?? 0))

/** The directions that are not plain rows from the left (written by hand): the card says so and what changing replaces. */
export const handWritten = (w: WalkDef): WalkDir[] => WALK_DIRS.filter((d) => w[d] && rowOfRun(w[d]) === undefined)

/** The shown directions the walk offers and how each is drawn: `mirror` = the other side's frames turned round. */
export function shownDirs(w: WalkDef): Array<{ dir: ShownDir; cells: CellRef[]; mirror: boolean }> {
  const right = w.right ?? w.side ?? (w.left ? w.left : undefined)
  const rightMirror = !w.right && !w.side && !!w.left
  const left = w.left ?? right
  const leftMirror = !w.left && !!right
  const out: Array<{ dir: ShownDir; cells: CellRef[]; mirror: boolean }> = []
  if (w.down?.length) out.push({ dir: 'down', cells: w.down, mirror: false })
  if (right?.length) out.push({ dir: 'right', cells: right, mirror: rightMirror })
  if (w.up?.length) out.push({ dir: 'up', cells: w.up, mirror: false })
  if (left?.length) out.push({ dir: 'left', cells: left, mirror: leftMirror })
  return out
}

// --- editing ------------------------------------------------------------------------------------------------------------------------

/** Where a walk lives: the slot's picture, or one of its per-side pictures. */
export interface WalkTarget { slot: Slot; side?: string }

/** The picture the target names (the slot's own entry or its side's). */
export function lookOf(doc: ProjectDoc, t: WalkTarget): Look | undefined {
  const entry = getLook(doc, t.slot)
  if (!entry) return undefined
  return t.side === undefined ? entry : entry.sides?.[t.side]
}

/** Applies `fn` to the walk of the target (undefined result removes it). */
export function editWalk(doc: ProjectDoc, t: WalkTarget, fn: (w: WalkDef | undefined, look: Look) => WalkDef | undefined): ProjectDoc {
  const look = lookOf(doc, t)
  if (!look) return doc
  const next = fn(look.walk, look)
  const updated = patchObj(look, { walk: next && Object.keys(next).length ? next : undefined })
  if (JSON.stringify(updated) === JSON.stringify(look)) return doc
  if (t.side !== undefined) return setSideLook(doc, t.slot, t.side, updated)
  return setLookExact(doc, t.slot, updated)
}

/** A first walk: four frames of the first row facing down (the picture's own image unless another is given). */
export function addWalk(doc: ProjectDoc, t: WalkTarget, cols?: number): ProjectDoc {
  return editWalk(doc, t, (w) => w ?? { down: runOfRow(0, Math.min(4, cols ?? 4)) })
}
export const removeWalk = (doc: ProjectDoc, t: WalkTarget): ProjectDoc => editWalk(doc, t, () => undefined)

const redo = (w: WalkDef, count: number): WalkDef => {
  const out: WalkDef = { ...w }
  for (const d of WALK_DIRS) if (w[d]?.length) out[d] = runOfRow(w[d]![0][1], count)
  return out
}

/** The image the walking frames come from ('' = the picture's own image). */
export const setWalkSheet = (doc: ProjectDoc, t: WalkTarget, sheet: string): ProjectDoc =>
  editWalk(doc, t, (w, look) => (w ? patchObj(w, { sheet: sheet === '' || sheet === look.sheet ? undefined : sheet }) : w))

/** One direction's row (1 = the top row is 0 here: pass the zero-based row); undefined takes the direction out. The count is the walk's. */
export function setWalkRow(doc: ProjectDoc, t: WalkTarget, dir: WalkDir, row: number | undefined): ProjectDoc {
  return editWalk(doc, t, (w) => {
    if (!w) return w
    const n = frameCount(w)
    return patchObj(w, { [dir]: row === undefined ? undefined : runOfRow(row, n) })
  })
}

/** How many frames every direction has: each keeps its row and becomes a run of this many cells. */
export const setWalkCount = (doc: ProjectDoc, t: WalkTarget, count: number): ProjectDoc =>
  editWalk(doc, t, (w) => (w ? redo(w, count) : w))

export const setWalkMs = (doc: ProjectDoc, t: WalkTarget, ms: number | undefined): ProjectDoc =>
  editWalk(doc, t, (w) => (w ? patchObj(w, { frameMs: ms === undefined || ms === DEFAULT_WALK_MS ? undefined : ms }) : w))

/** "Left and right have rows of their own": on = the side row becomes the right row and the left starts as the same row; off = the right
 *  row (else the left) becomes the side row again and the game turns it round. */
export function setOwnSides(doc: ProjectDoc, t: WalkTarget, on: boolean): ProjectDoc {
  return editWalk(doc, t, (w) => {
    if (!w) return w
    if (on) {
      const row = w.side ?? w.right ?? w.left
      return row ? patchObj(w, { side: undefined, right: row, left: w.left ?? row }) : w
    }
    const row = w.side ?? w.right ?? w.left
    return patchObj(w, { left: undefined, right: undefined, side: row })
  })
}

// --- checks (the game refuses the file for these) -------------------------------------------------------------------------------------------

export type Dims = (file: string) => { width: number; height: number } | undefined

/** What the game's loader would refuse about this walk (the same rules as readWalk), in plain words. */
export function walkProblems(doc: ProjectDoc, look: Look, dims?: Dims): string[] {
  const w = look.walk
  if (!w) return []
  const out: string[] = []
  const sheet = findSheet(doc, w.sheet ?? look.sheet)
  if (!sheet) return ['The walking frames use an image that is no longer in the game.']
  if (!WALK_DIRS.some((d) => w[d]?.length)) out.push('Give at least one direction a row of frames.')
  if (w.frameMs !== undefined && (w.frameMs < 20 || w.frameMs > 60000)) out.push('The time per frame must be between 20 and 60000 milliseconds.')
  const d = dims?.(sheet.file)
  if (d) {
    const cols = Math.floor(d.width / sheet.cellWidth), rows = Math.floor(d.height / sheet.cellHeight)
    const bad = WALK_DIRS.filter((dir) => w[dir]?.some(([c, r]) => c >= cols || r >= rows))
    if (bad.length) out.push(`Some frames are outside the image (it has ${cols} × ${rows} pieces). Lower the number of frames or the row.`)
  }
  for (const dir of WALK_DIRS) if ((w[dir]?.length ?? 0) > WALK_MAX_FRAMES) out.push(`A direction can have at most ${WALK_MAX_FRAMES} frames.`)
  return out
}
