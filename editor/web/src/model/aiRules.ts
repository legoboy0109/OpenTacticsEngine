// game.json "rules.ai" (docs/AI.md "Scripted-style computer players"): the order the computer's units act in and the named sets of attack weights. Nothing is written
// for the defaults ("best", no sets), and a block left empty is removed, so the game.json of a game that does not use it stays as it was. The checks
// mirror engine/core/data/loader_ai_rules.cpp.
import { withGame } from './doc'
import { forEachCommand } from './storyWalk'
import type { ProjectDoc } from './types'

export type TurnOrder = 'best' | 'order'

export interface WeightSet {
  dmg?: number; left?: number; friends?: number; class?: number; turn?: number; counter?: number; danger?: number; wounds?: number
  killValue?: number; dmgCap?: number; leftBase?: number; friendsCap?: number; counterCap?: number; noCounterBonus?: number; dangerDiv?: number; dangerCap?: number
  attackAtZero?: boolean
  priority?: Record<string, number>
  [k: string]: unknown
}
export interface AiRulesBlock { turnOrder?: TurnOrder; weights?: Record<string, WeightSet>; [k: string]: unknown }

export interface WeightKey { key: keyof WeightSet & string; min: number; max: number; def: number; kind: 'weight' | 'base' }
/** Every number of a set with the loader's range and the documented default (what an empty box means). */
export const WEIGHT_KEYS: WeightKey[] = [
  { key: 'dmg', min: 0, max: 100, def: 1, kind: 'weight' }, { key: 'left', min: 0, max: 100, def: 0, kind: 'weight' },
  { key: 'friends', min: 0, max: 100, def: 0, kind: 'weight' }, { key: 'class', min: 0, max: 100, def: 0, kind: 'weight' },
  { key: 'turn', min: 0, max: 100, def: 0, kind: 'weight' }, { key: 'counter', min: 0, max: 100, def: 0, kind: 'weight' },
  { key: 'danger', min: 0, max: 100, def: 0, kind: 'weight' }, { key: 'wounds', min: 0, max: 100, def: 0, kind: 'weight' },
  { key: 'killValue', min: 0, max: 1000, def: 50, kind: 'base' }, { key: 'dmgCap', min: 0, max: 1000, def: 40, kind: 'base' },
  { key: 'leftBase', min: 0, max: 1000, def: 20, kind: 'base' }, { key: 'friendsCap', min: 0, max: 1000, def: 10, kind: 'base' },
  { key: 'counterCap', min: 0, max: 1000, def: 40, kind: 'base' }, { key: 'noCounterBonus', min: 0, max: 1000, def: 10, kind: 'base' },
  { key: 'dangerDiv', min: 1, max: 1000, def: 8, kind: 'base' }, { key: 'dangerCap', min: 0, max: 1000, def: 20, kind: 'base' },
]
export const PRIORITY_MIN = -1000
export const PRIORITY_MAX = 1000
export const isSetName = (n: string): boolean => /^[A-Za-z0-9_-]+$/.test(n)

const block = (doc: ProjectDoc): AiRulesBlock => (doc.game?.rules as { ai?: AiRulesBlock } | undefined)?.ai ?? {}

export function aiRules(doc: ProjectDoc): { turnOrder: TurnOrder; weights: Record<string, WeightSet> } {
  const b = block(doc)
  const w = b.weights
  return { turnOrder: b.turnOrder === 'order' ? 'order' : 'best', weights: w !== null && typeof w === 'object' && !Array.isArray(w) ? w : {} }
}

/** Writes `rules.ai` from the pieces: defaults vanish, an empty block is removed, unknown keys stay. */
function writeBlock(doc: ProjectDoc, fn: (b: AiRulesBlock) => AiRulesBlock): ProjectDoc {
  return withGame(doc, (g) => {
    const next = fn({ ...block(doc) })
    if (next.turnOrder !== 'order') delete next.turnOrder
    if (next.weights && Object.keys(next.weights).length === 0) delete next.weights
    const rules = { ...(g.rules ?? {}) } as Record<string, unknown>
    if (Object.keys(next).length) rules.ai = next
    else delete rules.ai
    return { ...g, rules }
  })
}

export const setTurnOrder = (doc: ProjectDoc, order: TurnOrder): ProjectDoc => writeBlock(doc, (b) => ({ ...b, turnOrder: order }))

export function addWeightSet(doc: ProjectDoc, name: string): ProjectDoc {
  return writeBlock(doc, (b) => ({ ...b, weights: { ...(b.weights ?? {}), [name]: {} } }))
}

