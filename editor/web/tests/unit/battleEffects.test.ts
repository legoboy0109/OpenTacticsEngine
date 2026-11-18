import { describe, expect, it } from 'vitest'
import { battleChecks } from '../../src/model/battleChecks'
import {
  addEffect, effectChecks, effectFor, effectList, effectProblems, effectSounds, isRowRun, listable, nextSoundMs, removeEffect, renameByTypeKey,
  setByType, setEffectFor, setEffectIds, setEffectPicture, setEffectSound, setEffectTimed, updateEffect,
} from '../../src/model/battleEffects'
import type { BattleEffectDef, ProjectDoc } from '../../src/model/types'

const doc = (effects?: BattleEffectDef[]): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
  terrain: [{ id: 'plain', name: 'Plain' }], structures: null, items: null, research: null, maps: {}, scripts: {},
  weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'longbow', name: 'Longbow', group: 'bow' }],
  units: [{ id: 'knight', name: 'Knight' }, { id: 'archer', name: 'Archer' }],
  graphics: { sheets: [{ id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16 }], battle: effects ? { effects } : undefined },
  campaign: null, audio: { sounds: { zap: { files: ['audio/zap.wav'] }, boom: { files: ['audio/boom.wav'] } } }, characters: null, skills: null, abilities: null,
  conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)

const fx = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => ({ weapon: 'longbow', sheet: 'fx', frames: [[0, 0], [1, 0]], ...more })
const problems = (e: BattleEffectDef, extra: BattleEffectDef[] = []) => effectProblems(doc([e, ...extra]), 0)

