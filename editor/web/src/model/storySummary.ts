// Plain sentences for triggers, conditions, commands, objectives and whole events, so a list shows what happens
// without opening anything. Squares are shown 1-based (like the map editor's status line); the file stores 0-based.
import { chapterName } from './campaignTypes'
import { namedUnits, teamsOf, unitDisplayName } from './storyIds'
import {
  asFilter, commandKind, conditionKind, filterKind, rectOf, ruleKind, triggerKind,
  type Command, type CompareOp, type Condition, type Filter, type ObjectiveRule, type Objectives, type StoryEvent, type Trigger,
} from './storyTypes'
import type { MapFile, ProjectDoc } from './types'

export interface SumCtx { doc: ProjectDoc; map: MapFile }

const q = (s: string) => `“${s}”`
export const squareText = (x: number, y: number): string => `${x + 1}, ${y + 1}`
export const areaText = (r: { x: number; y: number; w: number; h: number }): string =>
  (r.w === 1 && r.h === 1 ? `square ${squareText(r.x, r.y)}` : `area ${squareText(r.x, r.y)} (${r.w} × ${r.h})`)

/** One line of someone's text: new lines become spaces and a long text is cut with "…". */
export function clip(text: string, max = 32): string {
  const one = text.replace(/\s+/g, ' ').trim()
  return one.length > max ? `${one.slice(0, max - 1).trimEnd()}…` : one
}

export const sideName = ({ map }: SumCtx, id: string): string => map.sides?.find((s) => s.id === id)?.name || id
const typeName = ({ doc }: SumCtx, id: string) => doc.units?.find((u) => u.id === id)?.name ?? id
export const itemLabel = ({ doc }: SumCtx, id: string): string => doc.items?.find((i) => i.id === id)?.name ?? id
export const techLabel = ({ doc }: SumCtx, id: string): string => doc.research?.find((t) => t.id === id)?.name ?? id
const resourceName = ({ doc }: SumCtx, id: string) => doc.game?.resources?.find((r) => r.id === id)?.name ?? id
export const switchName = ({ doc }: SumCtx, id: string): string => doc.game?.switches?.find((s) => s.id === id)?.name ?? id
export const variableName = ({ doc }: SumCtx, id: string): string => doc.game?.variables?.find((v) => v.id === id)?.name ?? id

/** A named unit as a name: its own name, else its type's; an id nothing knows is shown as typed. */
export function unitText(c: SumCtx, id: string): string {
  if (!id) return 'someone'
  const known = namedUnits(c.doc, c.map).find((u) => u.id === id)
  if (!known) return id
  const placed = c.map.units?.find((u) => u.id === id)
  if (placed) return unitDisplayName(c.doc, placed)
  return known.label.replace(/ \([^)]*\)$/, '')
}

/** A chapter's name (campaign scenes only; an id nothing knows is shown as typed). */
const chapterLabel = ({ doc }: SumCtx, id: string): string => {
  const ch = doc.campaign?.chapters.find((x) => x.id === id)
  return ch ? chapterName(ch) : id
}

const teamText = (c: SumCtx, id: string) => teamsOf(c.map).find((t) => t.id === id)?.label ?? id

export function filterText(c: SumCtx, f: Filter | string | undefined): string {
  const flt = asFilter(f)
  switch (filterKind(flt)) {
    case 'unit': return unitText(c, flt!.unit!)
    case 'side': return `any unit of side ${sideName(c, flt!.side!)}`
    case 'team': return `any unit of team ${teamText(c, flt!.team!)}`
    case 'type': return `any ${typeName(c, flt!.type!)}`
    case 'tag': return `any unit with the tag “${flt!.tag}”`
    default: return 'any unit'
  }
}

