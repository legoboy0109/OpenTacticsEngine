// The formula preset table from docs/EDITOR_UI.md. Each preset describes its
// expression as tokens, so ONE definition drives generating the Lua text,
// recognising it again later, and the plain-language summary.
import type { FormulaParams } from './types'

export type FormulaKey = 'damage' | 'hit' | 'crit' | 'strikes' | 'xpHit' | 'xpKill' | 'capture'

/** A literal piece of expression, or a slot filled from a parameter. A slot
 *  with `who` is a stat ('a.str' / 'b.def'); without it, a number. */
export type Tok = string | { p: string; who?: 'a' | 'b' }

interface ParamBase { id: string; label: string; help?: string }
export interface StatParam extends ParamBase {
  kind: 'stat'
  /** Words that identify the usual stat (matched against id and name). */
  hints: string[]
  /** Fallback: index among the stats that are not health/movement. */
  slot: number
}
export interface NumberParam extends ParamBase { kind: 'number'; default: number; min: number; max?: number; suffix?: string }
export interface BoolParam extends ParamBase { kind: 'bool'; default: boolean }
export type ParamDef = StatParam | NumberParam | BoolParam

export type NameOf = (statId: string) => string

export interface PresetDef {
  id: string
  name: string
  description: string
  params: ParamDef[]
  /** null = the key is removed from the file. */
  tokens: ((p: FormulaParams) => Tok[]) | null
  summary: (p: FormulaParams, n: NameOf) => string
}

export interface FormulaDef {
  key: FormulaKey
  label: string
  help: string
  presets: PresetDef[]
  /** What a missing key means: a preset that removes the key, or the
   *  expression the engine uses by default. */
  absent?: { preset: string } | { expr: string }
  /** Hand-written spellings the engine treats the same as a preset's text. */
  aliases?: Record<string, string>
}

const stat = (id: string, label: string, hints: string[], slot: number): StatParam => ({ id, kind: 'stat', label, hints, slot })
const num = (id: string, label: string, def: number, min = 0, max?: number, suffix?: string): NumberParam =>
  ({ id, kind: 'number', label, default: def, min, max, suffix })
const bool = (id: string, label: string, def: boolean): BoolParam => ({ id, kind: 'bool', label, default: def })

export const getNum = (p: FormulaParams, id: string, def: number): number => (typeof p[id] === 'number' ? (p[id] as number) : def)
export const getBool = (p: FormulaParams, id: string, def: boolean): boolean => (typeof p[id] === 'boolean' ? (p[id] as boolean) : def)
export const getStat = (p: FormulaParams, id: string): string => (typeof p[id] === 'string' ? (p[id] as string) : '')

const ATK = ['str', 'strength', 'atk', 'attack', 'power']
const DEF = ['def', 'defense', 'defence', 'armor']
const SKL = ['skl', 'skill', 'dex']
const SPD = ['spd', 'speed', 'agi']

