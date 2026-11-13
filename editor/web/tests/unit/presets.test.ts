import { describe, expect, it } from 'vitest'
import {
  attentionList, formulaView, formulasUsingStat, setFormulaExpression, setFormulaParam, setFormulaPreset,
} from '../../src/model/formulas'
import { FORMULA_DEFS, FORMULA_KEYS } from '../../src/model/presetDefs'
import { CUSTOM, canon, detect, fillParams, generate, summarize } from '../../src/model/presets'
import { removeStat } from '../../src/model/stats'
import { loadDemo } from './fixtures'

const srpg = loadDemo('demo-srpg')
const ctx = { stats: srpg.game!.stats, roles: srpg.game!.roles }

describe('presets', () => {
  it('has the documented presets', () => {
    expect(FORMULA_DEFS.damage.presets.map((p) => p.id)).toEqual(['attack-minus-defense', 'damage-chart', 'fixed'])
    expect(FORMULA_KEYS).toHaveLength(7)
  })

  for (const key of FORMULA_KEYS) {
    for (const preset of FORMULA_DEFS[key].presets) {
      it(`${key}/${preset.id}: generate -> detect round trip, with a summary`, () => {
        const params = fillParams(preset, undefined, ctx)
        const expr = generate(preset, params, ctx)
        const found = detect(FORMULA_DEFS[key], expr ?? undefined, undefined, ctx)
        expect(found.preset).toBe(preset.id)
        expect(summarize(preset, found.params, ctx).length).toBeGreaterThan(8)
        if (expr !== null) expect(canon(generate(preset, found.params, ctx)!)).toBe(canon(expr))
      })
    }
  }

  it('every toggle combination of attack-minus-defense round-trips', () => {
    const preset = FORMULA_DEFS.damage.presets[0]
    for (const weapon of [true, false]) for (const eff of [true, false]) for (const terrain of [true, false]) {
      const params = fillParams(preset, { weapon, eff, terrain, A: 'str', D: 'def' }, ctx)
      const found = detect(FORMULA_DEFS.damage, generate(preset, params, ctx)!, undefined, ctx)
      expect(found.preset).toBe('attack-minus-defense')
      expect(found.params).toMatchObject({ weapon, eff, terrain, A: 'str', D: 'def' })
    }
  })

  it('generates the documented expressions', () => {
    const p = FORMULA_DEFS.damage.presets[0]
    expect(generate(p, { A: 'str', D: 'def', weapon: true, eff: true, terrain: true }, ctx)).toBe('(a.str + (w.might or 0) * vs) - b.def - (tb.defense or 0)')
    expect(generate(p, { A: 'str', D: 'def', weapon: false, eff: true, terrain: false }, ctx)).toBe('(a.str * vs) - b.def')
    expect(generate(FORMULA_DEFS.damage.presets[1], { terrain: true }, ctx)).toBe('vs * (a.hp / a.maxhp) * (1 - 0.1 * (tb.defense or 0))')
    expect(generate(FORMULA_DEFS.hit.presets[2], { S: 'skl', P: 'spd', terrain: true }, ctx)).toBe('(w.hit or 100) + a.skl * 2 - b.spd * 2 - (tb.avoid or 0)')
    expect(generate(FORMULA_DEFS.strikes.presets[1], { P: 'spd', N: 4 }, ctx)).toBe('(a.spd - b.spd >= 4) and 2 or 1')
    expect(generate(FORMULA_DEFS.capture.presets[0], {}, ctx)).toBe('9999')
    expect(generate(FORMULA_DEFS.hit.presets[0], {}, ctx)).toBeNull()
  })

  it('recognises the hand-written strategy-rpg style formulas', () => {
    const d = detect(FORMULA_DEFS.damage, '(a.str + w.might) - b.def - (tb.defense or 0)', undefined, ctx)
    expect(d.preset).toBe('attack-minus-defense')
    expect(detect(FORMULA_DEFS.hit, 'w.hit + a.skl * 2 - b.spd * 2 - (tb.avoid or 0)', undefined, ctx).preset).toBe('accuracy-vs-evasion')
    expect(detect(FORMULA_DEFS.crit, 'a.skl / 2', undefined, ctx).preset).toBe('stat')
    expect(detect(FORMULA_DEFS.xpHit, '10 + math.max(0, b.level - a.level) * 3', undefined, ctx).preset).toBe('stronger')
  })

  it('missing keys mean the "off" presets or the engine defaults', () => {
    expect(detect(FORMULA_DEFS.hit, undefined, undefined, ctx).preset).toBe('always')
    expect(detect(FORMULA_DEFS.crit, undefined, undefined, ctx).preset).toBe('none')
    expect(detect(FORMULA_DEFS.strikes, undefined, undefined, ctx).preset).toBe('one')
    expect(detect(FORMULA_DEFS.xpKill, undefined, undefined, ctx)).toMatchObject({ preset: 'fixed', params: { N: 30 } })
    expect(detect(FORMULA_DEFS.capture, 'a.hp', undefined, ctx)).toMatchObject({ preset: 'health', params: { K: 1 } })
  })

  it('anything else is Custom and is never overwritten', () => {
    expect(detect(FORMULA_DEFS.damage, 'srpg_damage(a, b, w, tb, vs)', undefined, ctx).preset).toBe(CUSTOM)
    expect(detect(FORMULA_DEFS.damage, 'a.nonexistent - b.def', undefined, ctx).preset).toBe(CUSTOM)
    const view = formulaView(srpg.game!, 'damage')
    expect(view.preset).toBe(CUSTOM)
    expect(view.expr).toBe('srpg_damage(a, b, w, tb, vs)')
  })

  it('prefers the stored preset when two would produce the same text', () => {
    const d = detect(FORMULA_DEFS.capture, '9999', { preset: 'fixed', params: { N: 9999 } }, ctx)
    expect(d.preset).toBe('fixed')
    expect(detect(FORMULA_DEFS.capture, '9999', undefined, ctx).preset).toBe('instant')
  })
})

