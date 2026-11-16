// The "Chance" row of a rule (docs/SRPG.md "CHANCE"): nothing (it always applies), a percent, a stat as a percent
// (`a.skl` or `a.skl * 2`, `a` = the unit that owns the skill), or a custom formula (Advanced). The mode is read from the
// file's value, so a formula written by hand that happens to have the simple shape shows as a stat.
import type { Extra } from './types'
import { patchBlock } from './skillBlocks'

export type ChanceMode = 'always' | 'percent' | 'stat' | 'custom'
const STAT_FORMULA = /^\s*a\.([A-Za-z_][A-Za-z0-9_]*)(?:\s*\*\s*(\d+))?\s*$/

export function chanceMode(chance: unknown, statIds: string[]): ChanceMode {
  if (chance === undefined) return 'always'
  if (typeof chance === 'number') return 'percent'
  if (typeof chance === 'string') { const m = STAT_FORMULA.exec(chance); return m && statIds.includes(m[1]) ? 'stat' : 'custom' }
  return 'custom'
}

/** `[stat, times]` of a stat chance. */
export function statChance(chance: unknown): [string, number] {
  const m = typeof chance === 'string' ? STAT_FORMULA.exec(chance) : null
  return m ? [m[1], m[2] === undefined ? 1 : Number(m[2])] : ['', 1]
}
export const statChanceText = (stat: string, times: number): string => (times === 1 ? `a.${stat}` : `a.${stat} * ${times}`)

/** A new mode starts from something sensible; `always` removes the key. */
export function setChanceMode(b: Extra, mode: ChanceMode, stats: string[]): Extra {
  switch (mode) {
    case 'always': return patchBlock(b, { chance: undefined })
    case 'percent': return patchBlock(b, { chance: 30 })
    case 'stat': return patchBlock(b, { chance: statChanceText(stats[0] ?? 'skl', 1) })
    case 'custom': return patchBlock(b, { chance: typeof b.chance === 'string' ? b.chance : 'a.level * 2' })
  }
}
export const setChance = (b: Extra, chance: number | string): Extra => patchBlock(b, { chance })
