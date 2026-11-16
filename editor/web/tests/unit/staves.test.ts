// Staves in the editor (docs/SRPG.md section 8): the Staff item kind, its edits, the follow-through shared with weapons (ranks, groups,
// character locks, features), and the checks. Uses the engine's fixture tests/fixtures/srpg-staves, which has every staff key.
import { describe, expect, it } from 'vitest'
import { groupsOf, removeRank, setGearRank, setRankId, switchOffRanks } from '../../src/model/ranks'
import { rankChecks } from '../../src/model/rankChecks'
import { lockChecks } from '../../src/model/lockChecks'
import { deleteItem, setItemKind, updateItem } from '../../src/model/items'
import { cleanAllLocks, gearLockedTo, lockCount, setGearWielders, locksShown } from '../../src/model/locks'
import { reachOf, setAreaMode, setHealKind, setHealNumber, setHit, setRadius, setReachFormula, setReachMax, setReachMin, setReachMode, setStaffGroup, setStaffProp, setStaffTarget, setStaffXp, healOf } from '../../src/model/staff'
import { staffChecks } from '../../src/model/staffChecks'
import { itemChecks } from '../../src/model/itemChecks'
import { deleteCharacter } from '../../src/model/srpgRefs'
import { switchOffFeature } from '../../src/model/srpgFeatures'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import type { ProjectDoc } from '../../src/model/types'
import { loadStavesFixture } from './fixtures'

const fx = (): ProjectDoc => loadStavesFixture()
const item = (d: ProjectDoc, id: string) => d.items!.find((i) => i.id === id)!

describe('the fixture', () => {
  it('has no editor-side problem and no loader-refused staff', () => {
    expect(staffChecks(fx())).toEqual([])
    expect(itemChecks(fx())).toEqual([])
    expect(rankChecks(fx())).toEqual([])
    expect(lockChecks(fx())).toEqual([])
  })
  it('lists the staff group after the weapons’', () => {
    expect(groupsOf(fx())).toEqual(['sword', 'staff'].filter((g) => groupsOf(fx()).includes(g)))
    expect(groupsOf(fx())).toContain('staff')
  })
  it('reads every reach shape', () => {
    expect(reachOf(item(fx(), 'mend_staff'))).toMatchObject({ mode: 'formula', text: '1 + a.mag / 3' })
    expect(reachOf(item(fx(), 'rally_staff'))).toMatchObject({ mode: 'upto', max: 3 })
    expect(reachOf(item(fx(), 'chorus_staff'))).toMatchObject({ mode: 'between', min: 1, max: 4 })
    expect(healOf(item(fx(), 'chorus_staff'))).toMatchObject({ kind: 'percent', value: 25 })
    expect(healOf(item(fx(), 'fortune_staff'))).toMatchObject({ kind: 'full' })
  })
})

describe('the edits', () => {
  it('switch reach modes and clamp to the loader’s limits', () => {
    let d = setReachMode(fx(), 'rally_staff', 'between')
    expect(item(d, 'rally_staff').range).toEqual([1, 3])
    d = setReachMin(d, 'rally_staff', 9)
    expect(item(d, 'rally_staff').range).toEqual([9, 9])
    d = setReachMax(d, 'rally_staff', 99)
    expect(item(d, 'rally_staff').range).toEqual([9, 20])
    d = setReachMode(d, 'rally_staff', 'formula')
    expect(typeof item(d, 'rally_staff').range).toBe('string')
    d = setReachFormula(d, 'rally_staff', 'a.mag')
    d = setReachMode(d, 'rally_staff', 'upto')
    expect(item(d, 'rally_staff').range).toBe(3)
  })
  it('"only its user" drops the reach and the area in the same step', () => {
    const d = setStaffTarget(fx(), 'rally_staff', 'self')
    expect(item(d, 'rally_staff')).not.toHaveProperty('range')
    expect(item(d, 'rally_staff')).not.toHaveProperty('area')
    expect(item(d, 'rally_staff').target).toBe('self')
    expect(setStaffTarget(d, 'rally_staff', 'ally').items!.find((i) => i.id === 'rally_staff')).not.toHaveProperty('target')
  })
  it('area modes', () => {
    let d = setAreaMode(fx(), 'mend_staff', 'radius')
    expect(item(d, 'mend_staff').area).toEqual({ radius: 1 })
    d = setRadius(d, 'mend_staff', 50)
    expect(item(d, 'mend_staff').area).toEqual({ radius: 10 })
    d = setAreaMode(d, 'mend_staff', 'all')
    expect(item(d, 'mend_staff').area).toBe('all')
    expect(item(setAreaMode(d, 'mend_staff', 'single'), 'mend_staff')).not.toHaveProperty('area')
  })
  it('healing changes the first effect and keeps the others', () => {
    const base = updateItem(fx(), 'mend_staff', { effects: [{ heal: 5 }, { warp: { range: 3 } }] })
    const d = setHealKind(base, 'mend_staff', 'percent')
    expect(item(d, 'mend_staff').effects).toEqual([{ heal: { percent: 25 } }, { warp: { range: 3 } }])
    expect(item(setHealNumber(d, 'mend_staff', 7), 'mend_staff').effects![0]).toEqual({ heal: 7 })
    expect(healOf(item(base, 'mend_staff')).more).toBe(1)
  })
  it('can miss on / off; experience; numbers', () => {
    let d = setHit(fx(), 'mend_staff', '70')
    expect(item(d, 'mend_staff').hit).toBe(70)
    d = setHit(d, 'mend_staff', '70 + a.skill - b.res')
    expect(item(d, 'mend_staff').hit).toBe('70 + a.skill - b.res')
    expect(item(setHit(d, 'mend_staff', undefined), 'mend_staff')).not.toHaveProperty('hit')
    expect(item(setStaffXp(fx(), 'mend_staff', ''), 'mend_staff')).not.toHaveProperty('xp')
    expect(item(setStaffXp(fx(), 'mend_staff', '20'), 'mend_staff').xp).toBe(20)
    expect(item(setStaffXp(fx(), 'mend_staff', 'a.level'), 'mend_staff').xp).toBe('a.level')
    expect(item(setStaffProp(fx(), 'mend_staff', 'power', 9), 'mend_staff').props).toEqual({ power: 9 })
    expect(item(setStaffProp(fx(), 'mend_staff', 'power', undefined), 'mend_staff')).not.toHaveProperty('props')
  })
  it('keeps keys and stage-2 effects the form does not know', () => {
    const odd = updateItem(fx(), 'mend_staff', { effects: [{ heal: 5 }, { swap: true }], mystery: 3 })
    const d = setReachMax(setHit(odd, 'mend_staff', '50'), 'mend_staff', 4)
    expect(item(d, 'mend_staff').mystery).toBe(3)
    expect(item(d, 'mend_staff').effects![1]).toEqual({ swap: true })
  })
})

