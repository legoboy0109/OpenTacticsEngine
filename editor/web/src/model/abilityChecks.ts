// Editor-side checks for abilities (docs/SRPG.md "Checks / Problems"): an ability nobody has, plus the loader's refusals
// said in plain words, so a maker sees them before pressing Play. (The loader's own messages still reach the Problems list.)
import { spendsItemProblems } from './abilityItem'
import { summonItemProblems } from './summonItems'
import { targetsFor, effectKind, hasFixedRange, stealOf, targetOf, summonOf, unlockOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc, Section } from './types'

const EFFECT_WORDS = { push: 'push', pull: 'pull', swap: 'swap places', moveBehind: 'move to the other side', movePast: 'move past', heal: 'heal', summon: 'summon', steal: 'steal', refresh: 'refresh', cure: 'cure', unlock: 'unlock', none: 'give a condition' } as const

/** `ids`: the unit types, items or abilities the message is about, so a card can show only what concerns its own. */
export interface SrpgCheck { section: Section; text: string; ids?: string[] }

/** Does any unit type, character or skill hand the ability out? */
export function abilityIsGiven(doc: ProjectDoc, id: string): boolean {
  if ((doc.units ?? []).some((u) => u.abilities?.includes(id))) return true
  if ((doc.characters ?? []).some((c) => c.abilities?.includes(id))) return true
  return (doc.skills ?? []).some((s) => (s.blocks ?? []).some((b) => Array.isArray(b.do) && (b.do as Array<{ ability?: string }>).some((e) => e?.ability === id)))
}

function shapeChecks(doc: ProjectDoc, a: Ability): string[] {
  const kind = effectKind(a)
  const target = targetOf(a)
  if (!kind) return [`${a.name}: it does not say what it does. Pick an effect.`]
  const out: string[] = []
  if (!targetsFor(kind).includes(target)) {
    if (kind === 'summon') out.push(`${a.name}: a summon needs a free square as its target.`)
    else if (kind === 'heal') out.push(`${a.name}: healing cannot be used on an enemy or a square.`)
    else if (kind === 'steal') out.push(`${a.name}: stealing works on an enemy next to the unit.`)
    else if (kind === 'refresh') out.push(`${a.name}: refreshing works on an ally.`)
    else if (kind === 'cure') out.push(`${a.name}: curing works on the unit itself or an ally.`)
    else if (kind === 'unlock') out.push(`${a.name}: unlocking works on a square of a door or chest.`)
    else out.push(`${a.name}: “${EFFECT_WORDS[kind]}” needs a unit next to it as its target.`)
  }
  if (kind === 'none' && a.effect !== undefined) out.push(`${a.name}: an ability with no effect of its own must leave “effect” out.`)
  if (kind === 'summon') {
    const type = summonOf(a).type
    if (!type) out.push(`${a.name}: choose which unit type it summons.`)
    else if (!doc.units?.some((u) => u.id === type)) out.push(`${a.name}: the unit it summons (“${type}”) is not a unit type of this game.`)
    for (const t of summonItemProblems(doc, a)) out.push(`${a.name}: ${t}`)
  }
  if (hasFixedRange(kind) && a.range && (a.range[0] !== 1 || a.range[1] !== 1)) out.push(`${a.name}: “${EFFECT_WORDS[kind]}” works only on a unit right next to it.`)
  if (kind === 'steal') {
    const s = stealOf(a)
    if (a.effect?.steal !== true && (typeof a.effect?.steal !== 'object' || a.effect.steal === null)) out.push(`${a.name}: the steal setting is not one the editor can read.`)
    if (s.equipped === true && s.weapons !== true) out.push(`${a.name}: it can take the weapon in hand only if it can take weapons at all.`)
    if (s.if !== undefined && (typeof s.if !== 'string' || s.if.trim() === '')) out.push(`${a.name}: the “only if” formula of the steal is empty.`)
    if (s.reason !== undefined && (typeof s.reason !== 'string' || s.reason.trim() === '')) out.push(`${a.name}: the reason an item cannot be taken must be a few words, or left out.`)
    if (s.reason !== undefined && s.if === undefined) out.push(`${a.name}: a reason goes with an “only if” formula. Write the formula, or remove the reason.`)
  }
  if (kind === 'refresh') {
    const r = a.effect?.refresh
    if (r !== true && !(r && typeof r === 'object' && Number.isInteger(r.perTurn) && r.perTurn >= 1 && r.perTurn <= 9)) out.push(`${a.name}: how many times a unit can be refreshed in a turn must be from 1 to 9.`)
  }
  if (kind === 'cure') {
    const c = a.effect?.cure
    if (c !== 'all') {
      const ids = Array.isArray(c) ? c : []
      if (ids.length === 0) out.push(`${a.name}: it cures nothing yet. Pick at least one condition, or all of them.`)
      for (const id of ids) if (!doc.conditions?.some((x) => x.id === id)) out.push(`${a.name}: the condition “${id}” does not exist.`)
    }
  }
  if (kind === 'unlock') {
    const u = a.effect?.unlock
    const okShape = u === true || (Array.isArray(u) && u.length > 0 && u.every((x) => x === 'door' || x === 'chest')) ||
      (!!u && typeof u === 'object' && !Array.isArray(u) && (u.kinds === undefined || (Array.isArray(u.kinds) && u.kinds.length > 0 && u.kinds.every((x) => x === 'door' || x === 'chest'))))
    if (!okShape) out.push(`${a.name}: unlocking must be for doors, chests or both.`)
    const needs = unlockOf(a).needs
    if (needs) {
      const item = doc.items?.find((i) => i.id === needs)
      if (!item) out.push(`${a.name}: the lockpick “${needs}” is not an item of this game.`)
      else if (item.uses === undefined) out.push(`${a.name}: the lockpick ${item.name} has unlimited uses. A lockpick that wears out needs a number of uses (Items screen).`)
    }
    if (!doc.structures?.some((s) => s.door || s.chest)) out.push(`${a.name}: this game has no door or chest yet, so there is nothing to unlock (Structures screen).`)
  }
  return out
}

export function abilityChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  for (const a of doc.abilities ?? []) {
    for (const text of shapeChecks(doc, a)) out.push({ section: 'abilities', ids: [a.id], text })
    for (const text of spendsItemProblems(doc, a)) out.push({ section: 'abilities', ids: [a.id], text: `${a.name}: ${text}` })
    if (!abilityIsGiven(doc, a.id)) out.push({ section: 'abilities', ids: [a.id], text: `${a.name}: nobody has this ability yet. Give it to a unit type on the Units screen.` })
  }
  return out
}
