// The fifth batch of opt-in battle scene keys in the editor (docs/BATTLE_SCENE.md "Holding a pose", "Platforms", "Effect timing by distance", "Range bands"):
// an animation's holdFrame / startFrame / startDelayMs and the scene's durations, game.json "battlePlatformFrom", an item's "healScene" sound, a travel
// effect's "endOffset" by distance, "atRangeFixed" and "panAtMs". The checks mirror the loader (wording is the editor's own plain words).
import { describe, expect, it } from 'vitest'
import { setAnim } from '../../src/model/battleArt'
import { battleChecks } from '../../src/model/battleChecks'
import {
  addEffectRange, effectProblems, effectRanges, endOffsetAtDistance, patchEffectRange, setEffectRangeFixed, setEffectTimed, setEndOffset, setPanAt, updateEffect,
} from '../../src/model/battleEffects'
import { RESULT_HOLD_DEFAULT, holdProblems, sceneTiming, sceneTimingProblems, setSceneTiming, usesHolds } from '../../src/model/battleHold'
import { sceneChoiceProblems } from '../../src/model/battleSceneChecks'
import { platformFrom, setPlatformFrom } from '../../src/model/battleTables'
import { ITEM_SOUND_KEYS, OVERRIDE_KEY_LABEL } from '../../src/model/audioLabels'
import { overrideOf, setOverride } from '../../src/model/audio'
import { audioChecks } from '../../src/model/audioChecks'
import type { BattleAnimDef, BattleEffectDef, ProjectDoc } from '../../src/model/types'

const frames = (n: number): Array<[number, number]> => Array.from({ length: n }, (_, i) => [i, 0] as [number, number])
const anim = (n: number, more: Partial<BattleAnimDef> = {}): BattleAnimDef => ({ sheet: 'fighters', frames: frames(n), ms: 100, ...more })

const doc = (animations: Record<string, BattleAnimDef> = {}, effects?: BattleEffectDef[], scene?: Record<string, unknown>): ProjectDoc =>
  ({
    game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
    terrain: [{ id: 'plain', name: 'Plain' }], structures: null, research: null, maps: {}, scripts: {},
    weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'longbow', name: 'Longbow', group: 'bow' }],
    items: [{ id: 'mend_staff', name: 'Mend Staff', kind: 'staff' }], units: [{ id: 'knight', name: 'Knight' }],
    graphics: {
      sheets: [{ id: 'fighters', file: 'art/f.png', cellWidth: 32, cellHeight: 32 }, { id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16 }],
      battle: { sets: { knight: { animations: { stand: anim(1), ...animations } } }, effects, scene },
    },
    campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
  }) as unknown as ProjectDoc
const says = (list: string[], part: string) => list.some((t) => t.includes(part))
const checks = (d: ProjectDoc) => battleChecks(d).map((c) => c.text)

