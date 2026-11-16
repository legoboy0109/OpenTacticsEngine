// Editor-side checks for abilities (docs/SRPG.md "Checks / Problems"): an ability nobody has, plus the loader's refusals
// said in plain words, so a maker sees them before pressing Play. (The loader's own messages still reach the Problems list.)
import { TARGETS_FOR, effectKind, isMove, summonOf, targetOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc, Section } from './types'

const EFFECT_WORDS = { push: 'push', pull: 'pull', swap: 'swap places', moveBehind: 'move to the other side', heal: 'heal', summon: 'summon' } as const

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
  if (!TARGETS_FOR[kind].includes(target)) {
    if (kind === 'summon') out.push(`${a.name}: a summon needs a free square as its target.`)
    else if (kind === 'heal') out.push(`${a.name}: healing cannot be used on an enemy or a square.`)
    else out.push(`${a.name}: “${EFFECT_WORDS[kind]}” needs a unit next to it as its target.`)
  }
  if (kind === 'summon') {
    const type = summonOf(a).type
    if (!type) out.push(`${a.name}: choose which unit type it summons.`)
    else if (!doc.units?.some((u) => u.id === type)) out.push(`${a.name}: the unit it summons (“${type}”) is not a unit type of this game.`)
  }
  if (isMove(kind) && a.range && (a.range[0] !== 1 || a.range[1] !== 1)) out.push(`${a.name}: “${EFFECT_WORDS[kind]}” works only on a unit right next to it.`)
  return out
}

export function abilityChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  for (const a of doc.abilities ?? []) {
    for (const text of shapeChecks(doc, a)) out.push({ section: 'abilities', ids: [a.id], text })
    if (!abilityIsGiven(doc, a.id)) out.push({ section: 'abilities', ids: [a.id], text: `${a.name}: nobody has this ability yet. Give it to a unit type on the Units screen.` })
  }
  return out
}
