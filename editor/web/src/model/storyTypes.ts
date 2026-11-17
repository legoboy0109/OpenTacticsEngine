// Story events and objectives as they are written in a map file (docs/EVENTS.md). A trigger, condition, command or
// objective entry is an object with ONE "kind" key (plus that kind's extras); the editor finds the kind by which key
// is there. Every interface lists all the keys any kind may use as optional, so the forms stay simple, and carries an
// index signature so a key the editor does not know survives load -> edit -> save.
import type { Extra, ItemEntry, MapUnit } from './types'

/** A rectangle of squares, 0-based as stored (the editor shows 1-based numbers). */
export interface Rect { x: number; y: number; w: number; h: number }

/** Which unit(s): exactly one key, or none = any unit. */
export interface Filter extends Extra { unit?: string; side?: string; team?: string; type?: string; tag?: string }
export type FilterKind = 'any' | 'unit' | 'side' | 'team' | 'type' | 'tag'
export const FILTER_KINDS: FilterKind[] = ['any', 'unit', 'side', 'team', 'type', 'tag']

export type CompareOp = '=' | '!=' | '<' | '<=' | '>' | '>='
export const COMPARE_OPS: CompareOp[] = ['=', '!=', '<', '<=', '>', '>=']

export interface Trigger extends Extra {
  battle?: 'start' | 'won' | 'lost' | 'end'
  /** A round number, or "every". */
  turn?: number | 'every'
  side?: string
  reach?: Rect
  by?: Filter
  defeated?: Filter
  fight?: Filter
  with?: Filter
  /** A unit matching this promoted (docs/SRPG.md "Promotion"). */
  promoted?: Filter
  /** Two characters of a support pair reach `level` (any level when it is left out); docs/SRPG.md "Supports". */
  support?: [string, string]
  level?: number
  talk?: string
  to?: string
  label?: string
}
/** The kinds the editor offers (the file has six keys; battle and turn split in more). */
export type TriggerKind = 'start' | 'won' | 'lost' | 'end' | 'turn' | 'everyTurn' | 'reach' | 'defeated' | 'fight' | 'talk' | 'promoted' | 'support'
export const TRIGGER_KINDS: TriggerKind[] = ['start', 'won', 'lost', 'end', 'turn', 'everyTurn', 'reach', 'defeated', 'fight', 'talk', 'promoted', 'support']

export interface Condition extends Extra {
  switch?: string
  is?: boolean
  variable?: string
  op?: CompareOp
  value?: number
  /** In a map: which unit(s). In a campaign scene: a roster unit id (a plain string). */
  alive?: Filter | string
  dead?: Filter | string
  round?: { op: CompareOp; value: number }
  carries?: string
  unit?: Filter
  /** A technology id: true once `side` has finished it (docs/ECONOMY.md). */
  researched?: string
  /** A skill id: true when a matching living unit has the skill (docs/SRPG.md "Skills"). `unit` says which units. */
  hasSkill?: string
  /** A status condition id: true when a matching living unit has it (docs/EVENTS.md "Status conditions"). `unit` says which units. Map events only. */
  hasCondition?: string
  /** The side that must have it (a `researched` condition). */
  side?: string
  /** Fog of war: the team sees at least one living unit matching this filter (needs `team`). Map events only. */
  sees?: Filter
  /** The team that must see it (a `sees` condition): a team name, or a side id for that side's team. */
  team?: string
  /** The support level of a pair of characters compared with `value` (`op`, default >=); docs/SRPG.md "Supports". */
  support?: [string, string]
}
export type ConditionKind = 'switch' | 'variable' | 'alive' | 'dead' | 'round' | 'carries' | 'researched' | 'sees' | 'support' | 'hasSkill' | 'hasCondition'
export const CONDITION_KINDS: ConditionKind[] = ['switch', 'variable', 'alive', 'dead', 'round', 'carries', 'researched', 'sees', 'support', 'hasSkill', 'hasCondition']

/** One answer of a `choice` command: its words, an optional hiding condition list and the commands it runs (docs/EVENTS.md "Dialogue choices"). */
export interface ChoiceAnswer extends Extra {
  text: string
  if?: Condition[]
  do?: Command[]
}
/** A choice has two to four answers. */
export const MIN_ANSWERS = 2
export const MAX_ANSWERS = 4