describe('holding a pose: animation keys', () => {
  it('are written by setAnim and a frame number that no longer exists is pulled back to the last frame', () => {
    let d = doc({ dodge: anim(6) })
    d = setAnim(d, 'knight', 'dodge', { holdFrame: 3, startFrame: 2, startDelayMs: 80 })
    const a = d.graphics!.battle!.sets!.knight.animations.dodge
    expect([a.holdFrame, a.startFrame, a.startDelayMs]).toEqual([3, 2, 80])
    d = setAnim(d, 'knight', 'dodge', { frames: frames(3) })
    expect(d.graphics!.battle!.sets!.knight.animations.dodge.holdFrame).toBe(2)
    expect(d.graphics!.battle!.sets!.knight.animations.dodge.startFrame).toBe(2)
    d = setAnim(d, 'knight', 'dodge', { holdFrame: undefined, startFrame: undefined, startDelayMs: undefined })
    const clean = d.graphics!.battle!.sets!.knight.animations.dodge
    expect('holdFrame' in clean || 'startFrame' in clean || 'startDelayMs' in clean).toBe(false)
  })
  it('are accepted where the loader accepts them', () => {
    expect(holdProblems('k', 'attack', anim(4, { hit: 2, holdFrame: 3 }))).toEqual([])
    expect(holdProblems('k', 'attack', anim(4, { hit: 2, holdFrame: 2 }))).toEqual([])  // the hit frame itself
    expect(holdProblems('k', 'cast', anim(4, { holdFrame: 3 }))).toEqual([])           // (no hit given: the blow is on the last frame)
    expect(holdProblems('k', 'dodge', anim(6, { startFrame: 2, holdFrame: 3, startDelayMs: 80 }))).toEqual([])
    expect(holdProblems('k', 'dodgeRanged', anim(6, { startFrame: 0 }))).toEqual([])
    expect(holdProblems('k', 'dodge', anim(6, { holdFrame: 4, startDelayMs: 5000 }))).toEqual([])
    expect(holdProblems('k', 'hurt', anim(3, { holdFrame: 1 }))).toEqual([])
    expect(holdProblems('k', 'attack', anim(4))).toEqual([])
  })
  it('are refused with plain reasons otherwise', () => {
    expect(says(holdProblems('k', 'attack', anim(4, { holdFrame: 4 })), 'not one of its frames')).toBe(true)
    expect(says(holdProblems('k', 'attack', anim(4, { holdFrame: -1 })), 'not one of its frames')).toBe(true)
    expect(says(holdProblems('k', 'attack', anim(4, { holdFrame: 1.5 })), 'not one of its frames')).toBe(true)
    expect(says(holdProblems('k', 'dodge', anim(6, { startFrame: 6 })), 'not one of its frames')).toBe(true)
    expect(says(holdProblems('k', 'dodge', anim(6, { holdFrame: 2, startDelayMs: 5001 })), 'between 0 and 5000')).toBe(true)
    expect(says(holdProblems('k', 'dodge', anim(6, { startDelayMs: 10 })), 'neither a start frame nor a hold frame')).toBe(true)
    expect(says(holdProblems('k', 'attack', anim(4, { startFrame: 1 })), 'only the dodges')).toBe(true)
    expect(says(holdProblems('k', 'hurt', anim(3, { holdFrame: 1, startDelayMs: 10 })), 'only the dodges')).toBe(true)
    expect(says(holdProblems('k', 'dodge', anim(6, { startFrame: 4, holdFrame: 2 })), 'before its start frame')).toBe(true)
    expect(says(holdProblems('k', 'attack', anim(4, { hit: 3, holdFrame: 2 })), 'before the hit frame 4')).toBe(true)
    expect(says(holdProblems('k', 'attack', anim(6, { holdFrame: 2 })), 'before the hit frame 6')).toBe(true)  // (no hit given: the last frame)
    expect(says(holdProblems('k', 'defeat', anim(3, { holdFrame: 1 })), 'never waits')).toBe(true)
    expect(says(holdProblems('k', 'stand', anim(3, { holdFrame: 1 })), 'never waits')).toBe(true)
  })
  it('show up in the battle checks as blocking problems, and a game without them has none', () => {
    const bad = doc({ attack: anim(4, { hit: 3, holdFrame: 1 }) })
    expect(battleChecks(bad).some((c) => c.blocks && c.text.includes('before the hit frame'))).toBe(true)
    expect(checks(doc({ attack: anim(4, { hit: 2, holdFrame: 3 }), dodge: anim(6, { startFrame: 2, holdFrame: 3 }) })).filter((t) => t.includes('wait'))).toEqual([])
    expect(usesHolds(doc({ attack: anim(4, { hit: 2, holdFrame: 3 }) }))).toBe(true)
    expect(usesHolds(doc({ attack: anim(4, { hit: 2 }) }))).toBe(false)
  })
})

