// What a new trigger, condition, command or objective entry starts as: valid where it can be (the first thing that
// exists is picked), so the files stay loadable while the maker fills the form in. Changing a kind starts again
// from the new kind's default.
import { backgroundIds } from './backgrounds'
import { groupNames } from './behaviour'
import { firstPlaceFor, firstPlaceForEvent } from './placeStory'
import { namedUnits, teamsOf } from './storyIds'
import { pairsOf } from './supports'
import {
  filterKind, type Command, type CommandKind, type Condition, type ConditionKind, type Filter, type FilterKind,
  type ObjectiveRule, type RuleKind, type Trigger, type TriggerKind,
} from './storyTypes'
import type { MapFile, MapUnit, ProjectDoc } from './types'

export interface DefCtx {
  doc: ProjectDoc
  map: MapFile
  /** Set while a campaign scene is edited (campaignRoster.ts): fewer kinds, roster ids instead of unit filters. */
  scene?: {
    goto: boolean
    /** Only these kinds of command may be added (a support conversation takes `say` only). */
    only?: CommandKind[]
    /** The label of the speaker choice "nobody" when a line with no speaker is said by someone (a character's quotes: "This character (Eirik)"). */
    emptySpeaker?: string
  }
}

const firstUnit = (c: DefCtx) => namedUnits(c.doc, c.map)[0]?.id ?? ''
/** The first declared support pair (empty ids when there is none yet). */
const firstPair = (doc: ProjectDoc): [string, string] => {
  const p = pairsOf(doc)[0]
  return p ? [p.a, p.b] : ['', '']
}
/** The first side the computer plays (every unit of it by default), else anyone. */
const aiSideFilter = (c: DefCtx): Filter => { const s = c.map.sides?.find((x) => x.controller === 'ai'); return s ? { side: s.id } : {} }
const firstSide = (c: DefCtx) => c.map.sides?.[0]?.id ?? ''

/** A filter of the given kind pointing at the first thing of that kind (empty text when there is none yet). */
export function newFilter(c: DefCtx, kind: FilterKind): Filter {
  switch (kind) {
    case 'unit': return { unit: firstUnit(c) }
    case 'side': return { side: firstSide(c) }
    case 'team': return { team: teamsOf(c.map)[0]?.id ?? '' }
    case 'type': return { type: c.doc.units?.[0]?.id ?? '' }
    case 'tag': return { tag: '' }
    default: return {}
  }
}

/** Keeps the filter's kind when it is already that kind. */
export const withFilterKind = (c: DefCtx, f: Filter | undefined, kind: FilterKind): Filter => (filterKind(f) === kind && f ? f : newFilter(c, kind))

/** A sensible "which unit": the first named unit, else the first side, else anyone. */
export function defaultFilter(c: DefCtx): Filter {
  return firstUnit(c) ? { unit: firstUnit(c) } : firstSide(c) ? { side: firstSide(c) } : {}
}

export function newTrigger(c: DefCtx, kind: TriggerKind): Trigger {
  switch (kind) {
    case 'start': return { battle: 'start' }
    case 'won': return { battle: 'won' }
    case 'lost': return { battle: 'lost' }
    case 'end': return { battle: 'end' }
    case 'turn': return { turn: 1 }
    case 'everyTurn': return { turn: 'every' }
    case 'reach': return { reach: { x: 0, y: 0, w: 1, h: 1 } }
    case 'defeated': return { defeated: defaultFilter(c) }
    case 'fight': return { fight: defaultFilter(c) }
    case 'talk': {
      const units = namedUnits(c.doc, c.map)
      return { talk: units[0]?.id ?? '', to: units[1]?.id ?? '' }
    }
    case 'promoted': return { promoted: defaultFilter(c) }
    case 'support': return { support: firstPair(c.doc) }
    case 'place': return { place: firstPlaceForEvent(c.doc, c.map, 'opened'), is: 'opened' }
  }
}

export function newCondition(c: DefCtx, kind: ConditionKind): Condition {
  switch (kind) {
    case 'switch': return { switch: c.doc.game?.switches?.[0]?.id ?? '' }
    case 'variable': return { variable: c.doc.game?.variables?.[0]?.id ?? '', op: '=', value: 0 }
    // A scene names a roster unit by a plain id; a map event takes any unit filter.
    case 'alive': return { alive: c.scene ? firstUnit(c) : defaultFilter(c) }
    case 'dead': return { dead: c.scene ? firstUnit(c) : defaultFilter(c) }
    case 'round': return { round: { op: '>=', value: 2 } }
    case 'carries': return { carries: c.doc.items?.[0]?.id ?? '' }
    case 'researched': return { researched: c.doc.research?.[0]?.id ?? '', side: firstSide(c) }
    case 'hasSkill': return { hasSkill: c.doc.skills?.[0]?.id ?? '' }
    case 'hasCondition': return { hasCondition: c.doc.conditions?.[0]?.id ?? '' }
    case 'actor': return { actor: defaultFilter(c) }
    case 'difficulty': return { difficulty: c.doc.game?.difficulties?.default ?? c.doc.game?.difficulties?.list?.[0]?.id ?? '' }
    // The first team looking for the second side's units (a team always sees its own, so that would say nothing).
    case 'support': return { support: firstPair(c.doc), op: '>=', value: 1 }
    case 'sees': return { sees: { side: c.map.sides?.[1]?.id ?? firstSide(c) }, team: teamsOf(c.map)[0]?.id ?? '' }
  }
}