/** `to` is a filter (giveItem), true / false (setSwitch) or a number (setVariable). */
export interface Command extends Extra {
  say?: string
  /** Dialogue choices: the question (speaker / speakerName / portrait as for `say`), the variable that receives the answer's number
   *  (1 = the first answer), and the answers. */
  choice?: string
  store?: string
  answers?: ChoiceAnswer[]
  speaker?: string
  speakerName?: string
  portrait?: string
  addUnit?: MapUnit
  removeUnit?: Filter
  moveUnit?: string
  x?: number
  y?: number
  changeSide?: Filter
  side?: string
  giveItem?: string
  /** A filter (giveItem), true / false (setSwitch), a number (setVariable) or a unit type id (promote). */
  to?: Filter | boolean | number | string
  /** A unit's story id: it becomes the unit type `to` (docs/SRPG.md "Promotion"). */
  promote?: string
  /** Support points for a pair of characters (`by` may be negative); docs/SRPG.md "Supports". */
  addSupport?: [string, string]
  /** A skill id taught to (`to` filter) or forgotten by (`from` filter) every matching unit (docs/SRPG.md "Skills"). */
  teachSkill?: string
  forgetSkill?: string
  /** A status condition id given to (`to` filter, optional `turns`) or taken from (`from` filter; "all" = every condition) matching units. Map events only. */
  addCondition?: string
  removeCondition?: string
  turns?: number
  /** Who loses it: a filter (forgetSkill; takeItem in a map event) or a roster id (takeItem in a campaign scene). */
  from?: Filter | string
  /** takeItem: an item id; `from` says who loses it, `all: true` takes every copy (else one). */
  takeItem?: string
  all?: boolean
  giveResource?: Record<string, number>
  setSwitch?: string
  setVariable?: string
  addVariable?: string
  by?: number
  /** setVariable / addVariable: the number comes from a unit instead of `to` / `by` (docs/EVENTS.md "Reading a unit's numbers"). One of
   *  `level`, `hp`, `maxHp` (each holds the unit's id) or `stat` (a stat id, with `of` = the unit's id). */
  level?: string
  hp?: string
  maxHp?: string
  stat?: string
  of?: string
  objective?: Objectives
  win?: string
  lose?: string
  if?: Condition[]
  then?: Command[]
  else?: Command[]
  endTurn?: boolean
  focus?: { x: number; y: number } | string
  /** Campaign scenes only: the chapter that comes next after an "after" scene, or the chapter a "before" scene jumps to (docs/EVENTS.md "Campaign"). */
  goto?: string
  /** A technology id: `side` has it at once, free (docs/ECONOMY.md). Map events only. */
  grantResearch?: string
  /** Sound (docs/SOUND.md): a music track id (+ optional `fade` seconds), `stopMusic: true` (+ `fade`), a sound id. */
  playMusic?: string
  stopMusic?: boolean
  fade?: number
  playSound?: string
}
export type CommandKind =
  | 'say' | 'choice' | 'addUnit' | 'removeUnit' | 'moveUnit' | 'changeSide' | 'promote' | 'teachSkill' | 'forgetSkill' | 'giveItem' | 'takeItem' | 'giveResource'
  | 'setSwitch' | 'setVariable' | 'addVariable' | 'objective' | 'win' | 'lose' | 'if' | 'endTurn' | 'focus' | 'goto' | 'grantResearch'
  | 'playMusic' | 'stopMusic' | 'playSound' | 'addSupport' | 'addCondition' | 'removeCondition'
/** In the order the loader looks for them (`goto` is only ever found in a campaign scene). */
export const COMMAND_KINDS: CommandKind[] = [
  'say', 'choice', 'addUnit', 'removeUnit', 'moveUnit', 'changeSide', 'promote', 'teachSkill', 'forgetSkill', 'giveItem', 'takeItem', 'giveResource',
  'setSwitch', 'setVariable', 'addVariable', 'objective', 'win', 'lose', 'if', 'endTurn', 'focus', 'goto', 'grantResearch',
  'playMusic', 'stopMusic', 'playSound', 'addSupport', 'addCondition', 'removeCondition',
]

export interface StoryEvent extends Extra {
  id: string
  when: Trigger
  if?: Condition[]
  repeat?: boolean
  do: Command[]
}

export interface ObjectiveRule extends Extra { defeated?: Filter; reach?: Rect; by?: Filter; round?: number; survive?: number }
export type RuleKind = 'defeated' | 'reach' | 'round' | 'survive'
export const RULE_KINDS: RuleKind[] = ['defeated', 'reach', 'round', 'survive']
export interface Objectives extends Extra { text?: string; win?: ObjectiveRule[]; lose?: ObjectiveRule[] }

/** `items` in an addUnit is the same list as on a placed unit. */
export type { ItemEntry }

const has = (o: object, k: string) => Object.prototype.hasOwnProperty.call(o, k)

export function triggerKind(t: Trigger): TriggerKind | null {
  if (has(t, 'battle')) return t.battle === 'won' || t.battle === 'lost' || t.battle === 'end' ? t.battle : 'start'
  if (has(t, 'turn')) return t.turn === 'every' ? 'everyTurn' : 'turn'
  for (const k of ['reach', 'defeated', 'fight', 'talk', 'promoted', 'support'] as const) if (has(t, k)) return k
  return null
}

export function conditionKind(c: Condition): ConditionKind | null {
  return CONDITION_KINDS.find((k) => has(c, k)) ?? null
}

export function commandKind(c: Command): CommandKind | null {
  return COMMAND_KINDS.find((k) => has(c, k)) ?? null
}

/** Where a variable command takes its number from: written in the command, or read from a unit. */
export type NumberSource = 'number' | 'level' | 'hp' | 'maxHp' | 'stat'
export const NUMBER_SOURCES: NumberSource[] = ['number', 'level', 'hp', 'maxHp', 'stat']
export function numberSource(c: Command): NumberSource {
  return (['level', 'hp', 'maxHp', 'stat'] as const).find((k) => has(c, k)) ?? 'number'
}

export function ruleKind(r: ObjectiveRule): RuleKind | null {
  return RULE_KINDS.find((k) => has(r, k)) ?? null
}

/** Which filter key is set; none (or an empty object) = any unit. */
export function filterKind(f: Filter | undefined): FilterKind {
  if (!f) return 'any'
  return (FILTER_KINDS.find((k) => k !== 'any' && typeof f[k] === 'string') ?? 'any')
}

/** A plain string where a filter is expected means a specific unit (the loader accepts it). */
export function asFilter(f: Filter | string | undefined): Filter | undefined {
  return typeof f === 'string' ? { unit: f } : f
}

export const rectOf = (r: unknown): Rect => {
  const o = (r && typeof r === 'object' ? r : {}) as Partial<Rect>
  return { x: o.x ?? 0, y: o.y ?? 0, w: o.w ?? 1, h: o.h ?? 1 }
}