describe('battle effects: editing', () => {
  it('a new effect is for the first weapon without one (then a library effect) and uses the first image', () => {
    let d = addEffect(doc())
    expect(effectList(d)[0]).toEqual({ weapon: 'sword', sheet: 'fx', frames: [[0, 0]] })
    d = addEffect(d)
    expect(effectList(d)[1].weapon).toBe('longbow')
    d = addEffect(d)
    expect(effectList(d)[2]).toMatchObject({ id: 'effect1', sheet: 'fx' })
    expect(effectFor(effectList(d)[2])).toBe('library')
    expect(addEffect({ ...doc(), graphics: { sheets: [] } } as ProjectDoc)).toEqual({ ...doc(), graphics: { sheets: [] } })  // no image, no effect
  })
  it('changes keys, writes no empty ones, follows a renamed id in lists and drops a removed one', () => {
    let d = doc([fx({ id: 'bolt' }), { weapon: 'sword', effects: ['bolt', 'bolt'], byType: { knight: 'bolt', archer: ['bolt'] } }])
    d = updateEffect(d, 0, { id: 'arc' })
    expect(effectList(d)[1]).toMatchObject({ effects: ['arc', 'arc'], byType: { knight: 'arc', archer: ['arc'] } })
    d = removeEffect(d, 0)
    expect(effectList(d)).toHaveLength(1)
    expect(effectList(d)[0].effects).toBeUndefined()
    expect(effectList(d)[0].byType).toEqual({ knight: [], archer: [] })  // (the type keeps its row: an empty list means no effect)
    d = removeEffect(d, 0)
    expect(d.graphics!.battle).toBeUndefined()
    expect(removeEffect(d, 5)).toBe(d)
  })
  it('who it plays for: a weapon, a group, or only lists (a name, no lists)', () => {
    let d = doc([{ weapon: 'sword', sheet: 'fx', frames: [[0, 0]], effects: 'x', byType: { knight: [] } }])
    d = setEffectFor(d, 0, 'group')
    expect(effectList(d)[0]).toMatchObject({ group: 'blade' })
    expect(effectList(d)[0].weapon).toBeUndefined()
    d = setEffectFor(d, 0, 'library')
    expect(effectList(d)[0]).toMatchObject({ id: 'effect1' })
    expect(effectList(d)[0].effects).toBeUndefined()
    expect(effectList(d)[0].byType).toBeUndefined()
    expect(effectList(d)[0].group).toBeUndefined()
    d = setEffectFor(d, 0, 'weapon')
    expect(effectList(d)[0]).toMatchObject({ weapon: 'sword', id: 'effect1' })
  })
  it('the picture is a run of frames along a row; several images are left alone until typed over', () => {
    let d = doc([fx({ ms: [10, 20] })])
    d = setEffectPicture(d, 0, { row: 2, count: 4 })
    expect(effectList(d)[0].frames).toEqual([[0, 2], [1, 2], [2, 2], [3, 2]])
    expect(effectList(d)[0].ms).toBeUndefined()
    expect(isRowRun(effectList(d)[0])).toBe(true)
    expect(isRowRun(fx({ frames: [[0, 0], [1, 0, 'fx2']] }))).toBe(false)
    expect(isRowRun(fx({ frames: [[0, 0], [2, 0]] }))).toBe(false)
  })
  it('sounds are rows of milliseconds and sound ids; the next free time starts at 0', () => {
    let d = doc([fx()])
    expect(nextSoundMs(effectList(d)[0])).toBe(0)
    d = setEffectSound(d, 0, 0, 'zap')
    d = setEffectSound(d, 0, 233, 'boom')
    expect(effectList(d)[0].sounds).toEqual({ '0': 'zap', '233': 'boom' })
    expect(effectSounds(effectList(d)[0])).toEqual([[0, 'zap'], [233, 'boom']])
    expect(nextSoundMs(effectList(d)[0])).toBe(100)
    d = setEffectSound(setEffectSound(d, 0, 0, undefined), 0, 233, undefined)
    expect(effectList(d)[0].sounds).toBeUndefined()
  })
  it('flash and shake lists, the also-plays list and by-type rows', () => {
    let d = doc([fx({ id: 'a' }), { id: 'b', sheet: 'fx', frames: [[0, 0]] }, { weapon: 'sword' }])
    d = setEffectTimed(d, 0, 'flash', [{ atMs: 100, ms: 80, color: '#ffffff' }])
    d = setEffectTimed(d, 0, 'shake', [{ atMs: 100, ms: 200, strength: 2 }])
    expect(effectList(d)[0]).toMatchObject({ flash: [{ atMs: 100, ms: 80, color: '#ffffff' }], shake: [{ atMs: 100, ms: 200, strength: 2 }] })
    d = setEffectTimed(setEffectTimed(d, 0, 'flash', []), 0, 'shake', [])
    expect(effectList(d)[0].flash).toBeUndefined()
    expect(effectList(d)[0].shake).toBeUndefined()
    expect(listable(d).map((l) => l.id)).toEqual(['a', 'b'])
    expect(listable(d, 0).map((l) => l.id)).toEqual(['b'])
    d = setEffectIds(d, 2, ['a'])
    expect(effectList(d)[2].effects).toBe('a')  // one id is written as a plain string
    d = setEffectIds(d, 2, ['a', 'b'])
    expect(effectList(d)[2].effects).toEqual(['a', 'b'])
    d = setEffectIds(d, 2, [])
    expect(effectList(d)[2].effects).toBeUndefined()
    d = setByType(d, 2, 'knight', ['a'])
    d = setByType(d, 2, 'archer', [])
    d = setByType(d, 2, 'archer', ['a', 'b'])
    expect(effectList(d)[2].byType).toEqual({ knight: 'a', archer: ['a', 'b'] })
    d = renameByTypeKey(d, 2, 'knight', 'archer')  // taken: nothing changes
    expect(effectList(d)[2].byType).toEqual({ knight: 'a', archer: ['a', 'b'] })
    d = setByType(setByType(d, 2, 'knight', null), 2, 'archer', null)
    expect(effectList(d)[2].byType).toBeUndefined()
  })
})

