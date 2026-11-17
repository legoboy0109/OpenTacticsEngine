// The four effects only a condition has (docs/SRPG.md section 9): cannot act, cannot move, forbid, acts on its own. The Conditions form shows
// them as plain switches; in the file they are effects of `always` blocks. A switch finds its effect wherever the file has it (the first
// one), edits it where it stands, and a new one goes into a block that holds only such effects (one is made when there is none, as long as
// the condition has room for another block). A block that holds only these effects is a "control block": the form does not show it as a rule
// card. Pure functions; the loader's refusals are kept as greyed choices (cannot act with berserk) and as checks (conditionChecks.ts).
import { updateCondition } from './conditions'
import { MAX_BLOCKS, MAX_EFFECTS } from './skillRules'
import type { Condition } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'

export const OWN_EFFECTS = ['cannotAct', 'cannotMove', 'forbid', 'berserk'] as const
export type OwnEffect = (typeof OWN_EFFECTS)[number]
export const FORBID_KINDS = ['weapon', 'staff', 'usable'] as const
export type ForbidKind = (typeof FORBID_KINDS)[number]

const blocksOf = (c: Condition): Extra[] => (Array.isArray(c.blocks) ? c.blocks : [])
const effectsOf = (b: Extra): Extra[] => (Array.isArray(b.do) ? (b.do as Extra[]) : [])
const ownKind = (e: Extra): OwnEffect | undefined => OWN_EFFECTS.find((k) => Object.prototype.hasOwnProperty.call(e, k))

/** A block that is only a place for these effects: `always`, no "only if", no chance, no aura, and nothing else in it. */
export const isControlBlock = (b: Extra): boolean =>
  b.when === 'always' && !(Array.isArray(b.if) && b.if.length) && b.chance === undefined && !b.aura && effectsOf(b).length > 0 && effectsOf(b).every((e) => ownKind(e) !== undefined)

/** The positions (in the file) of the blocks the form shows as rule cards. */
export const ruleIndexes = (c: Condition): number[] => blocksOf(c).flatMap((b, i) => (isControlBlock(b) ? [] : [i]))

function find(c: Condition, kind: OwnEffect): { b: number; e: number } | null {
  const blocks = blocksOf(c)
  for (let b = 0; b < blocks.length; b++) {
    const e = effectsOf(blocks[b]).findIndex((x) => ownKind(x) === kind)
    if (e >= 0) return { b, e }
  }
  return null
}
const totalEffects = (c: Condition): number => blocksOf(c).reduce((n, b) => n + effectsOf(b).length, 0)

// --- reading ---------------------------------------------------------------------------------------------------------------

export interface CannotAct { on: boolean; counter: boolean; dodge: boolean }
export function cannotActOf(c: Condition): CannotAct {
  const at = find(c, 'cannotAct')
  if (!at) return { on: false, counter: false, dodge: false }
  const v = effectsOf(blocksOf(c)[at.b])[at.e].cannotAct
  const o = v && typeof v === 'object' ? (v as Extra) : {}
  return { on: true, counter: o.counter === true, dodge: o.dodge === true }
}
export const hasOwn = (c: Condition, kind: 'cannotMove' | 'berserk'): boolean => find(c, kind) !== null

export interface Forbid { on: boolean; kinds: string[]; groups: string[] }
export function forbidOf(c: Condition): Forbid {
  const at = find(c, 'forbid')
  if (!at) return { on: false, kinds: [], groups: [] }
  const v = effectsOf(blocksOf(c)[at.b])[at.e].forbid
  const o = v && typeof v === 'object' ? (v as Extra) : {}
  const list = (x: unknown): string[] => (Array.isArray(x) ? x.filter((s): s is string => typeof s === 'string') : [])
  return { on: true, kinds: list(o.kinds), groups: list(o.groups) }
}

/** Why a switch cannot be turned on / off now, or '': 'berserk' / 'cannotAct' (they contradict), 'full' (no room for another block), 'last' (the only effect). */
export function ownBlock(c: Condition, kind: OwnEffect): '' | 'berserk' | 'cannotAct' | 'full' | 'last' {
  const on = find(c, kind) !== null
  if (on) return totalEffects(c) <= 1 ? 'last' : ''
  if (kind === 'cannotAct' && hasOwn(c, 'berserk')) return 'berserk'
  if (kind === 'berserk' && cannotActOf(c).on) return 'cannotAct'
  const room = blocksOf(c).some((b) => isControlBlock(b) && effectsOf(b).length < MAX_EFFECTS)
  return room || blocksOf(c).length < MAX_BLOCKS ? '' : 'full'
}

