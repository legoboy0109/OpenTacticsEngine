// Editor-side checks for skills (docs/SRPG.md "Checks / Problems"): a skill nobody uses, plus what the loader would refuse
// about a rule, said in plain words so the maker sees it on the rule before pressing Play. (The loader's own messages
// still reach the Problems list after a save.) `ruleIssues` is also what shows "a rule the file has in a refused shape".
import type { SrpgCheck } from './abilityChecks'
import {
  AURA_EFFECTS, EFFECT_WHENS, FLAG_EFFECTS, FOE_EFFECTS, MAX_CONDITIONS, MAX_EFFECTS, NUMBER_RANGE, canJudgeFoe, condKindOf, condsFor, effectKindOf, hasChance, whenOf,
  type When,
} from './skillRules'
import { blocksOf } from './skills'
import type { Extra, ProjectDoc } from './types'

const list = (v: unknown): Extra[] => (Array.isArray(v) ? (v as Extra[]) : [])
const WHEN_WORDS: Record<When, string> = {
  always: 'a rule that always applies', turnStart: 'a rule for the start of a turn', combat: 'a rule for combat', strike: 'a rule for when the unit strikes',
  struck: 'a rule for when the unit is struck', defeat: 'a rule for after defeating a foe', afterCombat: 'a rule for after combat',
}

/** What is wrong with one rule, in plain words (empty = nothing the editor can see). */
export function ruleIssues(doc: ProjectDoc, b: Extra): string[] {
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
  }
  const hp = doc.game?.roles?.hp
  const move = doc.game?.roles?.move
  for (const e of effects) {
    const k = effectKindOf(e)
    if (!k) { out.push('It has an effect the editor does not know.'); continue }
    if (!EFFECT_WHENS[k].includes(when)) out.push(`That effect cannot be used in ${WHEN_WORDS[when]}.`)
    else if (aura && !AURA_EFFECTS.includes(k)) out.push('An effect that reaches other units can only change stats, hit, avoid, critical, critical avoid, damage and damage taken.')
    if (e.to === 'foe' && (!FOE_EFFECTS.includes(k) || when !== 'combat' || aura) && k !== 'hurt') out.push('Only some effects of a combat rule can be aimed at the foe.')
    if ((k === 'growths' || k === 'ability') && conds.length > 0) out.push(k === 'growths' ? 'Growths cannot have a condition.' : 'Giving an ability cannot have a condition.')
    if (k === 'stats' || k === 'growths') {
      const m = (e[k] && typeof e[k] === 'object' ? e[k] : {}) as Record<string, number>
      const ids = Object.keys(m)
      if (ids.length === 0) out.push('A stat effect needs at least one stat.')
      for (const s of ids) if (!doc.game?.stats?.some((x) => x.id === s)) out.push(`“${s}” is not a stat of this game.`)
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
