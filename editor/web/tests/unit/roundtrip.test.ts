import { describe, expect, it } from 'vitest'
import { setFormulaPreset } from '../../src/model/formulas'
import { setFeature } from '../../src/model/rules'
import { removeStat, renameStat } from '../../src/model/stats'
import { updateTerrain } from '../../src/model/terrain'
import { updateUnit } from '../../src/model/units'
import { updateWeapon } from '../../src/model/weapons'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const MARK = '__keep'
function mark(doc: ProjectDoc): ProjectDoc {
  const m = <T extends object>(o: T): T => ({ ...o, [MARK]: 'x' })
  return {
    ...doc,
    game: m({ ...doc.game!, stats: doc.game!.stats.map(m), roles: m(doc.game!.roles), rules: m({ ...doc.game!.rules!, progression: doc.game!.rules!.progression && m(doc.game!.rules!.progression) }) }),
    terrain: doc.terrain!.map(m),
    weapons: doc.weapons!.map(m),
    units: doc.units!.map(m),
    maps: Object.fromEntries(Object.entries(doc.maps).map(([k, v]) => [k, m(v!)])),
  }
}

describe('unknown keys survive editing', () => {
  for (const name of ['demo-srpg', 'demo-economy'] as const) {
    it(`${name}: a JSON round trip changes nothing`, () => {
      const d = loadDemo(name)
      expect(JSON.parse(JSON.stringify(d))).toEqual(d)
    })

    it(`${name}: edits keep keys the UI does not know`, () => {
      let d = mark(loadDemo(name))
      d = renameStat(d, 'hp', 'Hit Points')
      d = updateUnit(d, d.units![0].id, { name: 'Renamed', tags: ['x'] })
      d = updateWeapon(d, d.weapons![0].id, { counter: false })
      d = updateTerrain(d, d.terrain![0].id, { color: '#123456' })
      d = setFeature(setFormulaPreset(d, 'damage', 'fixed'), 'progression', true)
      d = removeStat(d, d.game!.stats.find((s) => s.id !== 'hp' && s.id !== 'mov')?.id ?? 'zzz')
      const text = JSON.parse(JSON.stringify(d))
      expect(text.game[MARK]).toBe('x')
      expect(text.game.roles[MARK]).toBe('x')
      expect(text.game.rules[MARK]).toBe('x')
      expect(text.game.stats.every((s: any) => s[MARK] === 'x')).toBe(true)
      expect(text.units.every((u: any) => u[MARK] === 'x')).toBe(true)
      expect(text.weapons.every((w: any) => w[MARK] === 'x')).toBe(true)
      expect(text.terrain.every((t: any) => t[MARK] === 'x')).toBe(true)
      expect(Object.values(text.maps).every((m: any) => m[MARK] === 'x')).toBe(true)
    })
  }

  it('edits on a section that failed to load (null) do nothing', () => {
    const d: ProjectDoc = { ...loadDemo('demo-srpg'), units: null }
    expect(updateUnit(d, 'lord', { name: 'x' })).toBe(d)
  })
})
