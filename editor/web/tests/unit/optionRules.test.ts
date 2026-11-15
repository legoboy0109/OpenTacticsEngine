// rules.research.needsStructure and rules.fog.ambush in the editor's model (docs/ECONOMY.md): defaults are never written,
// an emptied block disappears, unknown keys inside it are kept, and the checks mirror the loader.
import { describe, expect, it } from 'vitest'
import { economyChecks } from '../../src/model/economyChecks'
import { ambushRule, researchNeedsStructure, researchRuleSet, setAmbushRule, setResearchNeedsStructure } from '../../src/model/optionRules'
import { setResearchHere } from '../../src/model/structuresEconomy'
import type { ProjectDoc } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { loadPlainEconomy } from './fixtures'

const base = (): ProjectDoc => loadPlainEconomy()
const withRules = (d: ProjectDoc, extra: Record<string, unknown>): ProjectDoc =>
  ({ ...d, game: { ...d.game!, rules: { ...d.game!.rules, ...extra } } }) as ProjectDoc

describe('research needs a structure', () => {
  it('is off by default and writes nothing until switched on', () => {
    const d = base()
    expect(researchNeedsStructure(d)).toBe(false)
    expect(researchRuleSet(d)).toBe(false)
    expect(d.game!.rules).not.toHaveProperty('research')
    const on = setResearchNeedsStructure(d, true)
    expect(on.game!.rules!.research).toEqual({ needsStructure: true })
    expect(researchNeedsStructure(on)).toBe(true)
    expect(researchRuleSet(on)).toBe(true)
  })
  it('switching it off removes the block again (the default is not written)', () => {
    const d = setResearchNeedsStructure(setResearchNeedsStructure(base(), true), false)
    expect(d.game!.rules).not.toHaveProperty('research')
    expect(researchRuleSet(d)).toBe(false)
  })
  it('keeps keys it does not know and a block set to false by hand counts as set', () => {
    const d = withRules(base(), { research: { needsStructure: true, later: 1 } })
    const off = setResearchNeedsStructure(d, false)
    expect(off.game!.rules!.research).toEqual({ later: 1 })
    const hand = withRules(base(), { research: { needsStructure: false } })
    expect(researchRuleSet(hand)).toBe(true)
    expect(researchNeedsStructure(hand)).toBe(false)
  })
  it('a structure kind keeps research: true while the rule is off or on (nothing here removes it)', () => {
    let d = setResearchHere(base(), 'city', true)
    d = setResearchNeedsStructure(d, true)
    d = setResearchNeedsStructure(d, false)
    expect(d.structures!.find((s) => s.id === 'city')!.research).toBe(true)
  })
})

describe('ambush rule', () => {
  it('defaults to "endsTurn", is not written, and "canAct" writes the one key', () => {
    const d = base()
    expect(ambushRule(d)).toBe('endsTurn')
    expect(setAmbushRule(d, 'endsTurn').game!.rules).not.toHaveProperty('fog')
    const act = setAmbushRule(d, 'canAct')
    expect(act.game!.rules!.fog).toEqual({ ambush: 'canAct' })
    expect(ambushRule(act)).toBe('canAct')
    expect(setAmbushRule(act, 'endsTurn').game!.rules).not.toHaveProperty('fog')
  })
  it('keeps unknown keys inside the block', () => {
    const d = withRules(base(), { fog: { ambush: 'canAct', later: true } })
    expect(setAmbushRule(d, 'endsTurn').game!.rules!.fog).toEqual({ later: true })
  })
  it('the save body carries the rule', () => {
    const saved = base()
    const edited = setAmbushRule(setResearchNeedsStructure(saved, true), 'canAct')
    const body = buildBody(edited, saved, 'r', false)!
    expect((body.game as { rules: { research: unknown; fog: unknown } }).rules.research).toEqual({ needsStructure: true })
    expect((body.game as { rules: { fog: unknown } }).rules.fog).toEqual({ ambush: 'canAct' })
  })
})

describe('checks for the two rules', () => {
  const rules = (d: ProjectDoc) => economyChecks(d).filter((c) => c.section === 'rules').map((c) => c.text)
  it('say nothing for good values', () => {
    expect(rules(withRules(base(), { research: { needsStructure: true }, fog: { ambush: 'canAct' } }))).toEqual([])
  })
  it('name an unknown key, a non-boolean flag and an unknown ambush choice', () => {
    expect(rules(withRules(base(), { research: { needsBuilding: true } })).join(' ')).toMatch(/needsBuilding/)
    expect(rules(withRules(base(), { research: { needsStructure: 'yes' } })).join(' ')).toMatch(/on or off/)
    expect(rules(withRules(base(), { fog: { ambush: 'never' } })).join(' ')).toMatch(/endsTurn.*canAct.*never/)
    expect(rules(withRules(base(), { fog: { ambushes: 'canAct' } })).join(' ')).toMatch(/ambushes/)
  })
})
