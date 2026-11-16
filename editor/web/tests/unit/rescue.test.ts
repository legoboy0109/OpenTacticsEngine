// E3: carrying and pair up in the editor's model (docs/SRPG.md section 5 and "Editor"): the mode, who can carry whom as presets,
// the "while carrying" rows, the pair-up numbers, and what changing the mode throws away.
import { describe, expect, it } from 'vitest'
import {
  addCarrierRow, carrierRows, changeableStats, isFormulaWord, readCanCarry, readChange, removeCarrierRow, rescueMode, rescueModeLoss, rescueOf, setCanCarry,
  setCarrierRow, setRescueMode, setRulesPairBonus, writeCanCarry, writeChange,
} from '../../src/model/rescue'
import { rescueChecks } from '../../src/model/rescueChecks'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => {
  const d = loadDemo('demo-srpg')
  const { rescue: _r, ...rules } = d.game!.rules ?? {}
  return { ...d, game: { ...d.game!, rules } }
}
const stats = (d: ProjectDoc) => d.game!.stats.map((s) => s.id)

describe('rescue mode', () => {
  it('off writes no block; carry and pair up write only the mode; off again removes the lot', () => {
    let d = base()
    expect(rescueMode(d)).toBe('off')
    d = setRescueMode(d, 'carry')
    expect(rescueOf(d)).toEqual({ mode: 'carry' })
    d = setRescueMode(d, 'pairUp')
    expect(rescueOf(d)).toEqual({ mode: 'pairUp' })
    d = setCanCarry(d, 'a.str >= b.str')
    expect(setRescueMode(d, 'off').game!.rules).not.toHaveProperty('rescue')
  })
  it('leaving pair up drops the pair-up numbers (rules and unit types), keeping the rest; carry to pair up keeps all', () => {
    const s = changeableStats(base())[0].id
    let d = setRescueMode(base(), 'pairUp')
    d = setRulesPairBonus(d, s, 2)
    d = { ...d, units: d.units!.map((u, i) => (i === 0 ? { ...u, pairBonus: { [s]: 1 } } : u)) }
    d = setCanCarry(d, 'not b.tags.big')
    expect(rescueModeLoss(d, 'carry')).toBe(2)
    expect(rescueModeLoss(d, 'pairUp')).toBe(0)
    const c = setRescueMode(d, 'carry')
    expect(rescueOf(c)).toEqual({ mode: 'carry', canCarry: 'not b.tags.big' })
    expect(c.units![0]).not.toHaveProperty('pairBonus')
    expect(rescueChecks(c)).toEqual([])
    expect(rescueModeLoss(d, 'off')).toBe(3)
    const back = setRescueMode(c, 'pairUp')
    expect(rescueOf(back)).toEqual({ mode: 'pairUp', canCarry: 'not b.tags.big' })
  })
})

describe('who can carry whom', () => {
  it('presets read back from the formula they write; anything else is custom and kept exactly', () => {
    expect(readCanCarry(undefined)).toEqual({ kind: 'anyone' })
    expect(readCanCarry('')).toEqual({ kind: 'anyone' })
    const stat = { kind: 'stat' as const, mine: 'str', theirs: 'con' }
    expect(writeCanCarry(stat)).toBe('a.str >= b.con')
    expect(readCanCarry('a.str >= b.con')).toEqual(stat)
    const tag = { kind: 'tag' as const, tag: 'mounted' }
    expect(writeCanCarry(tag)).toBe('not b.tags.mounted')
    expect(readCanCarry('not b.tags.mounted')).toEqual(tag)
    const text = 'a.str >= b.str and not b.tags.mounted'
    expect(readCanCarry(text)).toEqual({ kind: 'custom', formula: text })
    expect(writeCanCarry({ kind: 'custom', formula: text })).toBe(text)
  })
  it('"anyone" removes the key; setting it with no block does nothing', () => {
    let d = setCanCarry(setRescueMode(base(), 'carry'), 'a.str >= b.str')
    expect(rescueOf(d)!.canCarry).toBe('a.str >= b.str')
    d = setCanCarry(d, undefined)
    expect(rescueOf(d)).toEqual({ mode: 'carry' })
    expect(setCanCarry(base(), 'x')).toEqual(base())
  })
  it('a tag is offered only when a formula can spell it', () => {
    expect(isFormulaWord('mounted')).toBe(true)
    expect(isFormulaWord('heavy armor')).toBe(false)
    expect(isFormulaWord('3d')).toBe(false)
  })
})

