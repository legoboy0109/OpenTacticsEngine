// Technologies (data/research.json, docs/ECONOMY.md "Research"): the list and each technology's settings. What other
// parts hold about technologies, and the clean-up when something is deleted, is researchRefs.ts.
import { clone, ids, withResearch } from './doc'
import { makeId } from './ids'
import { reconcileEconomy } from './researchRefs'
import { moveItem, patchObj, setNum, updateById } from './util'
import type { ProjectDoc, Technology } from './types'

export const TURNS_MAX = 99
/** `turns` when the key is missing (docs/ECONOMY.md). */
export const TURNS_DEFAULT = 1

export const newTechId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.research), { fallback: 'technology' })

/** A new technology is free and takes one turn, so the file is valid straight away. */
export function addTechnology(doc: ProjectDoc, name: string, id: string = newTechId(doc, name)): ProjectDoc {
  return withResearch(doc, (list) => [...list, { id, name }])
}

export function duplicateTechnology(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withResearch(doc, (list) => {
    const i = list.findIndex((t) => t.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    const copy: Technology = { ...clone(list[i]), id: newId ?? makeId(name, ids(list), { fallback: 'technology' }), name }
    return [...list.slice(0, i + 1), copy, ...list.slice(i + 1)]
  })
}

/** Removes the technology and everything that named it: other technologies' "needs first", upgrades that required it,
 *  and story events that check for or give it (all in this one edit; see researchRefs.ts). */
export function deleteTechnology(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.research?.some((t) => t.id === id)) return doc
  return reconcileEconomy(doc, { ...doc, research: doc.research.filter((t) => t.id !== id) })
}

export function updateTechnology(doc: ProjectDoc, id: string, patch: Partial<Omit<Technology, 'id'>>): ProjectDoc {
  return withResearch(doc, (list) => updateById(list, id, (t) => patchObj(t, patch)))
}

export function moveTechnology(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withResearch(doc, (list) => moveItem(list, from, to))
}

/** An empty description removes the key. */
export const setTechDescription = (doc: ProjectDoc, id: string, text: string): ProjectDoc =>
  updateTechnology(doc, id, { description: text === '' ? undefined : text })

/** Price in one resource; undefined removes it, and an empty price leaves the key out (= free). */
export function setTechCost(doc: ProjectDoc, id: string, resource: string, value: number | undefined): ProjectDoc {
  return withResearch(doc, (list) => updateById(list, id, (t) => patchObj(t, { cost: setNum(t.cost, resource, value) })))
}

/** Whole turns 0 to 99; 1 is the default and is not written, 0 means finished at once. */
export function setTechTurns(doc: ProjectDoc, id: string, turns: number): ProjectDoc {
  const n = Math.min(TURNS_MAX, Math.max(0, Math.round(turns)))
  return updateTechnology(doc, id, { turns: n === TURNS_DEFAULT ? undefined : n })
}
export const turnsOf = (t: Technology): number => (typeof t.turns === 'number' ? t.turns : TURNS_DEFAULT)

/** Technologies that must be finished first; empty removes the key. */
export const setTechRequires = (doc: ProjectDoc, id: string, list: string[]): ProjectDoc =>
  updateTechnology(doc, id, { requires: list.length ? list : undefined })

/** Unit types that cannot be built without it. The block holds nothing else, so an empty list removes it. */
export function setTechUnlocks(doc: ProjectDoc, id: string, units: string[]): ProjectDoc {
  return withResearch(doc, (list) => updateById(list, id, (t) => {
    const rest = { ...(t.unlocks ?? {}) }
    if (units.length) rest.units = units; else delete rest.units
    return patchObj(t, { unlocks: Object.keys(rest).length ? rest : undefined })
  }))
}

// --- bonus: which unit types (none listed = all) and a number per stat ---

/** Does the technology change stats at all? (a block with every number 0 or no number is refused by the loader) */
export const hasBonus = (t: Technology): boolean => !!t.bonus

/** Switching a bonus on starts as +1 of the first stat that is not the health or movement role, so it is valid at once. */
export function setBonusOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  if (!on) return updateTechnology(doc, id, { bonus: undefined })
  const g = doc.game
  const stat = g?.stats.find((s) => s.id !== g.roles.hp && s.id !== g.roles.move) ?? g?.stats[0]
  return updateTechnology(doc, id, { bonus: stat ? { stats: { [stat.id]: 1 } } : { stats: {} } })
}

/** Unit types the bonus is for; an empty list removes the key (= all unit types). */
export function setBonusTypes(doc: ProjectDoc, id: string, types: string[]): ProjectDoc {
  return withResearch(doc, (list) => updateById(list, id, (t) => {
    if (!t.bonus) return t
    const bonus = { ...t.bonus }
    if (types.length) bonus.types = types; else delete bonus.types
    return { ...t, bonus }
  }))
}

/** One stat's amount (positive or negative); undefined or 0 removes the row (a stat of 0 changes nothing). */
export function setBonusStat(doc: ProjectDoc, id: string, stat: string, value: number | undefined): ProjectDoc {
  return withResearch(doc, (list) => updateById(list, id, (t) => {
    if (!t.bonus) return t
    return { ...t, bonus: { ...t.bonus, stats: setNum(t.bonus.stats, stat, value === 0 ? undefined : value) ?? {} } }
  }))
}

// --- prerequisites: who may be picked without making a loop ---

/** Technologies that (directly or through others) need `from`. */
function dependents(list: Technology[], from: string): Set<string> {
  const out = new Set<string>()
  const queue = [from]
  while (queue.length) {
    const cur = queue.pop()!
    for (const t of list) if (t.requires?.includes(cur) && !out.has(t.id)) { out.add(t.id); queue.push(t.id) }
  }
  return out
}

/** Why `candidate` cannot be a prerequisite of `tech` (null when it can): itself, or one that already needs `tech`. */
export function needsFirstBlock(doc: ProjectDoc, tech: string, candidate: string): string | null {
  const list = doc.research ?? []
  if (candidate === tech) return 'a technology cannot need itself'
  if (dependents(list, tech).has(candidate)) return 'it needs this one already, so this would make a loop'
  return null
}

/** The technologies in a prerequisite loop, shown as "a → b → a" (null when there is none). Mirrors the loader. */
export function findLoop(list: Technology[]): string[] | null {
  const byId = new Map(list.map((t) => [t.id, t]))
  const state = new Map<string, 1 | 2>()
  const path: string[] = []
  const visit = (id: string): string[] | null => {
    state.set(id, 1)
    path.push(id)
    for (const next of byId.get(id)?.requires ?? []) {
      if (!byId.has(next)) continue
      if (state.get(next) === 1) return [...path.slice(path.indexOf(next)), next]
      if (!state.has(next)) { const loop = visit(next); if (loop) return loop }
    }
    path.pop()
    state.set(id, 2)
    return null
  }
  for (const t of list) if (!state.has(t.id)) { const loop = visit(t.id); if (loop) return loop }
  return null
}

export const techName = (doc: ProjectDoc, id: string): string => doc.research?.find((t) => t.id === id)?.name ?? id
