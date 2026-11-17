// "computerCaps" of data/caps.json (docs/SRPG.md section 11): the game-wide cap of computer-controlled units, per stat. Same shape
// as "caps"; an empty list is not written.
import { MAX_CAP, setCapIn } from './ranks'
import type { ProjectDoc } from './types'

export const computerCap = (doc: ProjectDoc, stat: string): number | undefined => {
  const map = doc.caps?.computerCaps
  const v = map && typeof map === 'object' ? (map as Record<string, number>)[stat] : undefined
  return typeof v === 'number' ? v : undefined
}

export function setComputerCap(doc: ProjectDoc, stat: string, value: number | undefined): ProjectDoc {
  if (!doc.caps) return doc
  const next = setCapIn(doc.caps.computerCaps as Record<string, number> | undefined, stat, value)
  const caps = { ...doc.caps }
  if (next) caps.computerCaps = next
  else delete caps.computerCaps
  return { ...doc, caps }
}

/** Checks mirroring the loader: a computer cap names a stat of the game and is a whole number from 1 to MAX_CAP. */
export function computerCapProblems(doc: ProjectDoc): string[] {
  const map = doc.caps?.computerCaps
  if (map === undefined) return []
  if (!map || typeof map !== 'object' || Array.isArray(map)) return ['The computer caps must be a list of stats with numbers.']
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const out: string[] = []
  for (const [stat, v] of Object.entries(map as Record<string, unknown>)) {
    if (!stats.has(stat)) out.push(`The computer caps name "${stat}", which is not a stat of this game.`)
    else if (typeof v !== 'number' || !Number.isInteger(v) || v < 1 || v > MAX_CAP) out.push(`The computer cap of "${stat}" must be a whole number from 1 to ${MAX_CAP}.`)
  }
  return out
}
