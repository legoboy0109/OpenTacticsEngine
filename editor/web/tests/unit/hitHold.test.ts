// "hitHold" (docs/BATTLE_SCENE.md "Holding a pose"): turning it on and off, one key at a time, and the checks that mirror the loader.
import { describe, expect, it } from 'vitest'
import { hitHoldOf, sceneTimingProblems, setHitHold, setHitHoldOn } from '../../src/model/battleHold'
import type { ProjectDoc } from '../../src/model/types'

const doc = (scene?: Record<string, unknown>): ProjectDoc =>
  ({ game: {}, files: {}, graphics: { sheets: [], battle: { sets: {}, ...(scene ? { scene } : {}) } } } as unknown as ProjectDoc)

describe('hitHold', () => {
  it('turns on as an empty group, keeps the other scene keys, and goes again with an emptied scene', () => {
    let d = setHitHoldOn(doc(), true)
    expect(hitHoldOf(d)).toEqual({})
    d = setHitHold(d, 'perHpMs', 33.333)
    d = setHitHold(d, 'killMs', 816.667)
    expect(hitHoldOf(d)).toEqual({ perHpMs: 33.333, killMs: 816.667 })
    d = setHitHold(d, 'killMs', undefined)
    expect(hitHoldOf(d)).toEqual({ perHpMs: 33.333 })
    expect(sceneTimingProblems(d)).toEqual([])
    d = setHitHoldOn(d, false)
    expect(d.graphics!.battle!.scene).toBeUndefined()
    const kept = setHitHoldOn(setHitHoldOn(doc({ resultHoldMs: 550 }), true), false)
    expect(kept.graphics!.battle!.scene).toEqual({ resultHoldMs: 550 })
  })
  const cases: Array<[string, unknown, string]> = [
    ['not a group', 5, 'must be a group of settings'],
    ['an unknown key', { perHp: 5 }, 'does not know'],
    ['per point too big', { perHpMs: 1001 }, '“perHpMs” must be a number from 0 to 1000'],
    ['a negative kill time', { killMs: -1 }, '“killMs” must be a number from 0 to 10000'],
    ['a word', { minMs: 'long' }, '“minMs” must be a number'],
  ]
  for (const [name, hh, text] of cases) {
    it(`refuses ${name}`, () => {
      expect(sceneTimingProblems(doc({ hitHold: hh })).join('\n')).toContain(text)
    })
  }
  it('takes decimals', () => {
    expect(sceneTimingProblems(doc({ hitHold: { minMs: 566.667, baseMs: 50, perHpMs: 33.333, killMs: 816.667, noChangeMs: 183.333 } }))).toEqual([])
  })
})