describe('holding a pose: the scene durations', () => {
  it('default to 533 and 0, are written only when different, and an emptied section goes', () => {
    let d = doc()
    expect(sceneTiming(d)).toEqual({ resultHoldMs: RESULT_HOLD_DEFAULT, resultHoldAgainMs: 0 })
    d = setSceneTiming(d, { resultHoldMs: 700 })
    expect(d.graphics!.battle!.scene).toEqual({ resultHoldMs: 700 })
    d = setSceneTiming(d, { resultHoldAgainMs: 120 })
    expect(d.graphics!.battle!.scene).toEqual({ resultHoldMs: 700, resultHoldAgainMs: 120 })
    expect(sceneTiming(d)).toEqual({ resultHoldMs: 700, resultHoldAgainMs: 120 })
    d = setSceneTiming(d, { resultHoldMs: 533, resultHoldAgainMs: 0 })
    expect('scene' in d.graphics!.battle!).toBe(false)
    // another key of the section is left alone
    const odd = doc({}, undefined, { resultHoldMs: 700, other: 1 })
    expect(setSceneTiming(odd, { resultHoldMs: 533 }).graphics!.battle!.scene).toEqual({ other: 1 })
  })
  it('are checked like the loader (0 to 5000)', () => {
    expect(sceneTimingProblems(doc({}, undefined, { resultHoldMs: 5000, resultHoldAgainMs: 0 }))).toEqual([])
    expect(says(sceneTimingProblems(doc({}, undefined, { resultHoldMs: 5001 })), 'between 0 and 5000')).toBe(true)
    expect(says(sceneTimingProblems(doc({}, undefined, { resultHoldAgainMs: -1 })), 'between 0 and 5000')).toBe(true)
    expect(says(sceneTimingProblems(doc({}, undefined, { resultHoldMs: 1.5 })), 'between 0 and 5000')).toBe(true)
    expect(sceneTimingProblems(doc())).toEqual([])
    expect(battleChecks(doc({}, undefined, { resultHoldMs: 9999 })).some((c) => c.blocks && c.text.includes('between 0 and 5000'))).toBe(true)
  })
})

describe('battlePlatformFrom (game.json)', () => {
  it('writes the key only for "own"', () => {
    const d = doc()
    expect(platformFrom(d)).toBe('same')
    const own = setPlatformFrom(d, 'own')
    expect(own.game!.battlePlatformFrom).toBe('own')
    expect(platformFrom(own)).toBe('own')
    const back = setPlatformFrom(own, 'same')
    expect('battlePlatformFrom' in back.game!).toBe(false)
    expect(setPlatformFrom(d, 'same')).toBe(d)
  })
  it('is checked, and a note says when it changes nothing', () => {
    const own = setPlatformFrom(doc(), 'own')
    expect(says(sceneChoiceProblems(own), 'no platforms yet')).toBe(true)
    const withPlat = { ...own, graphics: { ...own.graphics!, battle: { ...own.graphics!.battle!, platforms: { slab: { sheet: 'fx', cell: [0, 0] as [number, number] } } } } } as ProjectDoc
    expect(sceneChoiceProblems(withPlat).filter((t) => t.includes('platform'))).toEqual([])
    const bogus = { ...doc(), game: { ...doc().game!, battlePlatformFrom: 'both' as never } }
    expect(says(sceneChoiceProblems(bogus), 'must come from the same square')).toBe(true)
  })
})

describe('an item\'s healScene sound', () => {
  it('is an item sound key with its own label, written and cleared like the others', () => {
    expect(ITEM_SOUND_KEYS).toEqual(['use', 'heal', 'healScene'])
    expect(OVERRIDE_KEY_LABEL.healScene).toMatch(/battle scene/)
    let d = { ...doc(), items: [{ id: 'mend_staff', name: 'Mend Staff', kind: 'staff', sounds: { heal: 'chime' } }], audio: { sounds: { chime: { file: 'audio/c.wav' }, ping: { file: 'audio/p.wav' } } } } as unknown as ProjectDoc
    d = setOverride(d, 'items', 'mend_staff', 'healScene', 'ping')
    expect(overrideOf(d, 'items', 'mend_staff', 'healScene')).toBe('ping')
    expect(overrideOf(d, 'items', 'mend_staff', 'heal')).toBe('chime')  // "heal" keeps its meaning
    d = setOverride(d, 'items', 'mend_staff', 'healScene', '')  // silence in the scene on purpose
    expect(overrideOf(d, 'items', 'mend_staff', 'healScene')).toBe('')
    expect(audioChecks(d).filter((c) => /healScene|cannot have a sound/.test(c.text))).toEqual([])
    d = setOverride(d, 'items', 'mend_staff', 'healScene', undefined)
    expect('healScene' in (d.items![0].sounds as object)).toBe(false)
    // an unknown sound is still caught, and a unit type cannot have the key
    const ghost = setOverride(d, 'items', 'mend_staff', 'healScene', 'ghost')
    expect(audioChecks(ghost).some((c) => c.text.includes('ghost'))).toBe(true)
    expect(setOverride({ ...d, units: [{ id: 'knight', name: 'Knight' }] } as ProjectDoc, 'units', 'knight', 'healScene', 'ping').units![0]).toEqual({ id: 'knight', name: 'Knight' })
  })
})