describe('the kind', () => {
  it('a staff made from a usable item is valid; leaving removes every staff key in one undo step', () => {
    const d0 = fx()
    const usable = updateItem(d0, 'last_light', { kind: 'usable', heal: 5 })
    const asStaff = setItemKind(usable, 'last_light', 'staff')
    const s = item(asStaff, 'last_light')
    expect(s.kind).toBe('staff')
    expect(s.group).toBe('staff')
    expect(s.effects).toEqual([{ heal: 10 }])
    expect(s).not.toHaveProperty('heal')
    expect(staffChecks(asStaff).filter((c) => c.blocks)).toEqual([])
    const back = setItemKind(d0, 'rally_staff', 'usable')
    const u = item(back, 'rally_staff')
    for (const k of ['group', 'rank', 'rankXp', 'range', 'area', 'effects', 'target', 'hit', 'xp', 'wieldedBy', 'props', 'if']) expect(u).not.toHaveProperty(k)
    expect(u.heal).toBe(10)
    let h = initHistory(d0)
    h = pushEdit(h, back, undefined, 1)
    expect(item(undo(h).present, 'rally_staff').range).toBe(3)
  })
  it('the last staff of a group takes its starting ranks along', () => {
    let d = fx()
    for (const id of d.items!.filter((i) => i.kind === 'staff').map((i) => i.id)) d = deleteItem(d, id)
    expect(d.units!.find((u) => u.id === 'cleric')!).not.toHaveProperty('ranks')
    expect(d.characters!.find((c) => c.id === 'wen')!).not.toHaveProperty('ranks')
  })
  it('renaming the group of the only staves follows into classes and ranks when it is the only one; else leaves them', () => {
    let d = fx()
    for (const i of d.items!.filter((x) => x.kind === 'staff')) d = setStaffGroup(d, i.id, 'wand')
    expect(d.units!.find((u) => u.id === 'cleric')!.wields).toContain('staff')
    let single = fx()
    single = { ...single, items: single.items!.filter((i) => i.kind !== 'staff' || i.id === 'mend_staff') }
    single = setStaffGroup(single, 'mend_staff', 'wand')
    expect(single.units!.find((u) => u.id === 'cleric')!.wields).toEqual(['wand'])
    expect(single.units!.find((u) => u.id === 'cleric')!.ranks).toEqual({ wand: 'd' })
  })
})

