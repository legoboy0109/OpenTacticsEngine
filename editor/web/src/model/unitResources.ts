// game.json "unitResources" (docs/SRPG.md "Unit resources (MP and the like)"): pools each unit has for itself, whose maximum is a stat
// and which abilities spend ("spend"). Defaults (start 100, no regeneration) are never written; an empty list removes the key.
// The checks mirror the loader's refusals in plain words.
import { withAbilities, withGame } from './doc'
import { makeId } from './ids'
import { moveItem, patchObj, updateById } from './util'
import type { ProjectDoc, UnitResource } from './types'

export const UNIT_RESOURCE_MAX = 4
export const NAME_MAX = 8
export const START_DEFAULT = 100
const ID_FORMAT = /^[A-Za-z0-9_-]+$/

export const unitResourceList = (doc: ProjectDoc): UnitResource[] => doc.game?.unitResources ?? []
export const unitResourceIds = (doc: ProjectDoc): string[] => unitResourceList(doc).map((r) => r.id)
export const unitResourceName = (r: UnitResource): string => r.name ?? r.id

export const newUnitResourceId = (doc: ProjectDoc, name: string): string =>
  makeId(name, unitResourceIds(doc), { fallback: 'pool', reserved: (doc.game?.resources ?? []).map((r) => r.id) })

/** The stat a new pool is the maximum of: the first stat that is neither the hit points nor the movement stat (a magic or energy stat), else the first stat. */
function defaultMaxStat(doc: ProjectDoc): string {
  const stats = doc.game?.stats ?? []
  const roles = doc.game?.roles
  return (stats.find((s) => s.id !== roles?.hp && s.id !== roles?.move) ?? stats[0])?.id ?? ''
}

const write = (doc: ProjectDoc, list: UnitResource[]): ProjectDoc =>
  withGame(doc, (g) => {
    const { unitResources: _old, ...others } = g
    return (list.length ? { ...others, unitResources: list } : others) as typeof g
  })

export function addUnitResource(doc: ProjectDoc, name: string, id: string = newUnitResourceId(doc, name)): ProjectDoc {
  if (unitResourceList(doc).length >= UNIT_RESOURCE_MAX) return doc
  return write(doc, [...unitResourceList(doc), { id, name: name.slice(0, NAME_MAX), max: defaultMaxStat(doc) }])
}

const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

export interface UnitResourcePatch { name?: string; max?: string; start?: number; perTurn?: number; percent?: number }

/** One edit of a pool: values equal to the default are removed (start 100, regeneration 0). */
export function updateUnitResource(doc: ProjectDoc, id: string, p: UnitResourcePatch): ProjectDoc {
  return write(doc, updateById(unitResourceList(doc), id, (r) => {
    const next: UnitResource = { ...r }
    if (p.name !== undefined) next.name = p.name
    if (p.max !== undefined) next.max = p.max
    if (p.start !== undefined) {
      const v = clamp(p.start, 0, 100)
      if (v === START_DEFAULT) delete next.start
      else next.start = v
    }
    if (p.perTurn !== undefined || p.percent !== undefined) {
      const regen = { ...(r.regen ?? {}) }
      if (p.perTurn !== undefined) regen.perTurn = clamp(p.perTurn, 0, 999)
      if (p.percent !== undefined) regen.percent = clamp(p.percent, 0, 100)
      for (const k of ['perTurn', 'percent'] as const) if (!regen[k]) delete regen[k]
      if (Object.keys(regen).length) next.regen = regen
      else delete next.regen
    }
    return next
  }))
}

export const moveUnitResource = (doc: ProjectDoc, from: number, to: number): ProjectDoc => write(doc, moveItem(unitResourceList(doc), from, to))

/** Takes a resource id (a pool or a side resource) out of every ability's "spend"; an emptied "spend" goes. */
export function dropSpend(doc: ProjectDoc, resource: string): ProjectDoc {
  if (!doc.abilities?.some((a) => a.spend && resource in a.spend)) return doc
  return withAbilities(doc, (list) => list.map((a) => {
    if (!a.spend || !(resource in a.spend)) return a
    const rest = { ...a.spend }
    delete rest[resource]
    return patchObj(a, { spend: Object.keys(rest).length ? rest : undefined })
  }))
}

/** Deletes the pool and what abilities paid in it, in one edit. */
export const removeUnitResource = (doc: ProjectDoc, id: string): ProjectDoc =>
  write(dropSpend(doc, id), unitResourceList(doc).filter((r) => r.id !== id))

/** A stat that goes takes the pools that are its maximum with it (the loader refuses a pool with no stat). */
export function dropPoolsOfStat(doc: ProjectDoc, stat: string): ProjectDoc {
  const gone = unitResourceList(doc).filter((r) => r.max === stat)
  return gone.reduce((d, r) => removeUnitResource(d, r.id), doc)
}

/** Which abilities pay in the pool, for the question before deleting it. */
export const poolUsers = (doc: ProjectDoc, id: string): string[] => (doc.abilities ?? []).filter((a) => a.spend && id in a.spend).map((a) => a.name)

const isWhole = (n: unknown, lo: number, hi: number): boolean => typeof n === 'number' && Number.isInteger(n) && n >= lo && n <= hi

export function unitResourceChecks(doc: ProjectDoc): string[] {
  const list = doc.game?.unitResources
  if (list === undefined) return []
  if (!Array.isArray(list)) return ['Unit resources must be a list.']
  const out: string[] = []
  if (list.length > UNIT_RESOURCE_MAX) out.push(`A game can have at most ${UNIT_RESOURCE_MAX} unit resources (it has ${list.length}).`)
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const sides = new Set((doc.game?.resources ?? []).map((r) => r.id))
  const seen = new Set<string>()
  list.forEach((r, i) => {
    const who = r.name ?? r.id ?? `number ${i + 1}`
    if (typeof r.id !== 'string' || !ID_FORMAT.test(r.id)) out.push(`Unit resource ${who}: its id may use only letters, digits, “_” and “-”.`)
    else if (seen.has(r.id)) out.push(`Unit resource ${who}: another one has the same id “${r.id}”.`)
    else if (sides.has(r.id)) out.push(`Unit resource ${who}: “${r.id}” is also a resource of the sides. The ids must differ, so abilities can tell them apart.`)
    seen.add(r.id)
    const name = r.name ?? r.id
    if (typeof name !== 'string' || name.length < 1 || name.length > NAME_MAX) out.push(`Unit resource ${who}: the name must be 1 to ${NAME_MAX} characters.`)
    if (typeof r.max !== 'string' || r.max === '') out.push(`Unit resource ${who}: pick the stat that is its maximum.`)
    else if (!stats.has(r.max)) out.push(`Unit resource ${who}: its maximum, “${r.max}”, is not a stat of this game.`)
    if (r.start !== undefined && !isWhole(r.start, 0, 100)) out.push(`Unit resource ${who}: “start” must be a whole percent from 0 to 100.`)
    if (r.regen !== undefined) {
      if (!r.regen || typeof r.regen !== 'object' || Array.isArray(r.regen)) out.push(`Unit resource ${who}: refilling must have “per turn” and “percent” numbers.`)
      else {
        if (r.regen.perTurn !== undefined && !isWhole(r.regen.perTurn, 0, 999)) out.push(`Unit resource ${who}: the refill per turn must be a whole number from 0 to 999.`)
        if (r.regen.percent !== undefined && !isWhole(r.regen.percent, 0, 100)) out.push(`Unit resource ${who}: the refill percent must be a whole number from 0 to 100.`)
      }
    }
  })
  return out
}