const arrow = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => ({ weapon: 'longbow', sheet: 'fx', frames: [[0, 0]], mode: 'travel', ...more })
const fx = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => ({ weapon: 'sword', sheet: 'fx', frames: [[0, 0]], ...more })
const problems = (e: BattleEffectDef) => effectProblems(doc({}, [e]), 0)

describe('endOffset by distance', () => {
  it('works out the offset the game uses: the step that counts when it gives one, else the plain key', () => {
    const e = arrow({ endOffset: 10, atRange: [{ from: 3, endOffset: -40 }, { from: 5, delayMs: 100 }, { from: 7, delayMs: 50, endOffset: 0 }] })
    expect([1, 2, 3, 4, 5, 6, 7, 9].map((n) => endOffsetAtDistance(e, n))).toEqual([10, 10, -40, -40, 10, 10, 0, 0])
    expect(endOffsetAtDistance(arrow(), 4)).toBe(0)
  })
  it('patches one step and keeps the list sorted; an emptied number is no key', () => {
    let d = doc({}, [arrow()])
    d = addEffectRange(d, 0)
    d = patchEffectRange(d, 0, 0, { endOffset: 24 })
    expect(effectRanges(d.graphics!.battle!.effects![0])).toEqual([{ from: 2, delayMs: 200, endOffset: 24 }])
    d = patchEffectRange(d, 0, 0, { delayMs: undefined })
    expect(effectRanges(d.graphics!.battle!.effects![0])).toEqual([{ from: 2, endOffset: 24 }])
    d = patchEffectRange(d, 0, 0, { endOffset: undefined, delayMs: 50 })
    expect(effectRanges(d.graphics!.battle!.effects![0])).toEqual([{ from: 2, delayMs: 50 }])
    expect(patchEffectRange(d, 0, 5, { delayMs: 1 })).toBe(d)
  })
  it('is checked like the loader: a step needs a delay or an offset; offsets are for flights, -400 to 400', () => {
    expect(problems(arrow({ atRange: [{ from: 2, endOffset: 12 }, { from: 4, delayMs: 100, endOffset: -30 }] }))).toEqual([])
    expect(problems(arrow({ atRange: [{ from: 2, endOffset: 400 }, { from: 3, endOffset: -400 }] }))).toEqual([])
    expect(says(problems(arrow({ atRange: [{ from: 2 }] })), 'give a delay or')).toBe(true)
    expect(says(problems(arrow({ atRange: [{ from: 2, endOffset: 401 }] })), 'between -400 and 400')).toBe(true)
    expect(says(problems(arrow({ atRange: [{ from: 2, endOffset: -401 }] })), 'between -400 and 400')).toBe(true)
    expect(says(problems(arrow({ atRange: [{ from: 2, endOffset: 1.5 }] })), 'between -400 and 400')).toBe(true)
    expect(says(problems(fx({ atRange: [{ from: 2, delayMs: 5, endOffset: 5 }] })), 'only for a flying effect')).toBe(true)
    expect(says(problems(arrow({ atRange: [{ from: 2, delayMs: 15001 }] })), 'between 0 and 15000')).toBe(true)
    // the plain key widened to the same range
    expect(problems(arrow({ endOffset: -400 }))).toEqual([])
    expect(problems(arrow({ endOffset: 400 }))).toEqual([])
    expect(says(problems(arrow({ endOffset: 401 })), 'between -400 and 400')).toBe(true)
    expect(setEndOffset(doc({}, [arrow()]), 0, -40).graphics!.battle!.effects![0].endOffset).toBe(-40)
  })
})

