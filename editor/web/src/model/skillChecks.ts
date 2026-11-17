// Editor-side checks for skills (docs/SRPG.md "Checks / Problems"): a skill nobody uses, plus what the loader would refuse
// about a rule, said in plain words so the maker sees it on the rule before pressing Play. (The loader's own messages
// still reach the Problems list after a save.) `ruleIssues` is also what shows "a rule the file has in a refused shape".
import type { SrpgCheck } from './abilityChecks'
import { groupsOf } from './ranks'
import { amountOf } from './skillAmount'
import { CONDITION_TURNS_MAX } from './conditions'
import {
  AURA_EFFECTS, CONDITION_ONLY, EFFECT_WHENS, FLAG_EFFECTS, FOE_EFFECTS, MAX_CONDITIONS, MAX_EFFECTS, NUMBER_RANGE, canJudgeFoe, condKindOf, condsFor, effectKindOf, hasChance, whenOf,
  type EffectKind, type When,
} from './skillRules'
import { blocksOf } from './skills'
import type { Extra, ProjectDoc } from './types'

const list = (v: unknown): Extra[] => (Array.isArray(v) ? (v as Extra[]) : [])
const WHEN_WORDS: Record<When, string> = {
  always: 'a rule that always applies', turnStart: 'a rule for the start of a turn', combat: 'a rule for combat', strike: 'a rule for when the unit strikes',
  struck: 'a rule for when the unit is struck', defeat: 'a rule for after defeating a foe', afterCombat: 'a rule for after combat',
}

/** What is wrong with one rule, in plain words (empty = nothing the editor can see). */
export function ruleIssues(doc: ProjectDoc, b: Extra, forCondition = false): string[] {
  const out: string[] = []
  const when = whenOf(b)
  if (!when) return ['It does not say when it applies.']
  const aura = !!b.aura
  const conds = list(b.if)
  const effects = list(b.do)
  if (effects.length === 0) out.push('It does nothing yet. Add an effect.')
  if (effects.length > MAX_EFFECTS) out.push(`It has more than ${MAX_EFFECTS} effects.`)
  if (conds.length > MAX_CONDITIONS) out.push(`It has more than ${MAX_CONDITIONS} conditions.`)
  if (b.chance !== undefined && !hasChance(when)) out.push(`${WHEN_WORDS[when][0].toUpperCase()}${WHEN_WORDS[when].slice(1)} cannot have a chance: it must be the same every time.`)
  if (aura && when !== 'combat') out.push('Only a rule for combat can reach other units.')
  const okConds = condsFor(when, aura)
  for (const c of conds) {
    const k = condKindOf(c)
    if (k && !okConds.includes(k)) out.push(`“Only if” cannot ask that here: ${aura || when === 'turnStart' ? 'there is no fight to ask about.' : 'there is no combat.'}`)
    else if (k && c.who === 'foe' && !canJudgeFoe(k, when, aura)) out.push('There is no foe to ask about here.')
    if (k === 'condition' && !doc.conditions?.some((x) => x.id === c.condition)) out.push(`“Only if” asks about the condition “${String(c.condition)}”, which does not exist.`)
  }
  const hp = doc.game?.roles?.hp
  const move = doc.game?.roles?.move
  for (const e of effects) {
    const k = effectKindOf(e)
    if (!k) { out.push('It has an effect the editor does not know.'); continue }
    if (!EFFECT_WHENS[k].includes(when)) out.push(`That effect cannot be used in ${WHEN_WORDS[when]}.`)
    else if (aura && !AURA_EFFECTS.includes(k)) out.push('An effect that reaches other units can only change stats, hit, avoid, critical, critical avoid, damage and damage taken.')
    if (CONDITION_ONLY.includes(k) && !forCondition) out.push('That effect belongs to a status condition (the Conditions screen), not to a skill.')
    out.push(...ownEffectIssues(doc, e, k, when))
    if (e.to === 'foe' && (!FOE_EFFECTS.includes(k) || when !== 'combat' || aura) && k !== 'hurt') out.push('Only some effects of a combat rule can be aimed at the foe.')
    if ((k === 'growths' || k === 'ability') && conds.length > 0) out.push(k === 'growths' ? 'Growths cannot have a condition.' : 'Giving an ability cannot have a condition.')
    if (k === 'stats' || k === 'growths') {
      const m = (e[k] && typeof e[k] === 'object' ? e[k] : {}) as Record<string, number>
      const ids = Object.keys(m)
      if (ids.length === 0) out.push('A stat effect needs at least one stat.')
      for (const s of ids) if (!doc.game?.stats?.some((x) => x.id === s)) out.push(`“${s}” is not a stat of this game.`)
      if (k === 'stats' && forCondition && hp && ids.includes(hp)) out.push('A condition cannot change hit points (use hurt and heal for damage and healing over time).')
      if (k === 'stats' && when === 'combat' && ids.some((s) => s === hp || s === move)) out.push('A combat bonus cannot change hit points or movement.')
      if (k === 'stats' && when === 'always' && hp && ids.includes(hp) && conds.length > 0) out.push('A bonus to hit points cannot have a condition.')
    }
    if (k === 'ability' && !doc.abilities?.some((a) => a.id === e.ability)) out.push(`The ability “${String(e.ability)}” does not exist.`)
    const r = NUMBER_RANGE[k]
    if (r && (typeof e[k] !== 'number' || e[k] < r[0] || e[k] > r[1])) out.push(`That number must be from ${r[0]} to ${r[1]}.`)
    if (FLAG_EFFECTS.includes(k) && e[k] !== true) out.push('That effect is either on or not there.')
  }
  return out
}

