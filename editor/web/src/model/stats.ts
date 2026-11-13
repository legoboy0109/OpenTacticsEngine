// Stats: the numbers every unit has (HP, Strength, ...).
import { withGame } from './doc'
import { repairFormulasForStat } from './formulas'
import { makeId, RESERVED_STAT_IDS } from './ids'
import { cleanStat } from './references'
import { moveItem, updateById } from './util'
import type { ProjectDoc } from './types'

export const newStatId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.game?.stats ?? []).map((s) => s.id), { fallback: 'stat', reserved: RESERVED_STAT_IDS })

/** Adding a stat needs no change on units: a missing value counts as 0. */
export function addStat(doc: ProjectDoc, name: string, id: string = newStatId(doc, name)): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, stats: [...g.stats, { id, name }] }))
}

export function renameStat(doc: ProjectDoc, id: string, name: string): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, stats: updateById(g.stats, id, (s) => ({ ...s, name })) }))
}

export function moveStat(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, stats: moveItem(g.stats, from, to) }))
}

export function setRole(doc: ProjectDoc, role: 'hp' | 'move', statId: string): ProjectDoc {
  return withGame(doc, (g) => (g.roles[role] === statId ? g : { ...g, roles: { ...g.roles, [role]: statId } }))
}

/** Why this stat cannot be deleted, or null if it can. */
export function statRemovalBlock(doc: ProjectDoc, id: string): string | null {
  const roles = doc.game?.roles
  if (roles?.hp === id) return 'This stat is the one used as Health. Choose a different stat for Health first.'
  if (roles?.move === id) return 'This stat is the one used as Movement. Choose a different stat for Movement first.'
  return null
}

/** Removes the stat everywhere: units, growths, per-unit overrides on maps,
 *  and any combat-maths preset that named it (those need attention afterwards). */
export function removeStat(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.game || statRemovalBlock(doc, id)) return doc
  const repaired = withGame(doc, (g) => repairFormulasForStat(g, id))
  const cleaned = cleanStat(repaired, id)
  return withGame(cleaned, (g) => ({ ...g, stats: g.stats.filter((s) => s.id !== id) }))
}