export function removeWeightSet(doc: ProjectDoc, name: string): ProjectDoc {
  return writeBlock(doc, (b) => {
    const w = { ...(b.weights ?? {}) }
    delete w[name]
    return { ...b, weights: w }
  })
}

function editSet(doc: ProjectDoc, name: string, fn: (s: WeightSet) => WeightSet): ProjectDoc {
  return writeBlock(doc, (b) => (b.weights?.[name] ? { ...b, weights: { ...b.weights, [name]: fn({ ...b.weights[name] }) } } : b))
}

/** One number of a set; undefined (or the default) removes it, so a set only holds what it changes. */
export function setWeightNumber(doc: ProjectDoc, name: string, key: string, value: number | undefined): ProjectDoc {
  const spec = WEIGHT_KEYS.find((k) => k.key === key)
  return editSet(doc, name, (s) => {
    if (value === undefined || value === spec?.def) delete s[key]
    else s[key] = value
    return s
  })
}

export const setAttackAtZero = (doc: ProjectDoc, name: string, on: boolean): ProjectDoc => editSet(doc, name, (s) => { if (on) s.attackAtZero = true; else delete s.attackAtZero; return s })

/** The priority of a unit type (the `class` weight's table); undefined removes the row. */
export function setPriority(doc: ProjectDoc, name: string, type: string, value: number | undefined): ProjectDoc {
  return editSet(doc, name, (s) => {
    const p = { ...(s.priority ?? {}) }
    if (value === undefined) delete p[type]
    else p[type] = value
    if (Object.keys(p).length) s.priority = p
    else delete s.priority
    return s
  })
}

/** Behaviours (map units, addUnit units, setBehaviour commands, on every map) that use the weight set: for the delete question and the check. */
export function weightSetUsers(doc: ProjectDoc, name: string): number {
  let n = 0
  const uses = (b: unknown) => { if (b !== null && typeof b === 'object' && (b as { weights?: unknown }).weights === name) n++ }
  for (const m of Object.values(doc.maps)) {
    if (!m) continue
    for (const u of m.units ?? []) uses(u.behaviour)
    for (const e of m.events ?? []) forEachCommand(e.do, '', (c) => {
      if (c.addUnit && typeof c.addUnit === 'object') uses(c.addUnit.behaviour)
      if (c.setBehaviour !== undefined) uses(c.setBehaviour)
    })
  }
  return n
}

const isInt = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

/** What the loader would refuse about rules.ai, in plain words. */
export function aiRuleProblems(doc: ProjectDoc): string[] {
  const out: string[] = []
  const b = block(doc)
  if (b.turnOrder !== undefined && b.turnOrder !== 'best' && b.turnOrder !== 'order') out.push('The acting order must be “best action first” or “one unit at a time”.')
  if (b.weights === undefined) return out
  if (b.weights === null || typeof b.weights !== 'object' || Array.isArray(b.weights)) return [...out, 'The attack weights must be a list of named sets.']
  for (const [name, s] of Object.entries(b.weights)) {
    const say = (t: string) => out.push(`Attack weights “${name}”: ${t}`)
    if (name === '') say('a set needs a name.')
    if (s === null || typeof s !== 'object' || Array.isArray(s)) { say('must be a set of numbers.'); continue }
    for (const k of WEIGHT_KEYS) {
      const v = s[k.key]
      if (v !== undefined && (!isInt(v) || v < k.min || v > k.max)) say(`“${k.key}” must be a whole number from ${k.min} to ${k.max}.`)
    }
    if (s.attackAtZero !== undefined && typeof s.attackAtZero !== 'boolean') say('“attack at zero” must be on or off.')
    if (s.priority !== undefined) {
      if (s.priority === null || typeof s.priority !== 'object' || Array.isArray(s.priority)) say('the unit type priorities must be a list of unit type: number.')
      else for (const [t, v] of Object.entries(s.priority)) {
        if (!doc.units?.some((u) => u.id === t)) say(`the priority names the unit type “${t}”, which this game does not have.`)
        if (!isInt(v) || v < PRIORITY_MIN || v > PRIORITY_MAX) say(`the priority of “${t}” must be a whole number from ${PRIORITY_MIN} to ${PRIORITY_MAX}.`)
      }
    }
    if ((s.class ?? 0) !== 0 && !s.priority) out.push(`Attack weights “${name}”: the class weight does nothing until some unit types have a priority.`)
  }
  return out
}