describe('applying presets to the document', () => {
  it('writes the expression and remembers the choice under game.editor', () => {
    const next = setFormulaPreset(srpg, 'damage', 'fixed')
    expect(next.game!.formulas!.damage).toBe('5')
    expect(next.game!.editor!.formulas!.damage).toMatchObject({ preset: 'fixed' })
    expect(formulaView(next.game!, 'damage').preset).toBe('fixed')
    expect(setFormulaParam(next, 'damage', 'N', 7).game!.formulas!.damage).toBe('7')
  })

  it('removes the key for "off" presets and writes it back when turned on', () => {
    const off = setFormulaPreset(srpg, 'hit', 'always')
    expect('hit' in off.game!.formulas!).toBe(false)
    const on = setFormulaPreset(off, 'hit', 'accuracy')
    expect(on.game!.formulas!.hit).toBe('(w.hit or 100)')
  })

  it('carries parameters over between presets and custom text switches to Custom', () => {
    const a = setFormulaPreset(setFormulaPreset(srpg, 'crit', 'stat'), 'crit', 'fixed')
    expect(a.game!.formulas!.crit).toBe('5')
    const c = setFormulaExpression(a, 'crit', 'a.skl / 3 + 1')
    expect(formulaView(c.game!, 'crit').preset).toBe(CUSTOM)
    expect(c.game!.formulas!.crit).toBe('a.skl / 3 + 1')
  })

  it('deleting a stat a preset uses blanks that choice, keeps the game loadable, and flags it', () => {
    let doc = setFormulaPreset(srpg, 'damage', 'attack-minus-defense')
    doc = setFormulaParam(doc, 'damage', 'A', 'str')
    expect(formulasUsingStat(doc.game!, 'str')).toContain('damage')
    const after = removeStat(doc, 'str')
    expect(after.game!.stats.some((s) => s.id === 'str')).toBe(false)
    expect(after.game!.formulas!.damage).not.toContain('str')
    expect(after.game!.formulas!.damage).toMatch(/^\(0 \+/)
    expect(attentionList(after.game!).map((a) => a.key)).toContain('damage')
    expect(formulaView(after.game!, 'damage').missing).toEqual(['Attack stat'])
    // Choosing a stat again clears the warning.
    const fixed = setFormulaParam(after, 'damage', 'A', 'skl')
    expect(attentionList(fixed.game!).map((a) => a.key)).not.toContain('damage')
  })
})