describe('shared with weapons', () => {
  it('a rank id rename and a rank removal reach staves', () => {
    const renamed = setRankId(fx(), 'c', 'cc')
    expect(item(renamed, 'rally_staff').rank).toBe('cc')
    const removed = removeRank(fx(), 'b')
    expect(item(removed, 'high_staff')).not.toHaveProperty('rank')
  })
  it('ranks off removes rank and rankXp on staves', () => {
    const d = switchOffRanks(fx())
    for (const i of d.items!.filter((x) => x.kind === 'staff')) { expect(i).not.toHaveProperty('rank'); expect(i).not.toHaveProperty('rankXp') }
  })
  it('a locked staff: listed, counted, cleaned when the character goes or characters are switched off', () => {
    const d = fx()
    expect(locksShown(d, 'item')).toBe(true)
    expect(gearLockedTo(d, 'wen').map((g) => g.id)).toEqual(['kin_staff'])
    expect(lockCount(d)).toBe(1)
    expect(item(deleteCharacter(d, 'wen'), 'kin_staff')).not.toHaveProperty('wieldedBy')
    expect(item(cleanAllLocks(d), 'kin_staff')).not.toHaveProperty('wieldedBy')
    expect(item(switchOffFeature(d, 'characters'), 'kin_staff')).not.toHaveProperty('wieldedBy')
    expect(item(setGearWielders(d, 'item', 'mend_staff', ['tarn']), 'mend_staff').wieldedBy).toEqual(['tarn'])
    expect(item(setGearRank(d, 'item', 'mend_staff', 'e'), 'mend_staff').rank).toBe('e')
  })
  it('weapons carried or not does not hide a staff lock', () => {
    const fixed = { ...fx(), game: { ...fx().game!, rules: { ...fx().game!.rules, items: { weapons: 'fixed' as const } } } }
    expect(locksShown(fixed, 'item')).toBe(true)
    expect(locksShown(fixed, 'weapon')).toBe(false)
  })
})

describe('the checks', () => {
  const texts = (d: ProjectDoc) => staffChecks(d).map((c) => c.text)
  const blocking = (d: ProjectDoc) => itemChecks(d).map((c) => c.text)
  it('group missing', () => {
    expect(blocking(updateItem(fx(), 'mend_staff', { group: undefined })).join('\n')).toContain('needs a group')
  })
  it('reach outside the limits, min above max, empty formula, reach on a self staff', () => {
    expect(blocking(updateItem(fx(), 'mend_staff', { range: 21 })).join('\n')).toContain('from 1 to 20')
    expect(blocking(updateItem(fx(), 'mend_staff', { range: [4, 2] })).join('\n')).toContain('“from” cannot be above “to”')
    expect(blocking(updateItem(fx(), 'mend_staff', { range: [1, 30] })).join('\n')).toContain('up to 20')
    expect(blocking(updateItem(fx(), 'mend_staff', { range: '  ' })).join('\n')).toContain('formula is empty')
    expect(blocking(updateItem(fx(), 'rest_staff', { range: 2 })).join('\n')).toContain('takes no reach')
  })
  it('area radius outside the limits', () => {
    expect(blocking(updateItem(fx(), 'rally_staff', { area: { radius: 11 } })).join('\n')).toContain('from 1 to 10')
    expect(blocking(updateItem(fx(), 'rally_staff', { area: { radius: 0 } })).join('\n')).toContain('from 1 to 10')
  })
  it('healing a foe, a bad percentage, an empty chance', () => {
    expect(blocking(updateItem(fx(), 'mend_staff', { target: 'foe' })).join('\n')).toContain('cannot be used on foes')
    expect(blocking(updateItem(fx(), 'chorus_staff', { effects: [{ heal: { percent: 150 } }] })).join('\n')).toContain('1 to 100')
    expect(blocking(updateItem(fx(), 'fortune_staff', { hit: 130 })).join('\n')).toContain('0 to 100')
  })
  it('experience while the game has none', () => {
    const noXp = { ...fx(), game: { ...fx().game!, rules: { ...fx().game!.rules, progression: undefined } } }
    expect(blocking(noXp).join('\n')).toContain('no experience yet')
  })
  it('a rank when the game has no ladder; a lock when the game has no characters', () => {
    const noLadder = { ...fx(), ranks: null }
    expect(blocking(noLadder).join('\n')).toContain('no weapon ranks')
    expect(blocking({ ...fx(), characters: null }).join('\n')).toContain('locked to characters')
  })
  it('worth a look: no class can wield the group (not blocking)', () => {
    const d = { ...fx(), units: fx().units!.map((u) => ({ ...u, wields: u.wields?.filter((g) => g !== 'staff') })) }
    expect(texts(d).join('\n')).toContain('no class can wield its group')
    expect(staffChecks(d).every((c) => !c.blocks)).toBe(true)
    expect(blocking(d)).toEqual([])
  })
  it('rank mistakes on a staff come from the same rank checks as a weapon’s', () => {
    expect(rankChecks(setGearRank(fx(), 'item', 'mend_staff', 'zz')).map((c) => c.text).join('\n')).toContain('not in the ladder')
    expect(rankChecks(updateItem(fx(), 'mend_staff', { group: undefined, rank: 'c' })).map((c) => c.text).join('\n')).toContain('no group')
  })
  it('a lock nobody on the list can use', () => {
    const d = setGearWielders(fx(), 'item', 'mend_staff', ['tarn'])
    expect(lockChecks(d).map((c) => c.text).join('\n')).toContain('Nobody on the list can ever wield Mend Staff')
  })
  it('reach modes with the example are valid formulas for the game', () => {
    const d = setReachMode(setReachMode(fx(), 'rally_staff', 'formula'), 'rally_staff', 'formula')
    expect(String(item(d, 'rally_staff').range)).toMatch(/^1 \+ a\.\w+ \/ 3$/)
  })
})
