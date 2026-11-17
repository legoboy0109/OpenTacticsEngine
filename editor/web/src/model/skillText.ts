// The plain sentence a rule of a skill adds up to: "In combat, if it started the fight: damage +3." Shown under every rule
// and as the skill's second line in the list. Wording is a placeholder for the owner to review (docs/SRPG.md "E2 as built").
import { condKindOf, effectKindOf, whenOf, type When } from './skillRules'
import { amountOf } from './skillAmount'
import { statChance } from './skillChance'
import { blocksOf } from './skills'
import type { Skill } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'

export const WHEN_LEAD: Record<When, string> = {
  always: 'Always', turnStart: 'At the start of its turn', combat: 'In combat', strike: 'When it strikes', struck: 'When it is struck',
  defeat: 'After defeating a foe', afterCombat: 'After combat',
}
const OP_WORDS: Record<string, string> = { '=': 'exactly', '==': 'exactly', '!=': 'not', '<': 'below', '<=': 'at most', '>': 'above', '>=': 'at least' }
export const opWords = (op: unknown): string => OP_WORDS[String(op ?? '>=')] ?? 'at least'
const squares = (n: unknown) => `${n} ${n === 1 ? 'square' : 'squares'}`
const signed = (n: unknown) => (Number(n) >= 0 ? `+${n}` : `${n}`)

const statName = (doc: ProjectDoc, id: string) => doc.game?.stats?.find((s) => s.id === id)?.name ?? id
const statsText = (doc: ProjectDoc, m: unknown): string =>
  Object.entries((m && typeof m === 'object' ? m : {}) as Record<string, number>).map(([k, v]) => `${statName(doc, k)} ${signed(v)}`).join(', ')

/** One condition as words ("its HP is at most 50%"). `foe` conditions say "the foe". */
export function conditionText(doc: ProjectDoc, c: Extra): string {
  const foe = c.who === 'foe'
  const who = foe ? 'the foe' : 'it'
  const k = condKindOf(c)
  const v = (k ? c[k] : undefined) as Extra | string | boolean | undefined
  const o = (v && typeof v === 'object' ? v : {}) as Extra
  switch (k) {
    case 'hp': return `${foe ? 'the foe’s' : 'its'} HP is ${opWords(o.op)} ${o.percent ?? 0}%`
    case 'near': {
      const what = c.near === 'enemy' ? 'enemy' : 'ally'
      const match = c.match ? ` ${nameOfTypeOrTag(doc, String(c.match))}` : ''
      if ((c.op === '=' || c.op === '==') && Number(c.count ?? 1) === 0) return `no${match} ${what} is within ${squares(c.within ?? 1)}`
      return `${opWords(c.op)} ${c.count ?? 1}${match} ${what === 'enemy' ? (Number(c.count ?? 1) === 1 ? 'enemy' : 'enemies') : (Number(c.count ?? 1) === 1 ? 'ally' : 'allies')} within ${squares(c.within ?? 1)}`
    }
    case 'weapon': return `${foe ? 'the foe’s' : 'its'} weapon is ${weaponWords(doc, String(v ?? ''))}`
    case 'terrain': return `${who} stands on ${terrainWords(doc, String(v ?? ''))}`
    case 'structure': return v === true ? `${who} stands on a structure` : v === false ? `${who} stands on no structure` : `${who} stands on ${structureWords(doc, String(v ?? ''))}`
    case 'unit': return `${who} is ${nameOfTypeOrTag(doc, String(v ?? ''), true)}`
    case 'carrying': return v === false ? `${who} is not carrying anyone` : `${who} is carrying someone`
    case 'round': return `the round is ${opWords(o.op)} ${o.value ?? 0}`
    case 'initiating': return v === false ? 'it is defending' : 'it started the fight'
    case 'distance': return `the foe is ${o.op === '=' || o.op === '==' ? 'exactly' : opWords(o.op)} ${squares(o.value ?? 1)} away`
    case 'formula': return 'a custom rule holds'
    case 'condition': return `${who} has the condition ${conditionNameOf(doc, String(v ?? ''))}`
    default: return 'a condition the editor does not show (kept as it is)'
  }
}

const conditionNameOf = (doc: ProjectDoc, id: string): string => doc.conditions?.find((c) => c.id === id)?.name ?? id
const nameOfTypeOrTag = (doc: ProjectDoc, key: string, article = false): string => {
  const t = doc.units?.find((u) => u.id === key)
  if (t) return article ? `a ${t.name}` : t.name
  return article ? `tagged ${key}` : key
}
const weaponWords = (doc: ProjectDoc, key: string): string => doc.weapons?.find((w) => w.id === key)?.name ?? `in the group “${key}”`
const terrainWords = (doc: ProjectDoc, key: string): string => { const t = doc.terrain?.find((x) => x.id === key); return t ? t.name : `terrain tagged “${key}”` }
const structureWords = (doc: ProjectDoc, key: string): string => { const t = doc.structures?.find((x) => x.id === key); return t ? `a ${t.name}` : `a structure tagged “${key}”` }

