// The scripted-style computer player in the editor (docs/AI.md "Scripted-style computer players"): rules.ai (turn order, weight sets), the new keys of a behaviour
// block (short form, mode switches, checks that mirror the loader), the patrol mode, and renames / deletes of a unit named in "ignores".
import { describe, expect, it } from 'vitest'
import { addWeightSet, aiRuleProblems, aiRules, removeWeightSet, setAttackAtZero, setPriority, setTurnOrder, setWeightNumber, weightSetUsers } from '../../src/model/aiRules'
import { behaviourChecks, behaviourMark, behaviourPhrase, contextOf, dropBehaviourUnit, renameBehaviourUnit, tidyBehaviour, withMode } from '../../src/model/behaviour'
import { defaultRoute } from '../../src/model/behaviourScripted'
import type { MapFile, MapUnit, ProjectDoc } from '../../src/model/types'
import { parseLoose } from './fixtures'
import { readFileSync } from 'node:fs'
import path from 'node:path'

// Uses the engine's fixture tests/fixtures/ai-scripted (game.json has rules.ai and a map that uses every key).
const fixture = (): ProjectDoc => {
  const root = path.resolve(import.meta.dirname, '../../../../tests/fixtures/ai-scripted')
  const read = (rel: string) => parseLoose(readFileSync(path.join(root, rel), 'utf8'))
  const game = read('game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(m.file)
  return { game, terrain: read('data/terrain.json').terrain, items: read('data/items.json').items, weapons: read('data/weapons.json').weapons, units: read('data/units.json').units, maps } as unknown as ProjectDoc
}
const withUnits = (d: ProjectDoc, f: (u: MapUnit[]) => MapUnit[]): ProjectDoc => ({ ...d, maps: { ...d.maps, field: { ...d.maps.field!, units: f(d.maps.field!.units!) } } })
const texts = (d: ProjectDoc) => behaviourChecks(d, 'field', d.maps.field!).map((c) => c.text)
const map = (d: ProjectDoc): MapFile => d.maps.field!

describe('rules.ai', () => {
  it('reads the fixture and writes nothing for the defaults', () => {
    const d = fixture()
    expect(aiRules(d).turnOrder).toBe('order')
    expect(Object.keys(aiRules(d).weights)).toEqual(['classic', 'brave'])
    expect(aiRuleProblems(d)).toEqual([])
    const best = setTurnOrder(d, 'best')
    expect(best.game!.rules!.ai!.turnOrder).toBeUndefined()
    const bare = removeWeightSet(removeWeightSet(best, 'classic'), 'brave')
    expect(bare.game!.rules!.ai).toBeUndefined()  // an empty block is removed
  })

  it('adds a set, writes only what differs from the default and drops a number that is set back to its default', () => {
    let d = addWeightSet(setTurnOrder(fixture(), 'best'), 'mine')
    expect(d.game!.rules!.ai!.weights!.mine).toEqual({})
    d = setWeightNumber(d, 'mine', 'dmg', 3)
    d = setWeightNumber(d, 'mine', 'dangerDiv', 8)  // the default: not written
    d = setWeightNumber(d, 'mine', 'killValue', 70)
    expect(d.game!.rules!.ai!.weights!.mine).toEqual({ dmg: 3, killValue: 70 })
    d = setWeightNumber(d, 'mine', 'dmg', 1)  // dmg defaults to 1
    expect(d.game!.rules!.ai!.weights!.mine).toEqual({ killValue: 70 })
    d = setAttackAtZero(d, 'mine', true)
    d = setPriority(d, 'mine', 'archer', 5)
    expect(d.game!.rules!.ai!.weights!.mine).toEqual({ killValue: 70, attackAtZero: true, priority: { archer: 5 } })
    d = setPriority(d, 'mine', 'archer', undefined)
    d = setAttackAtZero(d, 'mine', false)
    expect(d.game!.rules!.ai!.weights!.mine).toEqual({ killValue: 70 })
  })

  it('checks what the loader refuses, in plain words', () => {
    const d = fixture()
    const bad = (w: Record<string, unknown>): string[] => aiRuleProblems({ ...d, game: { ...d.game!, rules: { ...d.game!.rules, ai: { weights: { w } } } } } as ProjectDoc)
    expect(bad({ dmg: 101 })[0]).toContain('“dmg” must be a whole number from 0 to 100')
    expect(bad({ dmg: 1.5 })[0]).toContain('whole number')
    expect(bad({ dangerDiv: 0 })[0]).toContain('from 1 to 1000')
    expect(bad({ priority: { dragon: 3 } })[0]).toContain('unit type “dragon”')
    expect(bad({ priority: { archer: 5000 } })[0]).toContain('from -1000 to 1000')
    expect(bad({ attackAtZero: 1 })[0]).toContain('on or off')
    expect(bad({ class: 3 })[0]).toContain('class weight does nothing')
  })

  it('counts the behaviours that use a set', () => {
    const d = fixture()
    expect(weightSetUsers(d, 'classic')).toBe(2)  // b1 and r1
    expect(weightSetUsers(d, 'brave')).toBe(2)
    expect(weightSetUsers(d, 'nothing')).toBe(0)
  })
})

describe('the keys of a behaviour block', () => {
  it('writes only what is set, in the short form', () => {
    expect(tidyBehaviour({ order: 0, caution: 'normal', idleTurns: 0, ignores: [], weights: '' })).toBeUndefined()
    expect(tidyBehaviour({ order: -3 })).toEqual({ order: -3 })
    expect(tidyBehaviour({ mode: 'hold', retreat: { below: 30 } })).toEqual({ mode: 'hold', retreat: { below: 30 } })
    expect(tidyBehaviour({ caution: 'none', weights: 'classic', idleTurns: 2, ignores: ['lord'] })).toEqual({ weights: 'classic', caution: 'none', idleTurns: 2, ignores: ['lord'] })
    // route belongs to patrol, measure to guardArea: they do not survive in another mode
    expect(tidyBehaviour({ mode: 'hold', route: [{ x: 1, y: 1 }, { x: 2, y: 1 }], measure: 'moveCost' })).toBe('hold')
    expect(tidyBehaviour({ mode: 'patrol', route: [{ x: 1, y: 1 }, { x: 2, y: 1 }] })).toEqual({ mode: 'patrol', route: [{ x: 1, y: 1 }, { x: 2, y: 1 }] })
    expect(tidyBehaviour({ mode: 'guardArea', within: 3, measure: 'moveCost' })).toEqual({ mode: 'guardArea', within: 3, measure: 'moveCost' })
    expect(tidyBehaviour({ mode: 'guardArea', within: 3, measure: 'steps' })).toEqual({ mode: 'guardArea', within: 3 })
  })

  it('switching to patrol gives a route, and the scripted keys carry over (a route and a measure do not)', () => {
    const d = fixture()
    const ctx = contextOf(d, map(d))
    const p = withMode({ mode: 'guardArea', within: 3, measure: 'moveCost', order: 2, weights: 'brave' }, 'patrol', ctx)
    expect(p.mode).toBe('patrol')
    expect(p.order).toBe(2)
    expect(p.weights).toBe('brave')
    expect(p.measure).toBeUndefined()
    expect(Array.isArray(p.route) && (p.route as unknown[]).length).toBe(2)
    expect(defaultRoute(14, 6)[0]).not.toEqual(defaultRoute(14, 6)[1])
    expect(defaultRoute(1, 6)[0]).not.toEqual(defaultRoute(1, 6)[1])
  })

  it('has a mark, a sentence and no complaint for the fixture’s units', () => {
    expect(behaviourMark({ mode: 'patrol', route: [] })).toBe('P')
    expect(behaviourPhrase({ mode: 'patrol', route: [] })).toBe('walk their route')
    expect(texts(fixture())).toEqual([])
  })

  it('checks every key the way the loader does', () => {
    const d = fixture()
    const one = (b: unknown): string[] => texts(withUnits(d, (u) => u.map((x) => (x.id === 'r5' ? { ...x, behaviour: b as MapUnit['behaviour'] } : x))))
    const has = (b: unknown, part: string) => expect(one(b).join('\n')).toContain(part)
    has({ order: 100001 }, 'from -100000 to 100000')
    has({ weights: 'ghost' }, 'attack weights “ghost”')
    has({ caution: 'reckless' }, '“caution” must be normal or none')
    has({ idleTurns: 100 }, 'from 1 to 99')
    has({ retreat: { until: 80 } }, 'retreat limit')
    has({ retreat: { below: 50, until: 50 } }, 'must be more than')
    has({ retreat: { below: 30, to: { terrain: ['lava'] } } }, 'terrain “lava”')
    has({ retreat: { below: 30, to: { terrain: [] } } }, 'at least one kind of terrain')
    has({ mode: 'patrol' }, 'a route needs 2 to 32 squares')
    has({ mode: 'patrol', route: [{ x: 3, y: 1 }] }, 'a route needs 2 to 32 squares')
    has({ mode: 'patrol', route: [{ x: 3, y: 1 }, { x: 3, y: 1 }] }, 'same square twice in a row')
    has({ mode: 'patrol', route: [{ x: 3, y: 1 }, { x: 40, y: 1 }] }, 'outside the map')
    has({ mode: 'hold', route: [{ x: 3, y: 1 }, { x: 4, y: 1 }] }, 'goes with “walks a route”')
    has({ ignores: ['ghost'] }, 'never chases “ghost”')
    has({ measure: 'moveCost' }, '“measured by” goes with')
    has({ mode: 'guardArea', within: 3, measure: 'miles' }, 'squares or walking cost')
    // A number that does nothing yet is only a note, until the game plays one unit at a time.
    const noOrder = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, ai: { ...d.game!.rules!.ai, turnOrder: 'best' } } } } as ProjectDoc
    expect(texts(withUnits(noOrder, (u) => u.map((x) => (x.id === 'r5' ? { ...x, behaviour: { order: 2 } } : x)))).join('\n')).toContain('plays its best action first')
  })

  it('follows a renamed or deleted unit named in "ignores"', () => {
    const d = fixture()
    const m = renameBehaviourUnit(map(d), 'r3', 'raider')
    expect((m.units!.find((u) => u.id === 'b3')!.behaviour as { ignores: string[] }).ignores).toEqual(['raider'])
    const gone = dropBehaviourUnit(map(d), 'r3')
    expect((gone.units!.find((u) => u.id === 'b3')!.behaviour as { idleTurns: number; ignores?: string[] })).toEqual({ order: 3, idleTurns: 2 })
  })
})
