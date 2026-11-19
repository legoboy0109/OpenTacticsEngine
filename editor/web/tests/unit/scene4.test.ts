// The fourth batch of opt-in battle scene keys in the editor (docs/BATTLE_SCENE.md): a range band's pan "backdrop" and "startMs", an effect's "atRange"
// and a travelling effect's "endOffset". The checks mirror the loader (wording is the editor's own plain words).
import { describe, expect, it } from 'vitest'
import {
  addEffectRange, delayAtDistance, effectProblems, effectRanges, setEffectRanges, setEndOffset, updateEffect,
} from '../../src/model/battleEffects'
import { enableStage } from '../../src/model/battleStage'
import { addBand, bandBackdrop, bandProblems, bandsOf, setBandBackdrop, setBandPanStart, setBandView } from '../../src/model/stageBands'
import type { BattleEffectDef, ProjectDoc } from '../../src/model/types'

const doc = (effects?: BattleEffectDef[], stage = false): ProjectDoc => {
  const d = {
    game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
    terrain: [{ id: 'plain', name: 'Plain' }], structures: null, research: null, maps: {}, scripts: {},
    weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'longbow', name: 'Longbow', group: 'bow' }],
    items: [], units: [{ id: 'knight', name: 'Knight' }],
    graphics: { sheets: [{ id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16 }], battle: effects ? { effects } : undefined },
    campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
  } as unknown as ProjectDoc
  return stage ? enableStage(d) : d
}
const fx = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => ({ weapon: 'longbow', sheet: 'fx', frames: [[0, 0]], ...more })
const arrow = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => fx({ mode: 'travel', ...more })
const problems = (e: BattleEffectDef) => effectProblems(doc([e]), 0)
const says = (list: string[], part: string) => list.some((t) => t.includes(part))

describe('atRange (delay by fight distance)', () => {
  it('adds steps from the first free distance, keeps them sorted, and an empty list is no key', () => {
    let d = doc([fx()])
    d = addEffectRange(d, 0)
    d = addEffectRange(d, 0)
    expect(effectRanges(d.graphics!.battle!.effects![0])).toEqual([{ from: 2, delayMs: 200 }, { from: 3, delayMs: 200 }])
    d = setEffectRanges(d, 0, [{ from: 4, delayMs: 400 }, { from: 2, delayMs: 267 }])
    expect(d.graphics!.battle!.effects![0].atRange).toEqual([{ from: 2, delayMs: 267 }, { from: 4, delayMs: 400 }])
    d = setEffectRanges(d, 0, [])
    expect('atRange' in d.graphics!.battle!.effects![0]).toBe(false)
  })
  it('works out the delay the game uses: the last step at or below the distance', () => {
    const e = fx({ atRange: [{ from: 4, delayMs: 400 }, { from: 2, delayMs: 267 }] })
    expect([1, 2, 3, 4, 9].map((n) => delayAtDistance(e, n))).toEqual([0, 267, 267, 400, 400])
    expect(delayAtDistance(fx(), 5)).toBe(0)
  })
  it('is accepted when right and refused with plain reasons otherwise', () => {
    expect(problems(fx({ atRange: [{ from: 2, delayMs: 267 }, { from: 4, delayMs: 0 }] }))).toEqual([])
    expect(problems(arrow({ atRange: [{ from: 2, delayMs: 100 }] }))).toEqual([])
    expect(says(problems(fx({ atRange: [] })), 'list of 1 to 16')).toBe(true)
    expect(says(problems(fx({ atRange: Array.from({ length: 17 }, (_, i) => ({ from: i + 1, delayMs: 1 })) })), 'list of 1 to 16')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 0, delayMs: 5 }] })), 'distance must be between 1 and 99')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 100, delayMs: 5 }] })), 'distance must be between 1 and 99')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 2, delayMs: -1 }] })), 'between 0 and 15000')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 2, delayMs: 15001 }] })), 'between 0 and 15000')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 2.5, delayMs: 5 }] })), 'distance must be between 1 and 99')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 2, delayMs: 5 }, { from: 2, delayMs: 9 }] })), 'another delay also starts at 2')).toBe(true)
  })
})

