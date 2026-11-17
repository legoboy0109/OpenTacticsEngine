// Battle art follow-ups in the document (docs/BATTLE_SCENE.md): a side's own image, rules by weapon and side, the extra animation
// slots and the 512-frame limit, with the checks that mirror the game's loader.
import { describe, expect, it } from 'vitest'
import { ANIM_KEYS, HIT_ANIMS, MAX_FRAMES, addAssign, addSet, assignKey, framesFromRow, setAnim, setSideSheet, updateAssign, updateSideSheet, weaponChoices } from '../../src/model/battleArt'
import { battleChecks } from '../../src/model/battleChecks'
import { removeSheet } from '../../src/model/graphics'
import type { ProjectDoc } from '../../src/model/types'

const doc = (): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
  terrain: [], structures: null,
  weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'bow', name: 'Bow', group: 'bow' }],
  items: null, research: null, units: [{ id: 'knight', name: 'Knight' }], maps: { m: { sides: [{ id: 'blue', name: 'Blue' }, { id: 'red', name: 'Red' }] } }, scripts: {},
  graphics: {
    sheets: [
      { id: 'a', file: 'art/a.png', cellWidth: 48, cellHeight: 48 },
      { id: 'a_red', file: 'art/a_red.png', cellWidth: 48, cellHeight: 48 },
      { id: 'small', file: 'art/small.png', cellWidth: 32, cellHeight: 32 },
    ],
  },
  campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)

describe('the extra animation slots and the frame limit', () => {
  it('four new slots, after the ones that exist, with their hit frames and fall-backs', () => {
    for (const k of ['standRanged', 'critRanged', 'dodgeRanged', 'miss']) expect(ANIM_KEYS).toContain(k)
    expect(ANIM_KEYS).toHaveLength(12)
    expect(ANIM_KEYS[0]).toBe('stand')
    expect(HIT_ANIMS).toEqual(expect.arrayContaining(['miss', 'critRanged']))
    expect(HIT_ANIMS).not.toContain('standRanged')
    let d = addSet(doc(), 'k')
    d = setAnim(d, 'k', 'miss', { frames: framesFromRow(1, 3), hit: 2 })
    expect(d.graphics!.battle!.sets!.k.animations.miss).toEqual({ sheet: 'a', frames: [[0, 1], [1, 1], [2, 1]], hit: 2 })
  })
  it('an animation may list 512 frames', () => {
    expect(MAX_FRAMES).toBe(512)
    expect(framesFromRow(0, 100)).toHaveLength(100)
    expect(framesFromRow(0, 5000)).toHaveLength(512)
    const d = addSet(doc(), 'k')
    expect(battleChecks(setAnim(d, 'k', 'attack', { frames: framesFromRow(0, 512) })).filter((c) => c.blocks)).toEqual([])
    const over = setAnim(d, 'k', 'attack', { frames: Array.from({ length: 513 }, () => [0, 0] as [number, number]) })
    expect(battleChecks(over).some((c) => c.blocks && /513 frames.*512/.test(c.text))).toBe(true)
  })
})