/** The unit an `addUnit` starts as: the first type and side, in the top-left square. */
export function newAddedUnit(c: DefCtx): MapUnit {
  return { type: c.doc.units?.[0]?.id ?? '', side: firstSide(c), x: 0, y: 0 }
}

export function newCommand(c: DefCtx, kind: CommandKind): Command {
  const sw = c.doc.game?.switches?.[0]?.id ?? ''
  const vr = c.doc.game?.variables?.[0]?.id ?? ''
  switch (kind) {
    case 'say': return { say: '' }
    case 'choice': return { choice: '', answers: [{ text: 'Yes' }, { text: 'No' }] }
    case 'addUnit': return { addUnit: newAddedUnit(c) }
    case 'removeUnit': return { removeUnit: defaultFilter(c) }
    case 'moveUnit': return { moveUnit: firstUnit(c), x: 0, y: 0 }
    case 'changeSide': return { changeSide: defaultFilter(c), side: firstSide(c) }
    case 'addSupport': return { addSupport: firstPair(c.doc), by: 10 }
    case 'promote': return { promote: firstUnit(c), to: c.doc.units?.[0]?.id ?? '' }
    case 'teachSkill': return { teachSkill: c.doc.skills?.[0]?.id ?? '', to: defaultFilter(c) }
    case 'forgetSkill': return { forgetSkill: c.doc.skills?.[0]?.id ?? '', from: defaultFilter(c) }
    case 'setBehaviour': return { setBehaviour: 'hold', units: aiSideFilter(c) }
    case 'wakeGroup': return { wakeGroup: groupNames(c.map)[0] ?? '' }
    case 'addCondition': return { addCondition: c.doc.conditions?.[0]?.id ?? '', to: defaultFilter(c) }
    case 'removeCondition': return { removeCondition: c.doc.conditions?.[0]?.id ?? '', from: defaultFilter(c) }
    case 'takeItem': return { takeItem: c.doc.items?.[0]?.id ?? '', from: c.scene ? firstUnit(c) : defaultFilter(c) }
    case 'giveItem': return { giveItem: c.doc.items?.[0]?.id ?? '', to: defaultFilter(c) }
    case 'grantResearch': return { grantResearch: c.doc.research?.[0]?.id ?? '', side: firstSide(c) }
    case 'giveResource': {
      const res = c.doc.game?.resources?.[0]?.id
      return { giveResource: res ? { [res]: 100 } : {}, side: firstSide(c) }
    }
    case 'setSwitch': return { setSwitch: sw, to: true }
    case 'setVariable': return { setVariable: vr, to: 0 }
    case 'addVariable': return { addVariable: vr, by: 1 }
    case 'objective': return { objective: {} }
    case 'win': return { win: teamsOf(c.map)[0]?.id ?? '' }
    case 'lose': return { lose: teamsOf(c.map)[0]?.id ?? '' }
    case 'if': return { if: [newCondition(c, c.scene ? 'switch' : 'round')], then: [] }
    case 'endTurn': return { endTurn: true }
    case 'focus': return { focus: { x: 0, y: 0 } }
    case 'background': return { background: backgroundIds(c.doc)[0] ?? '' }
    case 'fade': return { fade: 'out', ms: 500, color: '#000000' }
    case 'wait': return { wait: 500 }
    case 'shake': return { shake: { ms: 400, strength: 3 } }
    case 'leave': return { leave: firstUnit(c) }
    case 'clearStage': return { clearStage: true }
    case 'weather': return { weather: 'rain' }
    case 'narrate': return { narrate: '' }
    case 'goto': return { goto: c.doc.campaign?.chapters[0]?.id ?? '' }
    case 'playMusic': return { playMusic: Object.keys(c.doc.audio?.music ?? {})[0] ?? '' }
    case 'stopMusic': return { stopMusic: true }
    case 'playSound': return { playSound: Object.keys(c.doc.audio?.sounds ?? {})[0] ?? '' }
    case 'openDoor': case 'closeDoor': case 'lockDoor': case 'unlockDoor': case 'openChest': return { [kind]: firstPlaceFor(c.doc, c.map, kind) }
    case 'setTerrain': return { setTerrain: c.doc.terrain?.[0]?.id ?? '', area: { x: 0, y: 0, w: 1, h: 1 } }
    case 'applyChange': return { applyChange: Object.keys(c.map.changes ?? {})[0] ?? '' }
    case 'revertChange': return { revertChange: Object.keys(c.map.changes ?? {})[0] ?? '' }
    case 'setFog': return { setFog: false }
    case 'setVision': return { setVision: -1 }
    case 'storeItems': return { storeItems: c.scene ? firstUnit(c) : defaultFilter(c) }
  }
}

export function newRule(c: DefCtx, kind: RuleKind): ObjectiveRule {
  switch (kind) {
    case 'defeated': return { defeated: defaultFilter(c) }
    case 'reach': return { reach: { x: 0, y: 0, w: 1, h: 1 } }
    case 'round': return { round: 5 }
    case 'survive': return { survive: 5 }
  }
}