const damage: FormulaDef = {
  key: 'damage',
  label: 'Damage',
  help: 'How much health an attack takes away.',
  presets: [
    {
      id: 'attack-minus-defense',
      name: 'Attack minus defense',
      description: "The attacker's strength plus the weapon's power, minus the target's toughness.",
      params: [
        stat('A', 'Attack stat', ATK, 0),
        stat('D', 'Defense stat', DEF, 1),
        bool('weapon', 'Add the weapon bonus', true),
        bool('eff', "Use the weapon's effectiveness against the target", false),
        bool('terrain', 'Terrain defense reduces damage', true),
      ],
      tokens: (p) => {
        const w = getBool(p, 'weapon', true)
        const vs = getBool(p, 'eff', false)
        const head: Tok[] = w
          ? ['(', { p: 'A', who: 'a' }, ` + (w.might or 0)${vs ? ' * vs' : ''})`]
          : vs ? ['(', { p: 'A', who: 'a' }, ' * vs)'] : [{ p: 'A', who: 'a' }]
        return [...head, ' - ', { p: 'D', who: 'b' }, getBool(p, 'terrain', true) ? ' - (tb.defense or 0)' : '']
      },
      summary: (p, n) => {
        const w = getBool(p, 'weapon', true)
        const vs = getBool(p, 'eff', false)
        let s = `Damage = attacker's ${n(getStat(p, 'A'))}`
        if (w) s += ' + weapon Might'
        if (vs) s += w ? ' (multiplied by how effective the weapon is against the target)' : ', multiplied by how effective the weapon is against the target'
        s += ` - defender's ${n(getStat(p, 'D'))}`
        if (getBool(p, 'terrain', true)) s += ' - terrain defense'
        return s + '.'
      },
    },
    {
      id: 'damage-chart',
      name: 'Damage chart',
      description: 'Each weapon has a table of damage percent against every kind of unit; weaker units hit softer.',
      params: [bool('terrain', 'Terrain reduces damage', true)],
      tokens: (p) => ['vs * (a.hp / a.maxhp)', getBool(p, 'terrain', true) ? ' * (1 - 0.1 * (tb.defense or 0))' : ''],
      summary: (p) =>
        "Damage = the weapon's damage percent against the target, scaled by the attacker's remaining health" +
        (getBool(p, 'terrain', true) ? ', and reduced by 10% for each terrain defense star.' : '.'),
    },
    {
      id: 'fixed',
      name: 'Fixed amount',
      description: 'Every hit does the same damage.',
      params: [num('N', 'Damage', 5, 0)],
      tokens: () => [{ p: 'N' }],
      summary: (p) => `Every hit does ${getNum(p, 'N', 5)} damage.`,
    },
  ],
}

const hit: FormulaDef = {
  key: 'hit',
  label: 'Chance to hit',
  help: 'Whether an attack lands.',
  absent: { preset: 'always' },
  presets: [
    { id: 'always', name: 'Always hits', description: 'No misses.', params: [], tokens: null, summary: () => 'Every attack hits.' },
    {
      id: 'accuracy',
      name: 'Weapon accuracy',
      description: "Each weapon's accuracy number is the percent chance.",
      params: [],
      tokens: () => ['(w.hit or 100)'],
      summary: () => "Chance to hit = the weapon's accuracy.",
    },
    {
      id: 'accuracy-vs-evasion',
      name: 'Accuracy against evasion',
      description: "Weapon accuracy plus the attacker's skill, minus the defender's speed.",
      params: [
        stat('S', 'Skill stat', SKL, 2),
        stat('P', 'Speed stat', SPD, 3),
        bool('terrain', 'Terrain avoid lowers the chance', true),
      ],
      tokens: (p) => ['(w.hit or 100) + ', { p: 'S', who: 'a' }, ' * 2 - ', { p: 'P', who: 'b' }, ' * 2', getBool(p, 'terrain', true) ? ' - (tb.avoid or 0)' : ''],
      summary: (p, n) =>
        `Chance to hit = weapon accuracy + attacker's ${n(getStat(p, 'S'))} x 2 - defender's ${n(getStat(p, 'P'))} x 2` +
        (getBool(p, 'terrain', true) ? ' - terrain avoid.' : '.'),
    },
  ],
}

const crit: FormulaDef = {
  key: 'crit',
  label: 'Critical hits',
  help: 'A lucky hit that does extra damage.',
  absent: { preset: 'none' },
  presets: [
    { id: 'none', name: 'No critical hits', description: 'Damage is always normal.', params: [], tokens: null, summary: () => 'There are no critical hits.' },
    {
      id: 'fixed',
      name: 'Fixed chance',
      description: 'The same percent chance for everyone.',
      params: [num('N', 'Chance', 5, 0, 100, '%')],
      tokens: () => [{ p: 'N' }],
      summary: (p) => `Every attack has a ${getNum(p, 'N', 5)}% chance to be a critical hit.`,
    },
    {
      id: 'stat',
      name: 'Based on a stat',
      description: "The attacker's stat, divided down, is the percent chance.",
      params: [stat('S', 'Stat', SKL, 2), num('K', 'Divide by', 2, 1)],
      tokens: () => [{ p: 'S', who: 'a' }, ' / ', { p: 'K' }],
      summary: (p, n) => `Critical chance (percent) = attacker's ${n(getStat(p, 'S'))} divided by ${getNum(p, 'K', 2)}.`,
    },
  ],
}

