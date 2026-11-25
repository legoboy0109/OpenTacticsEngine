// Checks for status conditions (docs/SRPG.md section 9), in plain words. `blocks` = the loader refuses the game for it (this mirrors
// engine/core/data/loader_conditions.cpp, so a maker sees it before pressing Play; the loader's own message still shows in Problems after a save);
// the rest load fine but are worth a look: a condition nothing can inflict, an "until cured" condition nothing cures. The places that use a
// condition (a usable item, a weapon) have their refusals here too (`siteChecks`); a staff's are in staffChecks.ts and a skill rule's in skillChecks.ts.
import { inflictList } from './abilityInflicts'
import { blocksOfCondition, CONDITION_TURNS_MAX, CONDITION_WEAROFF_MAX, hasScalableEffect, MAX_CONDITIONS_LISTED, TAG_MAX } from './conditions'
import { isControlBlock } from './conditionBehaviour'
import { autoReviveChecks } from './downedChecks'
import { cureOf } from './staffConditions'
import { fxList, fxKind } from './staffFx'
import { MAX_BLOCKS } from './skillRules'
import { ruleIssues } from './skillChecks'
import type { Condition } from './srpgTypes'
import type { Extra, ProjectDoc, Section } from './types'

export interface ConditionCheck { section: Section; ids: string[]; text: string; blocks: boolean }

const whole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)
const effectsIn = (b: Extra): Extra[] => (Array.isArray(b.do) ? (b.do as Extra[]) : [])
const list = (v: unknown): Extra[] => (Array.isArray(v) ? (v as Extra[]) : [])
const chars = (s: string): number => Array.from(s).length

// --- who uses a condition ---------------------------------------------------------------------------------------------------------

/** Every block of every skill and condition (the places a `inflict` effect, an `immune` list or a `condition` test can sit). */
function allBlocks(doc: ProjectDoc): Array<{ owner: 'skill' | 'condition'; id: string; blocks: Extra[] }> {
  return [
    ...(doc.skills ?? []).map((s) => ({ owner: 'skill' as const, id: s.id, blocks: Array.isArray(s.blocks) ? s.blocks : [] })),
    ...(doc.conditions ?? []).map((c) => ({ owner: 'condition' as const, id: c.id, blocks: blocksOfCondition(c) })),
  ]
}

const storyCommands = (doc: ProjectDoc): Extra[] => {
  const out: Extra[] = []
  const walk = (cmds: unknown) => {
    for (const c of list(cmds)) {
      out.push(c)
      walk(c.then); walk(c.else)
      for (const a of list(c.answers)) walk(a.do)
    }
  }
  for (const m of Object.values(doc.maps)) for (const e of list(m?.events)) walk(e.do)
  return out
}

/** Does anything put the condition on a unit? A staff, a usable item, a weapon, an `inflict` effect of a skill or another condition, a story command. */
export function canBeInflicted(doc: ProjectDoc, id: string): boolean {
  if ((doc.items ?? []).some((i) => (i.kind === 'staff' && fxList(i).some((e) => fxKind(e) === 'inflict' && (e.inflict as Extra | undefined)?.condition === id)) || (i.kind === 'usable' && i.inflict?.condition === id))) return true
  if ((doc.weapons ?? []).some((w) => w.inflict?.condition === id)) return true
  if ((doc.abilities ?? []).some((a) => inflictList(a).some((e) => e.condition === id))) return true  // an ability's "inflict"
  if (allBlocks(doc).some((o) => o.blocks.some((b) => effectsIn(b).some((e) => (e.inflict as Extra | undefined)?.condition === id)))) return true
  return storyCommands(doc).some((c) => c.addCondition === id)
}

/** "harmful" (the default) or "helpful" (docs/SRPG.md "Ability conditions"). */
export const kindOf = (doc: ProjectDoc, id: string): 'harmful' | 'helpful' => ((doc.conditions ?? []).find((c) => c.id === id)?.kind === 'helpful' ? 'helpful' : 'harmful')