/** One effect as words ("damage +3"). */
export function effectText(doc: ProjectDoc, e: Extra, when?: When): string {
  const k = effectKindOf(e)
  const v = k ? e[k] : undefined
  const foe = e.to === 'foe'
  const own = foe ? 'the foe’s ' : ''
  switch (k) {
    case 'stats': return foe ? `the foe’s ${statsText(doc, v)}` : statsText(doc, v)
    case 'growths': return `growth per level: ${statsText(doc, v)}`
    case 'maxMoveCost': return `every square costs at most ${v} movement`
    case 'pass': return 'can move through enemy units'
    case 'moveAgain': return 'can move again after acting'
    case 'xpPercent': return `experience gained ${v}%`
    case 'ability': return `has the ability “${doc.abilities?.find((a) => a.id === v)?.name ?? v}”`
    case 'hit': return `${own}hit ${signed(v)}`
    case 'crit': return `${own}critical ${signed(v)}`
    case 'damage': return `${own}damage ${signed(v)}`
    case 'avoid': return `${own}avoid ${signed(v)}`
    case 'critAvoid': return `${own}critical avoid ${signed(v)}`
    case 'damageTaken': return `${own}damage taken ${signed(v)}`
    case 'damagePercent': return `damage dealt ${v}%`
    case 'damageTakenPercent': return `damage taken ${v}%`
    case 'strikes': return `${own}strikes ${signed(v)}`
    case 'multiStrike': return `each strike is made ${v} times in a row`
    case 'strikeFirst': return 'strikes first when defending'
    case 'counterAnyRange': return 'counterattacks from any distance'
    case 'noCounter': return 'the foe cannot counterattack'
    case 'drain': return `heals itself by ${v}% of the damage it deals`
    case 'strikeAgain': return 'strikes once more'
    case 'survive': return 'survives a deadly strike with 1 HP'
    case 'heal': {
      const a = amountOf(e, 'heal')
      const amount = a.kind === 'percent' ? `${a.value}% of its HP` : a.kind === 'formula' ? `an amount from the formula ${a.text}` : `${a.value} HP`
      return e.to === 'allies' ? `heals allies within ${squares(e.within ?? 1)} by ${amount.replace('of its HP', 'of their HP')}` : `heals ${amount}`
    }
    case 'hurt': {
      const a = amountOf(e, 'hurt')
      const amount = a.kind === 'percent' ? `${a.value}% of its maximum HP` : a.kind === 'formula' ? `an amount from the formula ${a.text}` : `${a.value} HP`
      const end = a.canKill ? ' (can defeat it)' : ' (never below 1)'
      if (e.to === 'self') return `loses ${amount}${end}`
      return e.to === 'enemies' || (e.to === undefined && when === 'turnStart') ? `hurts enemies within ${squares(e.within ?? 1)} for ${amount}${end}` : `hurts the foe for ${amount}${end}`
    }
    case 'actAgain': return 'can move and act again'
    case 'cannotAct': return `cannot act${e.cannotAct && typeof e.cannotAct === 'object' ? ` (${[(e.cannotAct as Extra).counter ? 'can still counterattack' : '', (e.cannotAct as Extra).dodge ? 'can still dodge' : ''].filter(Boolean).join(', ')})` : ''}`
    case 'cannotMove': return 'cannot move'
    case 'berserk': return 'acts on its own'
    case 'forbid': {
      const f = (v && typeof v === 'object' ? v : {}) as Extra
      const kinds = ((f.kinds as string[] | undefined) ?? []).map((k) => (k === 'staff' ? 'staves' : k === 'usable' ? 'usable items' : 'weapons'))
      const groups = ((f.groups as string[] | undefined) ?? []).map((g) => `“${g}”`)
      return `cannot use ${[...kinds, ...(groups.length ? [`the groups ${groups.join(', ')}`] : [])].join(' or ') || 'anything yet'}`
    }
    case 'immune': return `cannot get ${((Array.isArray(v) ? v : []) as string[]).map((id) => conditionNameOf(doc, id)).join(' or ') || 'any condition'}`
    case 'inflict': {
      const o = (v && typeof v === 'object' ? v : {}) as Extra
      return `${when === 'struck' ? 'puts the one who struck it' : 'puts the unit it struck'} in ${conditionNameOf(doc, String(o.condition ?? ''))}${o.turns ? ` for ${o.turns} turns` : ''}`
    }
    default: return 'an effect the editor does not show (kept as it is)'
  }
}

export function chanceText(doc: ProjectDoc, chance: unknown): string {
  if (typeof chance === 'number') return `with a ${chance}% chance`
  const [stat, times] = statChance(chance)
  return stat ? `with a chance of its ${statName(doc, stat)}${times === 1 ? '' : ` times ${times}`} percent` : 'with a chance from a custom formula'
}

/** The sentence of one rule. */
export function blockSummary(doc: ProjectDoc, b: Extra): string {
  const when = whenOf(b)
  const conds = (Array.isArray(b.if) ? (b.if as Extra[]) : []).map((c) => conditionText(doc, c))
  const effects = (Array.isArray(b.do) ? (b.do as Extra[]) : []).map((e) => effectText(doc, e, when ?? undefined)).join(', ')
  const aura = b.aura as Extra | undefined
  const head = [when ? WHEN_LEAD[when] : 'At an unknown time', conds.length ? `if ${conds.join(' and ')}` : '', b.chance !== undefined ? chanceText(doc, b.chance) : ''].filter(Boolean).join(', ')
  const gives = aura ? `${aura.to === 'enemies' ? 'enemies' : 'allies'} within ${squares(aura.within ?? 1)} get ${effects}` : effects
  return `${head}: ${gives || 'nothing'}.`
}

/** Every rule's sentence, one after another. */
export const skillSummary = (doc: ProjectDoc, s: Skill): string => blocksOf(s).map((b) => blockSummary(doc, b)).join(' ')
