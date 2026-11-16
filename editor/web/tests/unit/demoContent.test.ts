// Step D1 of docs/SRPG.md: the real demo and the strategy-rpg template carry the strategy-RPG content (the other tests use the
// frozen pre-D1 copy of the demo, see fixtures.ts).
import { describe, expect, it } from 'vitest'
import { loadLiveDemoSrpg, loadTemplate } from './fixtures'

describe('strategy-RPG content', () => {
  it('the demo has its characters, skills, abilities and support pairs', () => {
    const d = loadLiveDemoSrpg()
    expect(d.characters?.map((c: any) => c.id)).toEqual(['eirik', 'kell', 'lyra', 'bram', 'sera', 'gorm', 'hald'])
    expect(d.skills).toHaveLength(33)
    expect(d.abilities).toHaveLength(5)
    expect(d.supports?.pairs).toHaveLength(4)
    expect((d.game as any).rules.rescue.mode).toBe('carry')
  })

  it('the template has the thirty starter skills, pair up and one support pair', () => {
    const t = loadTemplate('strategy-rpg')
    expect(t.skills).toHaveLength(30)
    expect((t.game as any).rules.rescue.mode).toBe('pairUp')
    expect(t.supports?.pairs).toHaveLength(1)
    expect(t.abilities?.map((a: any) => a.id)).toEqual(['shove'])
  })
})
