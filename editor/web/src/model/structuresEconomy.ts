// Structure settings of the economy options (docs/ECONOMY.md): research here, upgrades and repairs.
import { sizeOf, updateStructure } from './structures'
import { patchObj, setNum } from './util'
import type { ProjectDoc, Structure, Upgrade } from './types'

export const setResearchHere = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateStructure(doc, id, { research: on ? true : undefined })

const sameFootprint = (a: Structure, b: Structure): boolean => sizeOf(a).w === sizeOf(b).w && sizeOf(a).h === sizeOf(b).h

/** Kinds `id` can be built up into: the same footprint, not itself, and not already listed (except `keep`, the entry being edited). */
export function upgradeTargets(doc: ProjectDoc, id: string, keep?: string): Structure[] {
  const me = doc.structures?.find((s) => s.id === id)
  if (!me) return []
  const taken = new Set((me.upgrades ?? []).map((u) => u.to))
  return (doc.structures ?? []).filter((s) => s.id !== id && sameFootprint(me, s) && (!taken.has(s.id) || s.id === keep))
}

/** "2 × 2": the footprint, for the reasons shown when a kind cannot be picked. */
export const footprintText = (s: Structure): string => `${sizeOf(s).w} × ${sizeOf(s).h}`

function withUpgrades(doc: ProjectDoc, id: string, fn: (list: Upgrade[]) => Upgrade[]): ProjectDoc {
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return doc
  const next = fn(s.upgrades ?? [])
  return updateStructure(doc, id, { upgrades: next.length ? next : undefined })
}

/** A new upgrade to the first kind that can still be picked (valid at once); the doc is unchanged when there is none. */
export function addUpgrade(doc: ProjectDoc, id: string): ProjectDoc {
  const to = upgradeTargets(doc, id)[0]
  return to ? withUpgrades(doc, id, (l) => [...l, { to: to.id }]) : doc
}

export const removeUpgrade = (doc: ProjectDoc, id: string, i: number): ProjectDoc => withUpgrades(doc, id, (l) => l.filter((_, k) => k !== i))

export const patchUpgrade = (doc: ProjectDoc, id: string, i: number, patch: Partial<Upgrade>): ProjectDoc =>
  withUpgrades(doc, id, (l) => l.map((u, k) => (k === i ? patchObj(u, patch) : u)))

export const setUpgradeCost = (doc: ProjectDoc, id: string, i: number, resource: string, value: number | undefined): ProjectDoc =>
  withUpgrades(doc, id, (l) => l.map((u, k) => (k === i ? patchObj(u, { cost: setNum(u.cost, resource, value) }) : u)))

// --- repairs: no key = every unit type; a list = only those unit types and tags; [] = none ---

export const repairsAll = (s: Structure): boolean => s.repairs === undefined
export const setRepairs = (doc: ProjectDoc, id: string, list: string[] | undefined): ProjectDoc => updateStructure(doc, id, { repairs: list })
