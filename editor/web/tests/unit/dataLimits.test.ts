// The web editor's number limits (src/model/dataLimits.ts) must not be looser than the loader's. The loader is C++, so the
// constants are read out of its source: a change there that tightens a range fails here until the editor follows.
import { readFileSync } from 'node:fs'
import path from 'node:path'
import { describe, expect, it } from 'vitest'
import { CHANCE_PERCENT, CRIT_MULTIPLIER_MAX, MOVE_COST_MAX, STAT_LIMIT, SUPPORT_BONUS_LIMIT } from '../../src/model/dataLimits'

const data = path.resolve(import.meta.dirname, '../../../../engine/core/data')
const src = (f: string): string => readFileSync(path.join(data, f), 'utf8')

describe('dataLimits match the loader', () => {
  it('stat numbers', () => {
    expect(Number(/kMaxStatNumber = (\d+)/.exec(src('loader_util.h'))![1])).toBe(STAT_LIMIT)
  })
  it('support bonuses', () => {
    expect(Number(/kMaxSupportBonus = (\d+)/.exec(src('loader_supports.cpp'))![1])).toBe(SUPPORT_BONUS_LIMIT)
  })
  it('skill chance percent', () => {
    expect(src('loader_skill_blocks.cpp')).toContain(`ch.asInt() >= ${CHANCE_PERCENT.min} && ch.asInt() <= ${CHANCE_PERCENT.max}`)
  })
  it('move costs (terrain and structures)', () => {
    expect(src('loader_content.cpp').split(`asInt() > ${MOVE_COST_MAX}`).length - 1).toBeGreaterThanOrEqual(2)
  })
  it('critical multiplier', () => {
    expect(src('loader.cpp')).toContain(`critMultiplier > ${CRIT_MULTIPLIER_MAX}`)
  })
})