// --- writing ---------------------------------------------------------------------------------------------------------------

/** The condition with effect `kind` replaced where it stands, added to a control block, or (fx undefined) taken out; a block left empty goes. */
function putOwn(c: Condition, kind: OwnEffect, fx: Extra | undefined): Condition {
  const blocks = blocksOf(c)
  const at = find(c, kind)
  if (at) {
    if (fx) return { ...c, blocks: blocks.map((b, i) => (i === at.b ? { ...b, do: effectsOf(b).map((e, j) => (j === at.e ? fx : e)) } : b)) }
    if (totalEffects(c) <= 1) return c // a condition does something, always
    const next = blocks.flatMap((b, i): Extra[] => {
      if (i !== at.b) return [b]
      const rest = effectsOf(b).filter((_, j) => j !== at.e)
      return rest.length ? [{ ...b, do: rest }] : []
    })
    return { ...c, blocks: next }
  }
  if (!fx) return c
  const home = blocks.findIndex((b) => isControlBlock(b) && effectsOf(b).length < MAX_EFFECTS)
  if (home >= 0) return { ...c, blocks: blocks.map((b, i) => (i === home ? { ...b, do: [...effectsOf(b), fx] } : b)) }
  if (blocks.length >= MAX_BLOCKS) return c
  return { ...c, blocks: [...blocks, { when: 'always', do: [fx] }] }
}

const edit = (doc: ProjectDoc, id: string, fn: (c: Condition) => Condition): ProjectDoc => {
  const c = doc.conditions?.find((x) => x.id === id)
  if (!c) return doc
  const next = fn(c)
  return next === c ? doc : updateCondition(doc, id, { blocks: next.blocks })
}

/** On: `true` (the plain form: no counterattack, no dodge). Off: the effect goes. Refused while the condition makes the unit act on its own. */
export function setCannotAct(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return edit(doc, id, (c) => {
    if (!on) return putOwn(c, 'cannotAct', undefined)
    return cannotActOf(c).on || ownBlock(c, 'cannotAct') ? c : putOwn(c, 'cannotAct', { cannotAct: true })
  })
}
/** "Can still counterattack" / "can still dodge"; both off writes the plain `true`. */
export function setCannotActOption(doc: ProjectDoc, id: string, option: 'counter' | 'dodge', on: boolean): ProjectDoc {
  return edit(doc, id, (c) => {
    const cur = cannotActOf(c)
    if (!cur.on) return c
    const next = { ...cur, [option]: on }
    const value = next.counter || next.dodge ? { ...(next.counter ? { counter: true } : {}), ...(next.dodge ? { dodge: true } : {}) } : true
    return putOwn(c, 'cannotAct', { cannotAct: value })
  })
}
export function setCannotMove(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return edit(doc, id, (c) => putOwn(c, 'cannotMove', on ? { cannotMove: true } : undefined))
}
export function setBerserk(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return edit(doc, id, (c) => (on && ownBlock(c, 'berserk') ? c : putOwn(c, 'berserk', on ? { berserk: true } : undefined)))
}

const forbidFx = (kinds: string[], groups: string[]): Extra => ({ forbid: { ...(groups.length ? { groups } : {}), ...(kinds.length ? { kinds } : {}) } })
/** Switching on starts with staves forbidden (the silence of the starter set). */
export function setForbidOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  return edit(doc, id, (c) => (on && ownBlock(c, 'forbid') ? c : putOwn(c, 'forbid', on ? forbidFx(['staff'], []) : undefined)))
}
export function setForbidKinds(doc: ProjectDoc, id: string, kinds: string[]): ProjectDoc {
  return edit(doc, id, (c) => {
    const cur = forbidOf(c)
    return cur.on ? putOwn(c, 'forbid', forbidFx([...FORBID_KINDS.filter((k) => kinds.includes(k)), ...kinds.filter((k) => !(FORBID_KINDS as readonly string[]).includes(k))], cur.groups)) : c
  })
}
export function setForbidGroups(doc: ProjectDoc, id: string, groups: string[]): ProjectDoc {
  return edit(doc, id, (c) => {
    const cur = forbidOf(c)
    return cur.on ? putOwn(c, 'forbid', forbidFx(cur.kinds, groups)) : c
  })
}