/** Does anything take it off? A staff or usable item that cures it (or every condition), another condition that replaces it, a story command. */
export function canBeCured(doc: ProjectDoc, id: string): boolean {
  for (const i of doc.items ?? []) {
    if (i.kind === 'staff' && (() => { const c = cureOf(i); return c.on && (c.all || c.kind === kindOf(doc, id) || c.ids.includes(id)) })()) return true
    if (i.kind === 'usable' && (i.cure === 'all' || i.cure === kindOf(doc, id) || (Array.isArray(i.cure) && i.cure.includes(id)))) return true
  }
  for (const a of doc.abilities ?? []) {  // a cure ability (docs/SRPG.md "Ability conditions": also every harmful / helpful one)
    const c = a.effect?.cure
    if (c === 'all' || c === kindOf(doc, id) || (Array.isArray(c) && c.includes(id))) return true
  }
  if ((doc.conditions ?? []).some((c) => c.replaces?.includes(id))) return true
  return storyCommands(doc).some((c) => c.removeCondition === id || c.removeCondition === 'all')
}

// --- one condition ----------------------------------------------------------------------------------------------------------------

function shapeIssues(doc: ProjectDoc, c: Condition): string[] {
  const out: string[] = []
  if (c.name.trim() === '') out.push('it needs a name.')
  if (c.tag !== undefined && (c.tag === '' || chars(c.tag) > TAG_MAX)) out.push(`the map tag is the short text shown on the unit and must be 1 to ${TAG_MAX} characters.`)
  if (c.turns !== undefined && c.until !== undefined) out.push('it has both a number of turns and “until”. Pick one way for it to end.')
  else if (c.turns === undefined && c.until === undefined) out.push('it needs to say how long it lasts: a number of turns, until cured, or until the map ends.')
  else if (c.until !== undefined && c.until !== 'cured' && c.until !== 'mapEnd') out.push('it lasts “until” cured or until the map ends, nothing else.')
  else if (c.turns !== undefined && (!whole(c.turns) || c.turns < 1 || c.turns > CONDITION_TURNS_MAX)) out.push(`the turns must be a whole number from 1 to ${CONDITION_TURNS_MAX}.`)
  if (c.kind !== undefined && c.kind !== 'harmful' && c.kind !== 'helpful') out.push('its kind must be harmful (a curse) or helpful (a blessing).')
  if (c.wearOff !== undefined && (!whole(c.wearOff) || c.wearOff < 1 || c.wearOff > CONDITION_WEAROFF_MAX)) out.push(`the chance to wear off early must be a whole number from 1 to ${CONDITION_WEAROFF_MAX} (leave it out for never).`)
  if (c.scale !== undefined) {
    if (c.scale !== 'turnsLeft') out.push('the effects can only fade as the turns left run out (“turnsLeft”).')
    else if (c.until !== undefined) out.push('the effects can fade only for a condition that counts turns; this one lasts until it is cured or the map ends.')
    else if (!hasScalableEffect(c)) out.push('the effects are set to fade, but no rule that always applies has a number to fade (a stat bonus, growth, sight in fog, staff reach or carrying bonus).')
  }
  for (const r of c.replaces ?? []) {
    if (r === c.id) out.push('it replaces itself.')
    else if (!doc.conditions?.some((x) => x.id === r)) out.push(`it replaces “${r}”, which is not a condition of this game.`)
  }
  const blocks = blocksOfCondition(c)
  if (blocks.length === 0 && c.autoRevive === undefined) out.push('it needs at least one rule (something it does).')  // an autoRevive is something it does
  out.push(...autoReviveChecks(doc, c))
  if (blocks.length > MAX_BLOCKS) out.push(`it has more than ${MAX_BLOCKS} rules.`)
  return out
}

/** The effects of all blocks: cannot act and acts-on-its-own together are refused (a unit that cannot act cannot act on its own either). */
function contradiction(c: Condition): string | undefined {
  const kinds = new Set(blocksOfCondition(c).flatMap((b) => effectsIn(b).flatMap((e) => ['cannotAct', 'berserk'].filter((k) => k in e))))
  return kinds.has('cannotAct') && kinds.has('berserk') ? '“cannot act” and “acts on its own” contradict each other (a unit that cannot act cannot act on its own either). Make two conditions.' : undefined
}