const strikes: FormulaDef = {
  key: 'strikes',
  label: 'Attacks per fight',
  help: 'How many times each side strikes in one fight.',
  absent: { preset: 'one' },
  presets: [
    { id: 'one', name: 'One attack', description: 'Each side strikes once.', params: [], tokens: null, summary: () => 'Each fighter attacks once.' },
    {
      id: 'double-faster',
      name: 'Attack twice when faster',
      description: 'A much faster attacker gets a second strike.',
      params: [stat('P', 'Speed stat', SPD, 3), num('N', 'Lead needed', 4, 1)],
      tokens: () => ['(', { p: 'P', who: 'a' }, ' - ', { p: 'P', who: 'b' }, ' >= ', { p: 'N' }, ') and 2 or 1'],
      summary: (p, n) => `The attacker strikes twice when its ${n(getStat(p, 'P'))} is at least ${getNum(p, 'N', 4)} higher than the defender's.`,
    },
  ],
}

const xp = (key: 'xpHit' | 'xpKill', label: string, help: string, def: string, defN: number, baseN: number, perN: number, verb: string): FormulaDef => ({
  key, label, help, absent: { expr: def },
  presets: [
    {
      id: 'fixed',
      name: 'Fixed amount',
      description: 'The same experience every time.',
      params: [num('N', 'Experience', defN, 0)],
      tokens: () => [{ p: 'N' }],
      summary: (p) => `${getNum(p, 'N', defN)} experience ${verb}.`,
    },
    {
      id: 'stronger',
      name: 'More for stronger enemies',
      description: 'A base amount, plus extra for each level the enemy is above the attacker.',
      params: [num('B', 'Base amount', baseN, 0), num('K', 'Extra per level', perN, 0)],
      tokens: () => [{ p: 'B' }, ' + math.max(0, b.level - a.level) * ', { p: 'K' }],
      summary: (p) => `${getNum(p, 'B', baseN)} experience ${verb}, plus ${getNum(p, 'K', perN)} for each level the enemy is above the attacker.`,
    },
  ],
})

const capture: FormulaDef = {
  key: 'capture',
  label: 'Capture speed',
  help: 'How many capture points a unit removes each turn.',
  absent: { expr: 'a.hp / 1' },
  aliases: { 'a.hp': 'a.hp / 1' },
  presets: [
    {
      id: 'instant',
      name: 'Instant',
      description: 'Capturing always finishes in one turn.',
      params: [],
      tokens: () => ['9999'],
      summary: () => 'Capturing finishes in one turn.',
    },
    {
      id: 'health',
      name: 'Faster with more health',
      description: "The unit's current health, divided down, is the capture amount.",
      params: [num('K', 'Divide health by', 10, 1)],
      tokens: () => ['a.hp / ', { p: 'K' }],
      summary: (p) => `Capture points per turn = the unit's current health divided by ${getNum(p, 'K', 10)}.`,
    },
    {
      id: 'fixed',
      name: 'Fixed points per turn',
      description: 'Every unit captures at the same speed.',
      params: [num('N', 'Points per turn', 10, 1)],
      tokens: () => [{ p: 'N' }],
      summary: (p) => `Each unit removes ${getNum(p, 'N', 10)} capture points per turn.`,
    },
  ],
}

export const FORMULA_DEFS: Record<FormulaKey, FormulaDef> = {
  damage,
  hit,
  crit,
  strikes,
  xpHit: xp('xpHit', 'Experience for a hit', 'Experience earned each time a unit hits an enemy.', '10', 10, 10, 3, 'for each hit'),
  xpKill: xp('xpKill', 'Experience for a defeat', 'Experience earned for defeating an enemy.', '30', 30, 40, 10, 'for defeating an enemy'),
  capture,
}

export const FORMULA_KEYS = Object.keys(FORMULA_DEFS) as FormulaKey[]
