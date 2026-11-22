// An item's "teaches" (docs/JOBS.md "Learning from equipment (ability points)"): skills and abilities a unit may use while it has a weapon or
// an accessory, and masters with ability points. Edits keep the key out of the file when it is empty; the checks mirror the loader.
import { withItems } from './doc'
import { patchObj, updateById } from './util'
import type { Item, ProjectDoc, Teach } from './types'

export const TEACH_MAX = 8
export const POINTS_MAX = 9999
export const POINTS_DEFAULT = 100
export type TeachKind = 'ability' | 'skill'

/** Kinds of item that can teach. */
export const canTeach = (i: Item): boolean => i.kind === 'weapon' || i.kind === 'accessory'
export const teachesOf = (i: Item): Teach[] => (Array.isArray(i.teaches) ? i.teaches : [])
export const teachKind = (t: Teach): TeachKind => (t.skill !== undefined && t.ability === undefined ? 'skill' : 'ability')
export const teachId = (t: Teach): string => (teachKind(t) === 'skill' ? t.skill : t.ability) ?? ''
/** True when some item teaches something (the ability points rule means nothing otherwise). */
export const anyTeaches = (doc: ProjectDoc): boolean => (doc.items ?? []).some((i) => teachesOf(i).length > 0)

const entry = (kind: TeachKind, id: string, points: number, extra: Teach = { points }): Teach => {
  const { ability: _a, skill: _s, ...rest } = extra
  return { ...rest, [kind]: id, points } as Teach
}
const setList = (doc: ProjectDoc, itemId: string, fn: (l: Teach[]) => Teach[]): ProjectDoc =>
  withItems(doc, (list) => updateById(list, itemId, (i) => { const next = fn(teachesOf(i)); return patchObj(i, { teaches: next.length ? next : undefined }) }))

/** Points the game already gives an entry on another item (the same entry has one cost everywhere). */
export function pointsFor(doc: ProjectDoc, kind: TeachKind, id: string, except?: string): number | undefined {
  for (const i of doc.items ?? []) if (i.id !== except) for (const t of teachesOf(i)) if (teachKind(t) === kind && teachId(t) === id) return t.points
  return undefined
}

/** Adds an entry the item does not teach yet: the first ability, else the first skill; its points follow the other items that teach it. */
export function addTeach(doc: ProjectDoc, itemId: string): ProjectDoc {
  const item = doc.items?.find((i) => i.id === itemId)
  if (!item || !canTeach(item) || teachesOf(item).length >= TEACH_MAX) return doc
  const has = new Set(teachesOf(item).map((t) => `${teachKind(t)}:${teachId(t)}`))
  const all: Array<[TeachKind, string]> = [...(doc.abilities ?? []).map((a): [TeachKind, string] => ['ability', a.id]), ...(doc.skills ?? []).map((s): [TeachKind, string] => ['skill', s.id])]
  const [kind, id] = all.find(([k, i]) => !has.has(`${k}:${i}`)) ?? (['ability', ''] as [TeachKind, string])
  return setList(doc, itemId, (l) => [...l, entry(kind, id, pointsFor(doc, kind, id, itemId) ?? POINTS_DEFAULT)])
}

export const removeTeach = (doc: ProjectDoc, itemId: string, index: number): ProjectDoc => setList(doc, itemId, (l) => l.filter((_, i) => i !== index))

/** Another entry in row `index`: the points follow what other items say about it. */
export function setTeachEntry(doc: ProjectDoc, itemId: string, index: number, kind: TeachKind, id: string): ProjectDoc {
  return setList(doc, itemId, (l) => l.map((t, i) => (i === index ? entry(kind, id, pointsFor(doc, kind, id, itemId) ?? t.points, t) : t)))
}

export const setTeachPoints = (doc: ProjectDoc, itemId: string, index: number, points: number): ProjectDoc =>
  setList(doc, itemId, (l) => l.map((t, i) => (i === index ? { ...t, points: Math.min(POINTS_MAX, Math.max(1, Math.round(points))) } : t)))

/** A deleted skill or ability leaves the items that taught it (an item left with none loses the key). */
export function dropTeach(doc: ProjectDoc, kind: TeachKind, id: string): ProjectDoc {
  if (!(doc.items ?? []).some((i) => teachesOf(i).some((t) => teachKind(t) === kind && teachId(t) === id))) return doc
  return withItems(doc, (list) => list.map((i) => {
    if (!teachesOf(i).some((t) => teachKind(t) === kind && teachId(t) === id)) return i
    const rest = teachesOf(i).filter((t) => !(teachKind(t) === kind && teachId(t) === id))
    return patchObj(i, { teaches: rest.length ? rest : undefined })
  }))
}

/** Items that teach the entry, for the question before a delete. */
export const teachUsers = (doc: ProjectDoc, kind: TeachKind, id: string): string[] =>
  (doc.items ?? []).filter((i) => teachesOf(i).some((t) => teachKind(t) === kind && teachId(t) === id)).map((i) => i.name)

const isWhole = (n: unknown, lo: number, hi: number): boolean => typeof n === 'number' && Number.isInteger(n) && n >= lo && n <= hi

/** The loader's refusals about "teaches", one sentence each (starting with the item's name). */
export function teachChecks(doc: ProjectDoc): string[] {
  const out: string[] = []
  const skills = new Set((doc.skills ?? []).map((s) => s.id))
  const abilities = new Set((doc.abilities ?? []).map((a) => a.id))
  const cost = new Map<string, { points: number; item: string }>()
  const kinds = new Map<string, Set<TeachKind>>()
  for (const i of doc.items ?? []) {
    if (i.teaches === undefined) continue
    const say = (t: string) => out.push(`${i.name}: ${t}`)
    if (!canTeach(i)) { say('only a weapon or an accessory can teach.'); continue }
    if (!Array.isArray(i.teaches) || i.teaches.length < 1 || i.teaches.length > TEACH_MAX) { say(`it must teach 1 to ${TEACH_MAX} entries.`); continue }
    const seen = new Set<string>()
    i.teaches.forEach((t, n) => {
      const where = `entry ${n + 1}`
      const hasA = t.ability !== undefined
      const hasS = t.skill !== undefined
      if (hasA === hasS) { say(`${where}: pick an ability or a skill, one of them.`); return }
      const kind = teachKind(t)
      const id = teachId(t)
      if (typeof id !== 'string' || id === '') { say(`${where}: pick what it teaches.`); return }
      if (!isWhole(t.points, 1, POINTS_MAX)) say(`${where}: the points must be a whole number from 1 to ${POINTS_MAX}.`)
      if (kind === 'ability' ? !abilities.has(id) : !skills.has(id)) say(`${where}: “${id}” is not ${kind === 'ability' ? 'an ability' : 'a skill'} of this game.`)
      if (seen.has(`${kind}:${id}`)) say(`it teaches “${id}” twice.`)
      seen.add(`${kind}:${id}`)
      const set = kinds.get(id) ?? new Set<TeachKind>()
      set.add(kind)
      kinds.set(id, set)
      if (isWhole(t.points, 1, POINTS_MAX)) {
        const key = `${kind}:${id}`
        const first = cost.get(key)
        if (!first) cost.set(key, { points: t.points, item: i.name })
        else if (first.points !== t.points) say(`“${id}” costs ${t.points} points here but ${first.points} on ${first.item}: an entry has one cost.`)
      }
    })
  }
  for (const [id, set] of kinds) if (set.size > 1) out.push(`“${id}” is taught as a skill and as an ability. Use another id for one of them.`)
  return out
}