describe('a different image for a side', () => {
  it('is a set-level "sides" entry, written only for a side that has one', () => {
    let d = addSet(doc(), 'k')
    d = updateSideSheet(d, 'k', 'red', 'a_red')
    expect(d.graphics!.battle!.sets!.k.sides).toEqual({ red: { sheet: 'a_red' } })
    expect(setSideSheet(d.graphics!.battle!.sets!.k, 'red')).toBe('a_red')
    expect(setSideSheet(d.graphics!.battle!.sets!.k, 'blue')).toBe('')
    d = updateSideSheet(d, 'k', 'red', '')
    expect('sides' in d.graphics!.battle!.sets!.k).toBe(false)
  })
  it('checks mirror the loader: unknown image, another layout, frames outside the side\'s image', () => {
    let d = addSet(doc(), 'k')
    d = updateSideSheet(d, 'k', 'red', 'a_red')
    expect(battleChecks(d).filter((c) => c.blocks)).toEqual([])
    expect(battleChecks(updateSideSheet(d, 'k', 'red', 'small')).some((c) => c.blocks && /pieces of 32 × 32.*48 × 48.*laid out the same way/.test(c.text))).toBe(true)
    const small = (f: string) => (f === 'art/a_red.png' ? { width: 96, height: 48 } : { width: 192, height: 192 })
    const wide = setAnim(d, 'k', 'attack', { frames: framesFromRow(1, 3) })
    expect(battleChecks(wide, small).some((c) => c.blocks && /smaller than the one it replaces/.test(c.text))).toBe(true)
    const gone = { ...d, graphics: { ...d.graphics!, battle: { ...d.graphics!.battle!, sets: { k: { ...d.graphics!.battle!.sets!.k, sides: { red: { sheet: 'nope' } } } } } } } as ProjectDoc
    expect(battleChecks(gone).some((c) => c.blocks && /image for the side “red” is not in the game/.test(c.text))).toBe(true)
  })
  it('an animation\'s own entry wins and is the one that is checked for that side', () => {
    let d = addSet(doc(), 'k')
    d = updateSideSheet(d, 'k', 'red', 'small')       // wrong layout at set level ...
    d = setAnim(d, 'k', 'stand', { sides: { red: { sheet: 'a_red' } } })   // ... but the stand has its own, right one
    expect(battleChecks(d).filter((c) => c.blocks)).toEqual([])
  })
  it('removing an image drops the side entries that use it, and keeps the set', () => {
    let d = addSet(doc(), 'k')
    d = updateSideSheet(d, 'k', 'red', 'a_red')
    d = setAnim(d, 'k', 'stand', { sides: { blue: { sheet: 'a_red' } } })
    const gone = removeSheet(d, 'a_red')
    const k = gone.graphics!.battle!.sets!.k
    expect(k.sides).toBeUndefined()
    expect(k.animations.stand.sides).toBeUndefined()
    expect(k.animations.stand.sheet).toBe('a')
  })
})

describe('rules by weapon and side', () => {
  it('a rule takes a weapon and a side, and the key tells two rules apart', () => {
    let d = addSet(doc(), 'k')
    d = addAssign(d, 'k', 'knight')
    d = updateAssign(d, 0, { weapon: 'sword', side: 'red' })
    expect(d.graphics!.battle!.assign![0]).toEqual({ set: 'k', type: 'knight', weapon: 'sword', side: 'red' })
    expect(assignKey({ set: 'k', type: 'knight', weapon: 'sword' })).not.toBe(assignKey({ set: 'k', type: 'knight', weapon: 'sword', side: 'red' }))
    expect(weaponChoices(d).map((w) => w.id)).toEqual(['sword', 'bow'])
    d = updateAssign(d, 0, { weapon: undefined, side: undefined })
    expect(d.graphics!.battle!.assign![0]).toEqual({ set: 'k', type: 'knight' })
  })
  it('checks: a rule with only a side is fine; a weapon with a group, an unknown weapon and a doubled rule are refused', () => {
    let d = addSet(doc(), 'k')
    const withRules = (rules: object[]) => ({ ...d, graphics: { ...d.graphics!, battle: { ...d.graphics!.battle!, assign: rules } } }) as ProjectDoc
    const blocks = (x: ProjectDoc) => battleChecks(x).filter((c) => c.blocks).map((c) => c.text)
    expect(blocks(withRules([{ set: 'k', side: 'red' }]))).toEqual([])
    expect(blocks(withRules([{ set: 'k', weapon: 'sword', group: 'blade' }])).join('|')).toMatch(/names a weapon and a weapon group/)
    expect(blocks(withRules([{ set: 'k', weapon: 'axe' }])).join('|')).toMatch(/weapon “axe”, which no longer exists/)
    expect(blocks(withRules([{ set: 'k' }])).join('|')).toMatch(/names no unit type, character, weapon, weapon group or side/)
    expect(blocks(withRules([{ set: 'k', type: 'knight', side: 'red' }, { set: 'k', type: 'knight', side: 'red' }])).join('|')).toMatch(/Rule 2 is the same as an earlier rule/)
    expect(blocks(withRules([{ set: 'k', type: 'knight', side: 'red' }, { set: 'k', type: 'knight', side: 'blue' }]))).toEqual([])
    d = withRules([])
  })
})