describe('atRangeFixed', () => {
  it('is written on the effect and on a row, and only when on', () => {
    let d = doc({}, [fx({ fade: [{ atMs: 0, ms: 200 }] })])
    d = setEffectRangeFixed(d, 0, true)
    expect(d.graphics!.battle!.effects![0].atRangeFixed).toBe(true)
    d = setEffectRangeFixed(d, 0, false)
    expect('atRangeFixed' in d.graphics!.battle!.effects![0]).toBe(false)
    d = setEffectTimed(d, 0, 'fade', [{ atMs: 0, ms: 200, atRangeFixed: true }])
    expect(d.graphics!.battle!.effects![0].fade![0].atRangeFixed).toBe(true)
  })
  it('is checked: not together with the effect\'s own atRange, and a row\'s must be on or off', () => {
    expect(problems(fx({ atRangeFixed: true }))).toEqual([])
    expect(problems(fx({ atRange: [{ from: 2, delayMs: 300 }], fade: [{ atMs: 0, ms: 200, atRangeFixed: true }], flash: [{ atMs: 0, ms: 50, atRangeFixed: true }],
      shake: [{ atMs: 0, ms: 50, strength: 2, atRangeFixed: true }], hide: [{ atMs: 0, ms: 50, what: ['backdrop'], atRangeFixed: true }] }))).toEqual([])
    expect(says(problems(fx({ atRangeFixed: true, atRange: [{ from: 2, delayMs: 5 }] })), 'not delayed by distance')).toBe(true)
    expect(says(problems(fx({ atRangeFixed: 'yes' as never })), 'on or off')).toBe(true)
    expect(says(problems(fx({ fade: [{ atMs: 0, ms: 200, atRangeFixed: 1 as never }] })), 'fade 1: “not moved by distance” must be on or off')).toBe(true)
    expect(says(problems(fx({ flash: [{ atMs: 0, ms: 50, atRangeFixed: 'x' as never }] })), 'flash 1')).toBe(true)
    expect(says(problems(fx({ shake: [{ atMs: 0, ms: 50, strength: 2, atRangeFixed: 'x' as never }] })), 'shake 1')).toBe(true)
    expect(says(problems(fx({ hide: [{ atMs: 0, ms: 50, what: ['backdrop'], atRangeFixed: 'x' as never }] })), 'hiding 1')).toBe(true)
  })
})

describe('panAtMs', () => {
  it('is written as a number and cleared with no key', () => {
    let d = doc({}, [fx()])
    d = setPanAt(d, 0, 250)
    expect(d.graphics!.battle!.effects![0].panAtMs).toBe(250)
    d = setPanAt(d, 0, 0)
    expect(d.graphics!.battle!.effects![0].panAtMs).toBe(0)  // 0 is a real value: the camera leaves when the effect starts
    d = setPanAt(d, 0, undefined)
    expect('panAtMs' in d.graphics!.battle!.effects![0]).toBe(false)
    expect(updateEffect(d, 0, { panAtMs: 7 }).graphics!.battle!.effects![0].panAtMs).toBe(7)
  })
  it('is checked like the loader (0 to 15000, whole)', () => {
    expect(problems(fx({ panAtMs: 0 }))).toEqual([])
    expect(problems(fx({ panAtMs: 15000 }))).toEqual([])
    expect(says(problems(fx({ panAtMs: -1 })), 'between 0 and 15000')).toBe(true)
    expect(says(problems(fx({ panAtMs: 15001 })), 'between 0 and 15000')).toBe(true)
    expect(says(problems(fx({ panAtMs: 2.5 })), 'between 0 and 15000')).toBe(true)
  })
})