describe('battle effects: checks mirror the loader', () => {
  it('a clean effect has none, in the effect checks and in the art screen\'s list', () => {
    const d = doc([fx({ id: 'ok', sounds: { '0': 'zap' }, hitAtMs: 233, layer: 'behind', flash: [{ atMs: 0, ms: 80 }], shake: [{ atMs: 0, ms: 80, strength: 1 }], at: 'striker' })])
    expect(effectChecks(d)).toEqual([])
    expect(battleChecks(d).filter((c) => c.blocks)).toEqual([])
  })
  const cases: Array<[string, Partial<BattleEffectDef>, string]> = [
    ['hit time too big', { hitAtMs: 9000 }, 'hit time must be between 0 and 5000'],
    ['hit time on a flying effect', { mode: 'travel', hitAtMs: 100 }, 'a flying effect hits when it arrives'],
    ['returns when it does not fly', { returns: true }, 'only a flying effect can fly back'],
    ['stays when it does not fly', { lingerMs: 100 }, 'only a flying effect can stay'],
    ['stays too long', { mode: 'travel', lingerMs: 9000 }, 'between 0 and 5000'],
    ['a layer that is not one', { layer: 'above' as never }, 'front, behind or screen'],
    ['a flying screen effect', { mode: 'travel', layer: 'screen' }, 'cannot fly'],
    ['turn round off the screen layer', { mirror: true }, 'only for an effect that covers the screen'],
    ['striker and flying', { mode: 'travel', at: 'striker' }, 'cannot fly'],
    ['striker and screen', { layer: 'screen', at: 'striker' }, 'no place to appear at'],
    ['an unknown "at"', { at: 'sky' as never }, 'target or the striker'],
    ['a sound that is not in the game', { sounds: { '0': 'nowhere' } }, 'not in the game\'s sounds'],
    ['a sound at a bad time', { sounds: { soon: 'zap' } }, 'milliseconds from 0 to 60000'],
    ['a flash with a bad colour', { flash: [{ atMs: 0, ms: 80, color: 'red' }] }, 'colour must look like #ffffff'],
    ['a flash that is too short', { flash: [{ atMs: 0, ms: 5 }] }, 'flash 1: the length'],
    ['a shake with no strength', { shake: [{ atMs: 0, ms: 80, strength: 0 }] }, 'shake 1: the strength'],
    ['a list naming nothing', { effects: ['ghost'] }, 'which is not an effect'],
    ['by type for a unit that does not exist', { byType: { wizard: [] } }, 'no unit type “wizard”'],
    ['an image that is not there', { sheet: 'nowhere' }, 'image is not in the game'],
    ['a bad name', { id: 'a b' }, 'letters, digits'],
  ]
  for (const [name, more, text] of cases) {
    it(`${name}`, () => {
      expect(problems(fx(more)).join('\n')).toContain(text)
    })
  }
  it('duplicates, missing targets and lists on library effects', () => {
    expect(effectProblems(doc([fx(), fx()]), 1).join()).toContain('already an effect for longbow')
    expect(effectProblems(doc([fx({ id: 'a' }), fx({ weapon: undefined, id: 'a' })]), 1).join()).toContain('same name')
    expect(problems({ sheet: 'fx', frames: [[0, 0]] }).join()).toContain('name a weapon or a weapon group')
    expect(problems({ id: 'lib', sheet: 'fx', frames: [[0, 0]], effects: 'x' }).join()).toContain('belong to a weapon or a weapon group')
    expect(problems({ weapon: 'axe', sheet: 'fx', frames: [[0, 0]] }).join()).toContain('weapon “axe” does not exist')
    expect(problems({ group: 'whip', sheet: 'fx', frames: [[0, 0]] }).join()).toContain('no weapon has the group “whip”')
    // an assignment only names effects; a list naming one with no pictures is refused
    expect(effectProblems(doc([{ weapon: 'sword', effects: ['a'] }, fx({ weapon: 'longbow', id: 'a' })]), 0)).toEqual([])
    expect(effectProblems(doc([{ weapon: 'sword', effects: ['b'] }, { weapon: 'longbow', id: 'b', effects: ['a'] }, fx({ weapon: undefined, id: 'a' })]), 0).join()).toContain('no pictures of its own')
    expect(effectProblems(doc([{ weapon: 'sword' }]), 0).join()).toContain('needs an image and frames')
  })
  it('a frame outside the image is found when the image size is known', () => {
    const d = doc([fx({ frames: [[0, 0], [9, 0]] })])
    expect(effectProblems(d, 0, () => ({ width: 64, height: 16 })).join()).toContain('outside the image (it has 4 × 1 pieces)')
    expect(effectProblems(d, 0)).toEqual([])
  })
})