export function conditionChecks(doc: ProjectDoc): ConditionCheck[] {
  const out: ConditionCheck[] = []
  const all = doc.conditions ?? []
  if (all.length > MAX_CONDITIONS_LISTED) out.push({ section: 'conditions', ids: [], text: `a game can have at most ${MAX_CONDITIONS_LISTED} conditions.`, blocks: true })
  const seen = new Set<string>()
  for (const c of all) {
    const say = (text: string, blocks = true) => out.push({ section: 'conditions', ids: [c.id], text: `${c.name}: ${text}`, blocks })
    if (seen.has(c.id)) say('another condition has the same id.')
    seen.add(c.id)
    for (const t of shapeIssues(doc, c)) say(t)
    blocksOfCondition(c).forEach((b, i) => {
      for (const t of ruleIssues(doc, b, true)) out.push({ section: 'conditions', ids: [c.id], text: `${c.name}, ${isControlBlock(b) ? 'what it stops' : `rule ${i + 1}`}: ${t}`, blocks: true })
    })
    const clash = contradiction(c)
    if (clash) say(clash)
    // Loads fine, but it can never come about / never end.
    if (!canBeInflicted(doc, c.id)) say('nothing can put this on a unit yet. Let a staff, an item, a weapon, a skill or an event inflict it.', false)
    if (c.until === 'cured' && !canBeCured(doc, c.id)) say('it lasts until it is cured, but nothing cures it. Let a staff, an item or an event cure it, or have another condition replace it.', false)
  }
  return out
}

// --- the places that use a condition: a usable item, a weapon ----------------------------------------------------------------------

const known = (doc: ProjectDoc, id: unknown): boolean => !!doc.conditions?.some((c) => c.id === id)
const unknownWords = (doc: ProjectDoc, id: unknown): string => (doc.conditions?.length ? `the condition “${String(id)}” does not exist (see the Conditions screen).` : `the game has no conditions yet, so “${String(id)}” cannot be named.`)

function turnsIssue(doc: ProjectDoc, condition: unknown, turns: unknown): string | undefined {
  if (turns === undefined) return undefined
  if (!whole(turns) || turns < 1 || turns > CONDITION_TURNS_MAX) return `the turns must be a whole number from 1 to ${CONDITION_TURNS_MAX}.`
  const c = doc.conditions?.find((x) => x.id === condition)
  return c && c.until !== undefined ? `${c.name} does not count turns (it lasts until it is cured or the map ends), so the turns mean nothing. Remove them.` : undefined
}

export function siteChecks(doc: ProjectDoc): ConditionCheck[] {
  const out: ConditionCheck[] = []
  for (const i of doc.items ?? []) {
    const say = (text: string) => out.push({ section: 'items', ids: [i.id], text: `${i.name}: ${text}`, blocks: true })
    if (i.kind !== 'usable' && (i.cure !== undefined || i.inflict !== undefined)) say('only a usable item can cure or inflict a condition.')
    if (i.cure !== undefined) {
      if (i.cure !== 'all' && i.cure !== 'harmful' && i.cure !== 'helpful') {
        if (!Array.isArray(i.cure) || i.cure.length === 0) say('curing needs a list of conditions, or every condition.')
        else {
          const seenIds = new Set<string>()
          for (const x of i.cure) {
            if (!known(doc, x)) say(`it cures ${unknownWords(doc, x)}`)
            else if (seenIds.has(x)) say(`it cures “${x}” twice.`)
            seenIds.add(x)
          }
        }
      }
    }
    if (i.inflict !== undefined) {
      const v = i.inflict as Extra
      if (!v || typeof v !== 'object' || typeof v.condition !== 'string') say('inflicting needs a condition.')
      else {
        const extra = Object.keys(v).find((k) => k !== 'condition' && k !== 'turns')
        if (extra) say(`inflicting has no “${extra}”, only a condition and optional turns.`)
        else if (!known(doc, v.condition)) say(`it inflicts ${unknownWords(doc, v.condition)}`)
        else { const t = turnsIssue(doc, v.condition, v.turns); if (t) say(t) }
      }
    }
  }
  for (const w of doc.weapons ?? []) {
    if (w.inflict === undefined) continue
    const say = (text: string) => out.push({ section: 'weapons', ids: [w.id], text: `${w.name}: ${text}`, blocks: true })
    const v = w.inflict as Extra
    if (!v || typeof v !== 'object' || typeof v.condition !== 'string') { say('inflicting needs a condition.'); continue }
    const extra = Object.keys(v).find((k) => k !== 'condition' && k !== 'chance' && k !== 'turns')
    if (extra) say(`inflicting has no “${extra}”, only a condition, a chance and optional turns.`)
    else if (!known(doc, v.condition)) say(`it may inflict ${unknownWords(doc, v.condition)}`)
    else { const t = turnsIssue(doc, v.condition, v.turns); if (t) say(t) }
    if (v.chance !== undefined && !(typeof v.chance === 'string' ? v.chance.trim() !== '' : whole(v.chance) && v.chance >= 1 && v.chance <= 100))
      say('the chance must be a percent from 1 to 100, or a formula such as “60 - b.res * 5” (a = the wielder, b = the unit struck).')
  }
  return out
}
