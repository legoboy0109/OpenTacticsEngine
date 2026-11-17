// Difficulties (docs/DIFFICULTY.md): game.json "difficulties", a map's "difficulty", a unit's "difficulty" and "difficultyShift".
import type { Extra, NumMap } from './types'

/** How the enemies of the human sides are changed: levels higher or lower, flat stat additions, then a percent of the stats. */
export interface DifficultyEnemy extends Extra { levels?: number; stats?: NumMap; statsPercent?: number }
export interface DifficultyDef extends Extra {
  id: string
  name: string
  description?: string
  enemy?: DifficultyEnemy
  /** Experience the human sides' units gain, in percent (missing = 100). */
  xpPercent?: number
  /** Resources the human sides gain (income, given, looted), in percent (missing = 100). */
  resourcePercent?: number
}
export type DifficultyChange = 'never' | 'lowerOnly' | 'free'
/** game.json "difficulties": the list runs from the easiest to the hardest. */
export interface DifficultySet extends Extra {
  default: string
  /** Missing = 'lowerOnly'. */
  change?: DifficultyChange
  /** The stats "statsPercent" scales (missing = every stat but the movement stat). */
  percentStats?: string[]
  list: DifficultyDef[]
}
/** A map's own numbers for one difficulty: they REPLACE the game's for what they name. */
export interface MapDifficulty extends Extra { enemy?: DifficultyEnemy; xpPercent?: number; resourcePercent?: number }
export type MapDifficulties = Record<string, MapDifficulty>