export function triggerText(c: SumCtx, t: Trigger): string {
  const side = t.side ?? c.map.sides?.[0]?.id
  const whose = side ? `, ${sideName(c, side)}’s turn` : ''
  switch (triggerKind(t)) {
    case 'start': return 'The battle starts'
    case 'won': return 'The battle is won'
    case 'lost': return 'The battle is lost'
    case 'end': return 'The battle ends'
    case 'turn': return `Turn ${t.turn}${whose}`
    case 'everyTurn': return `Every turn${whose}`
    case 'reach': return `${cap(filterText(c, t.by))} reaches the ${areaText(rectOf(t.reach))}`
    case 'defeated': return `${cap(filterText(c, t.defeated))} is defeated${t.by ? ` by ${filterText(c, t.by)}` : ''}`
    case 'fight': return `A fight begins: ${filterText(c, t.fight)}${t.with ? ` against ${filterText(c, t.with)}` : ''}`
    case 'talk': return `${unitText(c, t.talk ?? '')} talks to ${unitText(c, t.to ?? '')}`
    default: return 'Something happens'
  }
}

const OP_WORDS: Record<CompareOp, string> = { '=': 'is', '!=': 'is not', '<': 'is below', '<=': 'is at most', '>': 'is above', '>=': 'is at least' }
export const opWords = (op: string | undefined): string => OP_WORDS[(op ?? '=') as CompareOp] ?? 'is'

export function conditionText(c: SumCtx, k: Condition): string {
  switch (conditionKind(k)) {
    case 'switch': return `the switch ${q(switchName(c, k.switch!))} is ${k.is === false ? 'off' : 'on'}`
    case 'variable': return `the variable ${q(variableName(c, k.variable!))} ${opWords(k.op)} ${k.value ?? 0}`
    case 'alive': return `${filterText(c, k.alive)} is alive`
    case 'dead': return `${filterText(c, k.dead)} is defeated`
    case 'round': return `the round ${opWords(k.round?.op)} ${k.round?.value ?? 0}`
    case 'carries': return `${filterText(c, k.unit)} carries ${itemLabel(c, k.carries!)}`
    case 'researched': return `side ${sideName(c, k.side ?? '')} has researched ${q(techLabel(c, k.researched!))}`
    default: return 'something is true'
  }
}

export const conditionsText = (c: SumCtx, list: Condition[] | undefined): string =>
  (list?.length ? list.map((k) => conditionText(c, k)).join(' and ') : 'always')

/** "a Soldier", "an Archer". */
const aOrAn = (word: string) => `${/^[aeiou]/i.test(word) ? 'an' : 'a'} ${word}`
const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)
const lowerFirst = (s: string) => s.charAt(0).toLowerCase() + s.slice(1)

function amountsText(c: SumCtx, amounts: Record<string, number> | undefined): string {
  const parts = Object.entries(amounts ?? {}).map(([id, n]) => `${n} ${resourceName(c, id)}`)
  return parts.length ? parts.join(', ') : 'nothing'
}

