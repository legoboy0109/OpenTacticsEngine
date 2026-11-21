// Ranges the engine's loader enforces on numbers (engine/core/data/loader*.cpp, docs/GAME_FORMAT.md "Strict files").
// The number boxes use these so that a value the game would refuse to load cannot be typed. The loader is the authority;
// a box may be stricter than these (a sensible limit of its own) but never looser.

/** A stat, growth or stat bonus: unit types, characters, items, jobs, ranks, promotion gains, pair bonuses, research, doors, map units. */
export const STAT_LIMIT = 99999
/** A support level's bonus: combat numbers and `stats`, plus or minus. */
export const SUPPORT_BONUS_LIMIT = 1000
/** A skill rule's `chance` as a percent. */
export const CHANCE_PERCENT = { min: 0, max: 100 } as const
/** A move cost (0 = cannot enter). */
export const MOVE_COST_MAX = 9999
/** `formulas.critMultiplier`. */
export const CRIT_MULTIPLIER_MAX = 1000