describe('endOffset (a thrown weapon stops short)', () => {
  it('is written as no key at 0 or empty, and kept otherwise', () => {
    let d = doc([arrow()])
    d = setEndOffset(d, 0, 24)
    expect(d.graphics!.battle!.effects![0].endOffset).toBe(24)
    expect('endOffset' in setEndOffset(d, 0, 0).graphics!.battle!.effects![0]).toBe(false)
    expect('endOffset' in setEndOffset(d, 0, undefined).graphics!.battle!.effects![0]).toBe(false)
    expect(updateEffect(d, 0, { endOffset: 5 }).graphics!.battle!.effects![0].endOffset).toBe(5)
  })
  it('is checked like the loader: flying effects only, -400 to 400, whole', () => {
    expect(problems(arrow({ endOffset: 400 }))).toEqual([])
    expect(problems(arrow({ endOffset: -400 }))).toEqual([])
    expect(problems(arrow({ endOffset: 0 }))).toEqual([])
    expect(says(problems(arrow({ endOffset: 401 })), 'between -400 and 400')).toBe(true)
    expect(says(problems(arrow({ endOffset: -401 })), 'between -400 and 400')).toBe(true)
    expect(says(problems(arrow({ endOffset: 1.5 })), 'between -400 and 400')).toBe(true)
    expect(says(problems(fx({ endOffset: 10 })), 'only for a flying effect')).toBe(true)
  })
})

describe('pan backdrop and startMs on a range band', () => {
  const eachBand = () => setBandView(addBand(doc(undefined, true)), 0, 'each')
  it('writes backdrop only for scroll and startMs only when set', () => {
    let d = eachBand()
    expect(bandBackdrop(bandsOf(d)[0])).toBe('fixed')
    d = setBandBackdrop(d, 0, 'scroll')
    expect(bandsOf(d)[0].pan!.backdrop).toBe('scroll')
    expect(bandBackdrop(bandsOf(d)[0])).toBe('scroll')
    d = setBandBackdrop(d, 0, 'fixed')
    expect('backdrop' in bandsOf(d)[0].pan!).toBe(false)
    d = setBandPanStart(d, 0, 400)
    expect(bandsOf(d)[0].pan!.startMs).toBe(400)
    expect(bandProblems(d)).toEqual([])
    d = setBandPanStart(d, 0, undefined)
    expect('startMs' in bandsOf(d)[0].pan!).toBe(false)
    // A band without a camera has no pan to put them in.
    const both = addBand(doc(undefined, true))
    expect(setBandBackdrop(both, 0, 'scroll')).toEqual(both)
    expect(setBandPanStart(both, 0, 5)).toEqual(both)
  })
  it('says what the loader would refuse', () => {
    const d = eachBand()
    const withPan = (p: Record<string, unknown>) => {
      const b = bandsOf(d)[0]
      return bandProblems({ ...d, graphics: { ...d.graphics!, battle: { ...d.graphics!.battle!, stage: { ...d.graphics!.battle!.stage!, ranges: [{ ...b, pan: { ...b.pan!, ...p } }] } } } } as ProjectDoc)
    }
    expect(withPan({ backdrop: 'scroll', startMs: 15000 })).toEqual([])
    expect(says(withPan({ startMs: -1 }), 'between 0 and 15000')).toBe(true)
    expect(says(withPan({ startMs: 15001 }), 'between 0 and 15000')).toBe(true)
    expect(says(withPan({ startMs: 2.5 }), 'between 0 and 15000')).toBe(true)
    expect(says(withPan({ backdrop: 'slide' }), 'fixed or scroll')).toBe(true)
  })
})
