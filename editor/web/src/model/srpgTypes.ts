// Strategy-RPG data (docs/SRPG.md). Only what E0 itself needs is spelled out; every shape keeps the `Extra` index
// signature, so keys a screen does not know yet survive load -> edit -> save. E1 (characters), E2 (skills), E3
// (abilities, promotion, carrying) and E4 (supports) add the fields of their own list here, each in their own block.
import type { Command } from './storyTypes'
import type { Extra, ItemEntry, Named, NumMap } from './types'

/** "learns" entry on a unit type or a character: the skill is learned at that level. */
export interface Learn extends Extra { level: number; skill: string }

/** data/characters.json entry (section 1). */
export interface Character extends Named {
  description?: string
  /** The unit type it starts as. */
  type: string
  level?: number
  stats?: NumMap
  growths?: NumMap
  skills?: string[]
  learns?: Learn[]
  abilities?: string[]
  items?: ItemEntry[]
}

/** data/skills.json entry (section 3); the blocks are E2's to type. */
export interface Skill extends Named { description?: string; blocks?: Extra[] }

/** The `summon` effect of an ability (section 4); only `type` is required, a default is never written. */
export interface Summon extends Extra {
  type?: string; level?: 'summoner' | number; limit?: number; vanish?: boolean; lifetime?: number; canAct?: boolean; givesXp?: boolean
}
/** An ability's one effect: exactly one of these keys. */
export interface AbilityEffect extends Extra { push?: number; pull?: boolean; swap?: boolean; moveBehind?: boolean; heal?: number | 'full'; summon?: Summon }
export type AbilityTarget = 'self' | 'ally' | 'enemy' | 'unit' | 'cell'
/** data/abilities.json entry (section 4). `if` holds conditions of the skill kind; E3 shows and edits a few of them. */
export interface Ability extends Named {
  description?: string
  target?: AbilityTarget
  range?: [number, number]
  effect?: AbilityEffect
  cost?: 'action' | 'free'
  uses?: number
  cooldown?: number
  afterMove?: boolean
  if?: Extra[]
}

/** The combat numbers of a support level (section 6): a level states its whole bonus; `stats` never holds HP or move. */
export interface SupportBonus extends Extra { hit?: number; avoid?: number; crit?: number; critAvoid?: number; damage?: number; damageTaken?: number; stats?: NumMap }
/** One level of a pair: the points it needs, what it gives and the conversation that unlocks it (`say` commands only). */
export interface SupportLevel extends Extra { points: number; bonus?: SupportBonus; talk?: Command[] }
export interface SupportPair extends Extra { a: string; b: string; start?: number; levels: SupportLevel[] }
export type SupportTalks = 'battle' | 'camp' | 'both'
export interface SupportRules extends Extra {
  levelNames?: string[]
  points?: { endTurnAdjacent?: number; fightTogether?: number; heal?: number }
  range?: number; maxLevels?: number; conversations?: SupportTalks
}
/** data/supports.json (section 6). */
export interface Supports extends Extra { rules?: SupportRules; pairs: SupportPair[] }

/** The four optional features that have a file of their own. */
export type SrpgFeature = 'characters' | 'skills' | 'abilities' | 'supports'

/** One entry of a unit type's `promotions` (section 2); E3 refines it. */
export interface Promotion extends Extra {
  to: string; fromLevel?: number; items?: string[]; atLevel?: 'off' | 'offer' | 'auto'; levelAfter?: 'reset' | 'keep'; gains?: NumMap
}
/** game.json `rules.promotion` (section 2) and `rules.rescue` (section 5); E3 refines them. */
export interface PromotionRules extends Extra { levelAfter?: 'reset' | 'keep' }
export interface RescueRules extends Extra { mode?: 'carry' | 'pairUp'; canCarry?: string; carrierStats?: Record<string, number | { percent: number }>; pairBonus?: NumMap }
