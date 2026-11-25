// Plain sentences for triggers, conditions, commands, objectives and whole events, so a list shows what happens
// without opening anything. Squares are shown 1-based (like the map editor's status line); the file stores 0-based.
import { behaviourPhrase } from './behaviour'
import { chapterName } from './campaignTypes'
import { mapChangeText } from './mapChangeStory'
import { placesOf } from './placeStory'
import { namedUnits, teamsOf, unitDisplayName } from './storyIds'
import { fightText } from './storyFight'
import { showMapText } from './storyShowMapText'
import { levelName, pairLabel } from './supports'
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
export const skillLabel = ({ doc }: SumCtx, id: string): string => doc.skills?.find((s) => s.id === id)?.name ?? id
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

/** "the level of Eirik" for a variable command that reads a unit; null when the number is written in the command. */
function readingText(c: SumCtx, cmd: Command): string | null {
  const stat = (id: string) => c.doc.game?.stats?.find((s) => s.id === id)?.name ?? id
  if (cmd.level !== undefined) return `the level of ${unitText(c, cmd.level)}`
  if (cmd.hp !== undefined) return `the hit points of ${unitText(c, cmd.hp)}`
  if (cmd.maxHp !== undefined) return `the most hit points of ${unitText(c, cmd.maxHp)}`
  if (cmd.stat !== undefined) return `the ${q(stat(cmd.stat))} stat of ${unitText(c, cmd.of ?? '')}`
  return null
}

/** A chapter's name (campaign scenes only; an id nothing knows is shown as typed). */
const chapterLabel = ({ doc }: SumCtx, id: string): string => {
  const ch = doc.campaign?.chapters.find((x) => x.id === id)
  return ch ? chapterName(ch) : id
}

/** A party's name (campaign scenes only; an id nothing knows is shown as typed). */
const partyLabel = ({ doc }: SumCtx, id: string): string => {
  const p = doc.campaign?.parties?.find((x) => x.id === id)
  return p ? (p.name?.trim() ? p.name : p.id) : id
}

/** "Eirik and Lyra": the two characters of a support pair, as people know them. */
const pairText = ({ doc }: SumCtx, pair: unknown): string =>
  (Array.isArray(pair) && typeof pair[0] === 'string' && typeof pair[1] === 'string' && pair[0] !== '' ? pairLabel(doc, { a: pair[0], b: pair[1] }) : 'two characters')

/** "the door d1 (Cell Door)": a door, chest or place by its story name; one that is gone is shown as typed. */
function placeText(c: SumCtx, id: string): string {
  const p = placesOf(c.doc, c.map).find((x) => x.id === id)
  const word = p?.kind === 'chest' ? 'chest' : p?.kind === 'visit' ? 'place' : p?.kind === 'door' ? 'door' : 'door or chest'
  return id === '' ? `a ${word}` : p ? `the ${word} ${q(id)} (${p.kindName})` : `${q(id)}`
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
    case 'actor': return 'the unit that set the event off'
    default: return 'any unit'
  }
}

export function triggerText(c: SumCtx, t: Trigger): string {
  const side = t.side ?? c.map.sides?.[0]?.id
  const whose = side ? `, ${sideName(c, side)}’s turn` : ''
  switch (triggerKind(t)) {
    case 'start': return 'The battle starts'
    case 'beforePrep': return 'Before the preparation screen'
    case 'won': return 'The battle is won'
    case 'lost': return 'The battle is lost'
    case 'end': return 'The battle ends'
    case 'turn': return `Turn ${t.turn}${whose}`
    case 'everyTurn': return `Every turn${whose}`
    case 'reach': return `${cap(filterText(c, t.by))} reaches the ${areaText(rectOf(t.reach))}`
    case 'defeated': return `${cap(filterText(c, t.defeated))} is defeated${t.by ? ` by ${filterText(c, t.by)}` : ''}`
    case 'fight': return `A fight begins: ${filterText(c, t.fight)}${t.with ? ` against ${filterText(c, t.with)}` : ''}`
    case 'talk': return `${unitText(c, t.talk ?? '')} talks to ${unitText(c, t.to ?? '')}`
    case 'promoted': return `${cap(filterText(c, t.promoted))} is promoted`
    case 'support': return `${cap(pairText(c, t.support))} reach ${t.level === undefined ? 'a support level' : `support level ${levelName(c.doc.supports?.rules, t.level - 1)}`}`
    case 'place': {
      const by = t.by ? ` by ${filterText(c, t.by)}` : ''
      const what = t.is === 'visited' ? 'is visited' : t.is === 'destroyed' ? 'is destroyed' : 'is opened'
      return `${cap(placeText(c, t.place ?? ''))} ${what}${by}`
    }
    default: return 'Something happens'
  }
}