export function commandText(c: SumCtx, cmd: Command): string {
  switch (commandKind(cmd)) {
    case 'say': {
      const who = cmd.speaker ? unitText(c, cmd.speaker) : cmd.speakerName ? cmd.speakerName : ''
      const text = clip(String(cmd.say ?? ''))
      return who ? `${who} says ${q(text)}` : `Says ${q(text)}`
    }
    case 'addUnit': {
      const u = cmd.addUnit
      if (!u) return 'Adds a unit'
      const name = u.name?.trim() ? `${u.name}, ${aOrAn(typeName(c, u.type))}` : aOrAn(typeName(c, u.type))
      return `Adds ${name} (${sideName(c, u.side)}) at ${squareText(u.x, u.y)}`
    }
    case 'removeUnit': return `Removes ${filterText(c, cmd.removeUnit)}`
    case 'moveUnit': return `Moves ${unitText(c, cmd.moveUnit ?? '')} to ${squareText(cmd.x ?? 0, cmd.y ?? 0)}`
    case 'changeSide': return `Moves ${filterText(c, cmd.changeSide)} to side ${sideName(c, cmd.side ?? '')}`
    case 'giveItem': return `Gives ${itemLabel(c, cmd.giveItem ?? '')} to ${filterText(c, typeof cmd.to === 'object' ? cmd.to : undefined)}`
    case 'grantResearch': return `Gives side ${sideName(c, cmd.side ?? '')} the technology ${q(techLabel(c, cmd.grantResearch ?? ''))}`
    case 'giveResource': return `Gives side ${sideName(c, cmd.side ?? '')} ${amountsText(c, cmd.giveResource)}`
    case 'setSwitch': return `Turns the switch ${q(switchName(c, cmd.setSwitch ?? ''))} ${cmd.to === false ? 'off' : 'on'}`
    case 'setVariable': return `Sets the variable ${q(variableName(c, cmd.setVariable ?? ''))} to ${typeof cmd.to === 'number' ? cmd.to : 0}`
    case 'addVariable': return `Adds ${cmd.by ?? 0} to the variable ${q(variableName(c, cmd.addVariable ?? ''))}`
    case 'objective': return cmd.objective?.text ? `Changes the objective to ${q(clip(cmd.objective.text))}` : 'Changes the objectives'
    case 'win': return `Team ${teamText(c, cmd.win ?? '')} wins the battle`
    case 'lose': return `Team ${teamText(c, cmd.lose ?? '')} loses the battle`
    case 'if': {
      const n = cmd.then?.length ?? 0, m = cmd.else?.length ?? 0
      return `If ${conditionsText(c, cmd.if)}: ${n} ${n === 1 ? 'command' : 'commands'}${m ? `, otherwise ${m}` : ''}`
    }
    case 'endTurn': return 'Ends the talking unit’s turn'
    case 'goto': return `Goes on to chapter ${q(chapterLabel(c, cmd.goto ?? ''))}`
    case 'focus': return typeof cmd.focus === 'string' ? `Moves the view to ${unitText(c, cmd.focus)}` : `Moves the view to ${squareText(rectOf(cmd.focus).x, rectOf(cmd.focus).y)}`
    default: return 'Does something'
  }
}

export function ruleText(c: SumCtx, r: ObjectiveRule): string {
  switch (ruleKind(r)) {
    case 'defeated': return `${cap(filterText(c, r.defeated))} is defeated`
    case 'reach': return `${r.by ? cap(filterText(c, r.by)) : 'A unit'} reaches the ${areaText(rectOf(r.reach))}`
    case 'round': return `Round ${r.round} begins`
    case 'survive': return `Survive ${r.survive} ${r.survive === 1 ? 'round' : 'rounds'}`
    default: return 'Something happens'
  }
}

export function objectivesText(c: SumCtx, o: Objectives | undefined): string {
  if (!o || (!o.text && !o.win?.length && !o.lose?.length)) return 'No objectives'
  const parts: string[] = []
  if (o.text) parts.push(q(clip(o.text, 40)))
  if (o.win?.length) parts.push(`win: ${o.win.map((r) => lowerFirst(ruleText(c, r))).join('; ')}`)
  if (o.lose?.length) parts.push(`lose: ${o.lose.map((r) => lowerFirst(ruleText(c, r))).join('; ')}`)
  return parts.join(' · ')
}

/** "Turn 4, Red’s turn → Gorm says “Now!…”, adds a Soldier (Red) at 10, 2 and 1 more". */
export function eventSummary(c: SumCtx, e: StoryEvent): string {
  return `${triggerText(c, e.when ?? {})} → ${commandsSummary(c, e.do, 'nothing yet')}`
}

/** "Gorm says “Now!…”, adds a Soldier (Red) at 10, 2 and 1 more": the first two commands, then how many are left. */
export function commandsSummary(c: SumCtx, list: Command[] | undefined, none: string): string {
  const first = (list ?? []).slice(0, 2).map((cmd, i) => (i === 0 || commandKind(cmd) === 'say' ? commandText(c, cmd) : lowerFirst(commandText(c, cmd))))
  const more = (list?.length ?? 0) - first.length
  return first.length ? `${first.join(', ')}${more > 0 ? ` and ${more} more` : ''}` : none
}

/** The sentences the clean-up lists use for what it removes. */
export const describeFor = (c: SumCtx) => ({
  event: (e: StoryEvent) => eventSummary(c, e),
  command: (k: Command) => commandText(c, k),
  rule: (r: ObjectiveRule) => ruleText(c, r),
})