describe('while carrying', () => {
  it('halved, minus N, percent and plus N read and write the loader’s shapes', () => {
    expect(writeChange({ mode: 'halved', n: 50 })).toEqual({ percent: 50 })
    expect(writeChange({ mode: 'minus', n: 2 })).toBe(-2)
    expect(writeChange({ mode: 'plus', n: 2 })).toBe(2)
    expect(writeChange({ mode: 'percent', n: 5000 })).toEqual({ percent: 1000 })
    expect(readChange({ percent: 50 })).toEqual({ mode: 'halved', n: 50 })
    expect(readChange({ percent: 75 })).toEqual({ mode: 'percent', n: 75 })
    expect(readChange(-3)).toEqual({ mode: 'minus', n: 3 })
    expect(readChange(1)).toEqual({ mode: 'plus', n: 1 })
  })
  it('rows are added for stats not yet listed, never the health stat, changed in place and removed', () => {
    let d = setRescueMode(base(), 'carry')
    const hp = d.game!.roles.hp
    expect(changeableStats(d).map((s) => s.id)).not.toContain(hp)
    d = addCarrierRow(d)
    d = addCarrierRow(d)
    const rows = carrierRows(d)
    expect(rows).toHaveLength(2)
    expect(new Set(rows.map((r) => r.stat)).size).toBe(2)
    expect(rows.every((r) => r.stat !== hp && r.change.mode === 'halved')).toBe(true)
    d = setCarrierRow(d, 0, rows[0].stat, { mode: 'minus', n: 1 })
    expect(carrierRows(d)[0]).toEqual({ stat: rows[0].stat, change: { mode: 'minus', n: 1 } })
    expect(Object.keys(rescueOf(d)!.carrierStats!)).toEqual(rows.map((r) => r.stat))
    d = removeCarrierRow(removeCarrierRow(d, 1), 0)
    expect(rescueOf(d)).toEqual({ mode: 'carry' })
  })
  it('no row can be added once every stat that may change has one', () => {
    let d = setRescueMode(base(), 'carry')
    for (let i = 0; i < stats(d).length + 2; i++) d = addCarrierRow(d)
    expect(carrierRows(d)).toHaveLength(changeableStats(d).length)
  })
})

describe('the partner behind gives', () => {
  it('numbers are written per stat and an emptied table removes the key', () => {
    const s = changeableStats(base())[0].id
    let d = setRulesPairBonus(setRescueMode(base(), 'pairUp'), s, 2)
    expect(rescueOf(d)!.pairBonus).toEqual({ [s]: 2 })
    d = setRulesPairBonus(d, s, undefined)
    expect(rescueOf(d)).toEqual({ mode: 'pairUp' })
  })
  it('says the loader’s refusals in plain words', () => {
    const d = setRescueMode(base(), 'carry')
    const hp = d.game!.roles.hp
    const bad = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, rescue: { mode: 'carry' as const, carrierStats: { [hp]: -1, nope: -1 }, pairBonus: { [hp]: 1 } } } } }
    const text = rescueChecks(bad).map((c) => c.text).join('\n')
    expect(text).toMatch(/health stat cannot be changed/)
    expect(text).toMatch(/“nope” is not a stat/)
    expect(text).toMatch(/belongs to Pair up/)
    const unit = { ...d, units: d.units!.map((u, i) => (i === 0 ? { ...u, pairBonus: { [hp]: 1 } } : u)) }
    expect(rescueChecks(unit).map((c) => c.text).join('\n')).toMatch(/needs the game to be set to Pair up/)
  })
})
