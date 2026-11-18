// The promotion sounds of the standard set (docs/SOUND.md "Promotion sounds"): `use.promotionSounds`, a list of `{ atMs, sound }` that play at those
// times after a promotion starts. Pure edits on the document; the checks mirror engine/player/sound/audio_data.cpp (readPromotionSounds).
import { soundsOf, withAudio } from './audio'
import type { ProjectDoc } from './types'

export interface PromotionSoundRow { atMs: number; sound: string }
export const MAX_PROMOTION_SOUNDS = 16
export const MAX_PROMOTION_MS = 60000

export function promotionSoundRows(doc: ProjectDoc): PromotionSoundRow[] {
  const list = doc.audio?.use?.promotionSounds
  return Array.isArray(list) ? list.map((e) => ({ atMs: typeof e?.atMs === 'number' ? e.atMs : 0, sound: typeof e?.sound === 'string' ? e.sound : '' })) : []
}

function write(doc: ProjectDoc, rows: PromotionSoundRow[]): ProjectDoc {
  return withAudio(doc, (a) => {
    const use = { ...(a.use ?? {}) }
    if (rows.length) use.promotionSounds = rows.map((r) => ({ atMs: r.atMs, sound: r.sound }))
    else delete use.promotionSounds
    if (Object.keys(use).length === 0) { const { use: _g, ...rest } = a; return rest }
    return { ...a, use }
  })
}

/** Adds a row after the last (400 ms later, or at 0) with the first sound of the game. */
export function addPromotionSound(doc: ProjectDoc): ProjectDoc {
  const rows = promotionSoundRows(doc)
  const first = Object.keys(soundsOf(doc))[0]
  if (rows.length >= MAX_PROMOTION_SOUNDS || !first) return doc
  const last = rows[rows.length - 1]
  return write(doc, [...rows, { atMs: last ? Math.min(MAX_PROMOTION_MS, last.atMs + 400) : 0, sound: first }])
}
export const patchPromotionSound = (doc: ProjectDoc, i: number, p: Partial<PromotionSoundRow>): ProjectDoc => write(doc, promotionSoundRows(doc).map((r, j) => (j === i ? { ...r, ...p } : r)))
export const removePromotionSound = (doc: ProjectDoc, i: number): ProjectDoc => write(doc, promotionSoundRows(doc).filter((_, j) => j !== i))

export function promotionSoundProblems(doc: ProjectDoc): string[] {
  const list = doc.audio?.use?.promotionSounds
  if (list === undefined) return []
  if (!Array.isArray(list) || list.length === 0 || list.length > MAX_PROMOTION_SOUNDS) return [`The promotion sounds must be a list of 1 to ${MAX_PROMOTION_SOUNDS}.`]
  const out: string[] = []
  promotionSoundRows(doc).forEach((r, i) => {
    if (!Number.isInteger(r.atMs) || r.atMs < 0 || r.atMs > MAX_PROMOTION_MS) out.push(`Promotion sound ${i + 1}: the time is a whole number of milliseconds from 0 to ${MAX_PROMOTION_MS}.`)
    if (!(r.sound in soundsOf(doc))) out.push(`Promotion sound ${i + 1}: ${r.sound ? `“${r.sound}”` : 'a sound'} is not a sound of this game.`)
  })
  return out
}