const conditionLabel = (c: SumCtx, id: string): string => c.doc.conditions?.find((x) => x.id === id)?.name ?? id

const OP_WORDS: Record<CompareOp, string> = { '=': 'is', '!=': 'is not', '<': 'is below', '<=': 'is at most', '>': 'is above', '>=': 'is at least' }
/** "the difficulty is Hard" / "is at least Normal" / "is at most Normal". */
export function difficultyConditionText(doc: SumCtx['doc'], k: Condition): string {
  const name = (id: string | undefined) => doc.game?.difficulties?.list.find((d) => d.id === id)?.name ?? id ?? ''
  const v = k.difficulty
  if (typeof v === 'string') return `the difficulty is ${q(name(v))}`
  if (v?.atLeast !== undefined) return `the difficulty is at least ${q(name(v.atLeast))}`
  return `the difficulty is at most ${q(name(v?.atMost))}`
}

const partyWords = (k: Condition): string => (k.party === 'active' ? ' (active party)' : '')

export const opWords = (op: string | undefined): string => OP_WORDS[(op ?? '=') as CompareOp] ?? 'is'

export function conditionText(c: SumCtx, k: Condition): string {
  switch (conditionKind(k)) {
    case 'switch': return `the switch ${q(switchName(c, k.switch!))} is ${k.is === false ? 'off' : 'on'}`
    case 'variable': return `the variable ${q(variableName(c, k.variable!))} ${opWords(k.op)} ${k.value ?? 0}`
    case 'alive': return `${filterText(c, k.alive)} is alive`
    case 'dead': return `${filterText(c, k.dead)} is defeated`
    case 'round': return `the round ${opWords(k.round?.op)} ${k.round?.value ?? 0}`
    case 'carries': return `${filterText(c, k.unit)} carries ${itemLabel(c, k.carries!)}`
    case 'support': return `${pairText(c, k.support)}’s support level ${opWords(k.op ?? '>=')} ${k.value ?? 0}`
    case 'sees': return `team ${teamText(c, k.team ?? '')} sees ${filterText(c, k.sees)}`
    case 'researched': return `side ${sideName(c, k.side ?? '')} has researched ${q(techLabel(c, k.researched!))}`
    case 'hasSkill': return `${filterText(c, k.unit)} has the skill ${q(skillLabel(c, k.hasSkill ?? ''))}`
    case 'hasCondition': return `${filterText(c, k.unit)} has the condition ${q(conditionLabel(c, k.hasCondition ?? ''))}`
    case 'actor': return `${filterText(c, k.actor)} is the unit that set this off`
    case 'rosterAlive': return `${unitText(c, k.rosterAlive ?? '')} is alive in the campaign${partyWords(k)}`
    case 'rosterDead': return `${unitText(c, k.rosterDead ?? '')} has fallen in the campaign${partyWords(k)}`
    case 'inRoster': return `${unitText(c, k.inRoster ?? '')} is in the roster${partyWords(k)}`
    case 'difficulty': return difficultyConditionText(c.doc, k)
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

/** " (fades in over 2 seconds)"; nothing when no fade is given (the player's default cross-fade), " (at once)" for 0. */
const fadeText = (fade: number | undefined, dir: 'in' | 'out'): string =>
  (typeof fade !== 'number' ? '' : fade === 0 ? ' (at once)' : ` (fades ${dir} over ${fade} ${fade === 1 ? 'second' : 'seconds'})`)

const PLACE_WORDS: Record<string, string> = { farLeft: 'at the far left', left: 'on the left', midLeft: 'left of the middle', center: 'in the middle', midRight: 'right of the middle', right: 'on the right', farRight: 'at the far right' }
/** "Sends Eirik's items to the storage, except Potion" (docs/EVENTS.md "Sending a unit's items to storage"). */
function storeItemsText(c: SumCtx, cmd: Command): string {
  const keep = (Array.isArray(cmd.keep) ? cmd.keep : []).map((id) => itemLabel(c, id))
  return `Sends ${typeof cmd.storeItems === 'string' ? unitText(c, cmd.storeItems) : filterText(c, cmd.storeItems)}’s items to the storage${keep.length ? `, except ${keep.join(', ')}` : ''}`
}

export function commandText(c: SumCtx, cmd: Command): string {
  switch (commandKind(cmd)) {
    case 'say': {
      const who = cmd.speaker ? unitText(c, cmd.speaker) : cmd.speakerName ? cmd.speakerName : ''
      const text = clip(String(cmd.say ?? ''))
      const where = cmd.position ? ` (${PLACE_WORDS[cmd.position] ?? cmd.position})` : ''
      return who ? `${who} says ${q(text)}${where}` : `Says ${q(text)}${where}`
    }
    case 'choice': {
      const who = cmd.speaker ? unitText(c, cmd.speaker) : cmd.speakerName ? cmd.speakerName : ''
      const n = cmd.answers?.length ?? 0
      const asks = `asks ${q(clip(String(cmd.choice ?? '')))} (${n} ${n === 1 ? 'answer' : 'answers'}${cmd.store ? `, saved in ${q(variableName(c, cmd.store))}` : ''})`
      return who ? `${who} ${asks}` : cap(asks)
    }
    case 'addUnit': {
      const u = cmd.addUnit
      if (!u) return 'Adds a unit'
      const name = u.name?.trim() ? `${u.name}, ${aOrAn(typeName(c, u.type ?? ''))}` : aOrAn(typeName(c, u.type ?? ''))
      return `Adds ${name} (${sideName(c, u.side)}${u.guest === true ? ', as a guest' : ''}) at ${squareText(u.x, u.y)}`
    }
    case 'removeUnit': return cmd.hide === true ? `Hides ${filterText(c, cmd.removeUnit)} (it can come back)` : `Removes ${filterText(c, cmd.removeUnit)}`
    case 'fight': return fightText(c, cmd)
    case 'showMap': return showMapText(c, cmd)
    case 'returnMap': return 'Goes back to the battle’s map'
    case 'showUnit': return `Brings back ${unitText(c, cmd.showUnit ?? '')} at ${squareText(cmd.x ?? 0, cmd.y ?? 0)}`
    case 'moveUnit': return `${cmd.walk ? 'Walks' : 'Moves'} ${unitText(c, cmd.moveUnit ?? '')} to ${squareText(cmd.x ?? 0, cmd.y ?? 0)}`
    case 'guestJoins': return `Makes ${filterText(c, cmd.guestJoins)} an ordinary unit of the player, joining the party if it is a guest`
    case 'changeSide': return `Moves ${filterText(c, cmd.changeSide)} to side ${sideName(c, cmd.side ?? '')}`
    case 'addSupport': return `${(cmd.by ?? 0) < 0 ? 'Takes' : 'Gives'} ${Math.abs(cmd.by ?? 0)} support ${Math.abs(cmd.by ?? 0) === 1 ? 'point' : 'points'} ${(cmd.by ?? 0) < 0 ? 'from' : 'to'} ${pairText(c, cmd.addSupport)}`
    case 'promote': return typeof cmd.to === 'string' && cmd.to ? `Promotes ${unitText(c, cmd.promote ?? '')} into ${aOrAn(typeName(c, cmd.to))}` : `Promotes ${unitText(c, cmd.promote ?? '')} (its only promotion)`
    case 'giveItem': return `Gives ${itemLabel(c, cmd.giveItem ?? '')} to ${filterText(c, typeof cmd.to === 'object' ? cmd.to : undefined)}`
    case 'takeItem': return `Takes ${cmd.all ? 'every' : 'one'} ${q(itemLabel(c, cmd.takeItem ?? ''))} from ${filterText(c, cmd.from)}`
    case 'grantResearch': return `Gives side ${sideName(c, cmd.side ?? '')} the technology ${q(techLabel(c, cmd.grantResearch ?? ''))}`
    case 'teachSkill': return `Teaches the skill ${q(skillLabel(c, cmd.teachSkill ?? ''))} to ${filterText(c, typeof cmd.to === 'object' ? cmd.to : undefined)}`
    case 'forgetSkill': return `Makes ${filterText(c, cmd.from)} forget the skill ${q(skillLabel(c, cmd.forgetSkill ?? ''))}`
    case 'setBehaviour': return `Tells ${filterText(c, cmd.units)} to ${behaviourPhrase(cmd.setBehaviour)}`
    case 'wakeGroup': return `Wakes the group ${q(cmd.wakeGroup ?? '')}`
    case 'addCondition': return `Puts the condition ${q(conditionLabel(c, cmd.addCondition ?? ''))} on ${filterText(c, typeof cmd.to === 'object' ? cmd.to : undefined)}${cmd.turns ? ` for ${cmd.turns} ${cmd.turns === 1 ? 'turn' : 'turns'}` : ''}`
    case 'removeCondition': return cmd.removeCondition === 'all' ? `Cures ${filterText(c, cmd.from)} of every condition` : `Cures ${filterText(c, cmd.from)} of the condition ${q(conditionLabel(c, cmd.removeCondition ?? ''))}`
    case 'giveResource': return `Gives side ${sideName(c, cmd.side ?? '')} ${amountsText(c, cmd.giveResource)}`
    case 'setSwitch': return `Turns the switch ${q(switchName(c, cmd.setSwitch ?? ''))} ${cmd.to === false ? 'off' : 'on'}`
    case 'setVariable': return `Sets the variable ${q(variableName(c, cmd.setVariable ?? ''))} to ${readingText(c, cmd) ?? (typeof cmd.to === 'number' ? cmd.to : 0)}`
    case 'addVariable': return `Adds ${readingText(c, cmd) ?? cmd.by ?? 0} to the variable ${q(variableName(c, cmd.addVariable ?? ''))}`
    case 'objective': return cmd.objective?.text ? `Changes the objective to ${q(clip(cmd.objective.text))}` : 'Changes the objectives'
    case 'win': return `Team ${teamText(c, cmd.win ?? '')} wins the battle`
    case 'lose': return `Team ${teamText(c, cmd.lose ?? '')} loses the battle`
    case 'if': {
      const n = cmd.then?.length ?? 0, m = cmd.else?.length ?? 0
      return `If ${conditionsText(c, cmd.if)}: ${n} ${n === 1 ? 'command' : 'commands'}${m ? `, otherwise ${m}` : ''}`
    }
    case 'endTurn': return 'Ends the talking unit’s turn'
    case 'goto': return `Goes on to chapter ${q(chapterLabel(c, cmd.goto ?? ''))}`
    case 'playMusic': return `Plays the music ${q(cmd.playMusic ?? '')}${fadeText(typeof cmd.fade === 'number' ? cmd.fade : undefined, 'in')}`
    case 'stopMusic': return `Stops the music${fadeText(typeof cmd.fade === 'number' ? cmd.fade : undefined, 'out')}`
    case 'playSound': return `Plays the sound ${q(cmd.playSound ?? '')}`
    case 'openDoor': return `Opens ${placeText(c, cmd.openDoor ?? '')}`
    case 'closeDoor': return `Shuts ${placeText(c, cmd.closeDoor ?? '')}`
    case 'lockDoor': return `Locks ${placeText(c, cmd.lockDoor ?? '')}`
    case 'unlockDoor': return `Unlocks ${placeText(c, cmd.unlockDoor ?? '')}`
    case 'openChest': return `Opens ${placeText(c, cmd.openChest ?? '')}${typeof (cmd as Record<string, unknown>).by === 'string' ? `, the contents go to ${unitText(c, String((cmd as Record<string, unknown>).by))}` : ''}`
    case 'storeItems': return storeItemsText(c, cmd)
    case 'switchParty': return `Switches to the party ${q(partyLabel(c, cmd.switchParty ?? ''))}`
    case 'mergeParties': {
      const from = (Array.isArray(cmd.mergeParties) ? cmd.mergeParties : []).map((id) => q(partyLabel(c, id)))
      return `Merges ${from.length ? from.join(' and ') : 'no party'} into ${q(partyLabel(c, cmd.into ?? ''))}`
    }
    case 'focus': return `${typeof cmd.focus === 'string' ? `Moves the view to ${unitText(c, cmd.focus)}` : `Moves the view to ${squareText(rectOf(cmd.focus).x, rectOf(cmd.focus).y)}`}${typeof cmd.ms === 'number' ? (cmd.ms === 0 ? ' at once' : ` over ${cmd.ms} ms`) : typeof cmd.speed === 'number' ? ` at ${cmd.speed} squares a second` : ''}`
    case 'background':
      return cmd.background === null || cmd.background === undefined ? `Takes the background away${cmd.transition === 'fade' ? ' with a fade' : ''}` : `Shows the background ${q(String(cmd.background))}${cmd.transition === 'fade' ? ` with a fade over ${cmd.ms ?? 500} ms` : ''}`
    case 'fade': return cmd.fade === 'in' ? `Fades the screen back in over ${cmd.ms ?? 500} ms` : `Fades the screen out to ${cmd.color ?? '#000000'} over ${cmd.ms ?? 500} ms`
    case 'wait': return `Waits ${typeof cmd.wait === 'number' ? cmd.wait : 0} ms`
    case 'shake': return `Shakes the screen for ${cmd.shake?.ms ?? 400} ms (strength ${cmd.shake?.strength ?? 3})`
    case 'leave': return `${q(cmd.leave ?? '')} leaves the stage${cmd.transition === 'fade' ? ' with a fade' : ''}`
    case 'enter': return `${q(cmd.enter ?? '')} comes on stage ${PLACE_WORDS[cmd.position ?? ''] ?? ''}${cmd.transition === 'fade' ? ' with a fade' : ''}`
    case 'move': return `${q(cmd.move ?? '')} moves ${PLACE_WORDS[cmd.position ?? ''] ?? ''}${cmd.transition === 'cut' ? ' at once' : ''}`
    case 'setPhaseMusic': return !cmd.setPhaseMusic || cmd.setPhaseMusic.music === null ? `Takes the changed music of ${sideName(c, cmd.setPhaseMusic?.side ?? '')}’s turn away` : cmd.setPhaseMusic.music === '' ? `Gives ${sideName(c, cmd.setPhaseMusic.side ?? '')}’s turn no music of its own` : `Changes the music of ${sideName(c, cmd.setPhaseMusic.side ?? '')}’s turn to ${q(cmd.setPhaseMusic.music ?? '')}`
    case 'clearStage': return 'Clears the stage'
    case 'weather': return cmd.weather === 'rain' ? 'Makes it rain' : cmd.weather === 'snow' ? 'Makes it snow' : 'Clears the weather'
    case 'narrate': return `Narrates ${q(clip(String(cmd.narrate ?? '')))}`
    default: return mapChangeText(c.doc, cmd) ?? 'Does something'
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