const knownCondition = (doc: ProjectDoc, id: unknown): boolean => !!doc.conditions?.some((x) => x.id === id)
const isWhole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

/** What the loader refuses about the effects added for conditions (docs/SRPG.md section 9) and the new forms of heal and hurt. */
function ownEffectIssues(doc: ProjectDoc, e: Extra, k: EffectKind, when: When): string[] {
  const out: string[] = []
  const v = e[k]
  const obj = (x: unknown): Extra => (x && typeof x === 'object' && !Array.isArray(x) ? (x as Extra) : {})
  if (k === 'heal' || k === 'hurt') {
    const a = amountOf(e, k)
    if (a.kind === 'formula' ? a.text.trim() === '' : a.kind === 'percent' ? !isWhole(a.value) || a.value < 1 || a.value > 100 : !isWhole(a.value) || a.value < 1 || a.value > 1000000)
      out.push(a.kind === 'formula' ? 'The amount formula is empty.' : a.kind === 'percent' ? 'A percentage must be a whole number from 1 to 100.' : 'The amount must be a whole number of 1 or more.')
    if (k === 'hurt') {
      if (e.to === 'self' && when !== 'turnStart') out.push('Hurting itself belongs to a rule for the start of a turn.')
      if (e.to === 'enemies' && when !== 'turnStart') out.push('Hurting enemies within reach belongs to a rule for the start of a turn.')
      if (e.to === 'foe' && when !== 'afterCombat') out.push('Hurting the foe belongs to a rule for after combat.')
    }
  }
  if (k === 'cannotAct' && v !== true) {
    const o = obj(v)
    if (typeof v !== 'object' || Array.isArray(v) || Object.keys(o).some((x) => x !== 'counter' && x !== 'dodge') || ['counter', 'dodge'].some((x) => x in o && typeof o[x] !== 'boolean'))
      out.push('“Cannot act” is on, with the two optional switches “can still counterattack” and “can still dodge”.')
  }
  if (k === 'forbid') {
    const o = obj(v)
    const kinds = Array.isArray(o.kinds) ? (o.kinds as unknown[]) : []
    const groups = Array.isArray(o.groups) ? (o.groups as unknown[]) : []
    if (kinds.length === 0 && groups.length === 0) out.push('It forbids nothing yet. Tick an item kind or a group.')
    for (const x of kinds) if (x !== 'weapon' && x !== 'staff' && x !== 'usable') out.push(`“${String(x)}” is not an item kind (weapons, staves or usable items).`)
    for (const g of groups) if (!groupsOf(doc).includes(String(g))) out.push(`“${String(g)}” is no weapon or staff group of this game.`)
  }
  if (k === 'immune') {
    const list = Array.isArray(v) ? (v as unknown[]) : []
    if (list.length === 0) out.push('It is immune to nothing yet. Pick at least one condition.')
    for (const id of list) if (!knownCondition(doc, id)) out.push(`The condition “${String(id)}” does not exist.`)
  }
  if (k === 'inflict') {
    const o = obj(v)
    if (!knownCondition(doc, o.condition)) out.push(`The condition “${String(o.condition ?? '')}” does not exist.`)
    else if (o.turns !== undefined) {
      if (!isWhole(o.turns) || o.turns < 1 || o.turns > CONDITION_TURNS_MAX) out.push(`The turns must be a whole number from 1 to ${CONDITION_TURNS_MAX}.`)
      else if (doc.conditions?.find((x) => x.id === o.condition)?.until !== undefined) out.push('That condition does not count turns, so the number of turns means nothing.')
    }
  }
  return out
}

/** Does anything hand the skill out? Unit types (always or learned), characters, accessories, weapons. A skill nobody names
 *  can still be taught by an event: that counts. */
export function skillIsUsed(doc: ProjectDoc, id: string): boolean {
  const learns = (l: Array<{ skill: string }> | undefined) => !!l?.some((x) => x.skill === id)
  if ((doc.units ?? []).some((u) => u.skills?.includes(id) || learns(u.learns))) return true
  if ((doc.characters ?? []).some((c) => c.skills?.includes(id) || learns(c.learns))) return true
  if ((doc.weapons ?? []).some((w) => w.skills?.includes(id))) return true
  if ((doc.items ?? []).some((i) => i.skills?.includes(id))) return true
  return Object.values(doc.maps).some((m) => m && JSON.stringify(m.events ?? []).includes(`"teachSkill":"${id}"`))
}

export function skillChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  for (const s of doc.skills ?? []) {
    blocksOf(s).forEach((b, i) => { for (const text of ruleIssues(doc, b)) out.push({ section: 'skills', ids: [s.id], text: `${s.name}, rule ${i + 1}: ${text}` }) })
    if (blocksOf(s).length === 0) out.push({ section: 'skills', ids: [s.id], text: `${s.name}: it has no rule yet.` })
    if (!skillIsUsed(doc, s.id)) out.push({ section: 'skills', ids: [s.id], text: `${s.name}: nobody has this skill yet. Give it to a unit type, character, weapon or accessory.` })
  }
  return out
}
