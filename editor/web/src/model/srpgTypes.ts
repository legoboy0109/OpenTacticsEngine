// Strategy-RPG data (docs/SRPG.md). Only what E0 itself needs is spelled out; every shape keeps the `Extra` index
// signature, so keys a screen does not know yet survive load -> edit -> save. E1 (characters), E2 (skills), E3
// (abilities, promotion, carrying) and E4 (supports) add the fields of their own list here, each in their own block.
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

/** data/abilities.json entry (section 4); the rest is E3's to type. */
export interface Ability extends Named { description?: string }

/** data/supports.json (section 6): `rules`, `pairs`; E4 types them. */
export interface Supports extends Extra { rules?: Extra; pairs?: Array<{ a: string; b: string } & Extra> }

/** The four optional features that have a file of their own. */
export type SrpgFeature = 'characters' | 'skills' | 'abilities' | 'supports'

/** One entry of a unit type's `promotions` (section 2); E3 refines it. */
export interface Promotion extends Extra {
  to: string; fromLevel?: number; items?: string[]; atLevel?: 'off' | 'offer' | 'auto'; levelAfter?: 'reset' | 'keep'; gains?: NumMap
}
/** game.json `rules.promotion` (section 2) and `rules.rescue` (section 5); E3 refines them. */
export interface PromotionRules extends Extra { levelAfter?: 'reset' | 'keep' }
export interface RescueRules extends Extra { mode?: 'carry' | 'pairUp'; canCarry?: string; carrierStats?: Record<string, number | { percent: number }>; pairBonus?: NumMap }
