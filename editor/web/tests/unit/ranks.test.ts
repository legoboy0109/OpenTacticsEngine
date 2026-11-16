// Weapon ranks and stat caps (docs/SRPG.md section 7, editor step "as built"): the ladder edits, the pickers' edits, what deleting or
// renaming a rank, a weapon group or a stat does to everything that names it, switching the features on and off, the checks, and what
// goes over the wire.
import { describe, expect, it } from 'vitest'
import { buildBody } from '../../src/store/autosave'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import { rankChecks, capChecks } from '../../src/model/rankChecks'
import {
  DEFAULT_LADDER, addRank, canSwitchOnRanks, classCap, fillDefaultLadder, gameCap, groupsOf, moveRank, removeRank, resultingCap, setCapIn, setGameCap,
  setGrowth, setRankId, setRankIn, setRankXp, setWeaponRank, setWeaponRankXp, switchOffCaps, switchOffRanks, switchOnCaps, switchOnRanks, tidyLadder,
} from '../../src/model/ranks'
import { groupRankUsageLines, rankUsageLines } from '../../src/model/rankRefs'
import { removeStat } from '../../src/model/stats'
import { deleteWeapon, setWeaponGroup } from '../../src/model/weapons'
import { updateMapUnit } from '../../src/model/mapUnits'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo, loadRanksFixture, loadTemplate } from './fixtures'

const fx = (): ProjectDoc => loadRanksFixture()
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const ids = (d: ProjectDoc) => d.ranks!.ranks.map((r) => r.id)
/** The fixture with one placed unit that has its own rank. */
function withPlaced(d: ProjectDoc): ProjectDoc {
  const m = d.maps[mapId(d)]!
  const units = m.units!.map((u, i) => (i === 0 ? { ...u, ranks: { sword: 'c' } } : u))
  return { ...d, maps: { ...d.maps, [mapId(d)]: { ...m, units } } }
}

describe('the ladder', () => {
  it('starts empty and fills with the default ladder only when empty (E to S, first at 0, rising)', () => {
    const empty = switchOnRanks({ ...fx(), ranks: null })
    expect(empty.ranks).toEqual({ growth: 'fixed', ranks: [] })
    const filled = fillDefaultLadder(empty)
    expect(filled.ranks!.ranks).toEqual(DEFAULT_LADDER)
    expect(DEFAULT_LADDER.map((r) => r.name)).toEqual(['E', 'D', 'C', 'B', 'A', 'S'])
    expect(DEFAULT_LADDER.map((r) => r.xp)).toEqual([0, 30, 70, 120, 180, 250])
    expect(fillDefaultLadder(filled)).toBe(filled)
  })
  it('a new rank goes last, 50 above the last, with its own id; at most 12', () => {
    let d = addRank(fx())
    expect(d.ranks!.ranks.at(-1)).toEqual({ id: 'new_rank', name: 'New rank', xp: 62 })
    d = addRank(d)
    expect(ids(d).at(-1)).toBe('new_rank_2')
    for (let i = 0; i < 20; i++) d = addRank(d)
    expect(d.ranks!.ranks).toHaveLength(12)
  })
  it('experience: typing a number moves the ranks above it up; the first stays 0', () => {
    const d = setRankXp(fx(), 'd', 10)
    expect(d.ranks!.ranks.map((r) => r.xp)).toEqual([0, 10, 11, 12])
    const e = setRankXp(fx(), 'e', 5)
    expect(e.ranks!.ranks[0].xp).toBe(0)
    expect(tidyLadder([{ id: 'a', name: 'A', xp: 4 }, { id: 'b', name: 'B', xp: 4 }]).map((r) => r.xp)).toEqual([0, 4])
    expect(tidyLadder([{ id: 'a', name: 'A', xp: 0 }, { id: 'b', name: 'B', xp: 0 }]).map((r) => r.xp)).toEqual([0, 1])
  })
  it('moving a rank moves id and name; the experience numbers stay at their steps', () => {
    const d = moveRank(fx(), 0, 1)
    expect(ids(d)).toEqual(['d', 'e', 'c', 'b'])
    expect(d.ranks!.ranks.map((r) => r.xp)).toEqual([0, 3, 7, 12])
  })
  it('removing the first rank makes the next one the first (0 experience)', () => {
    const d = removeRank(fx(), 'e')
    expect(d.ranks!.ranks.map((r) => [r.id, r.xp])).toEqual([['d', 0], ['c', 7], ['b', 12]])
  })
  it('growth is stored as chosen', () => {
    expect(setGrowth(fx(), 'fixed').ranks!.growth).toBe('fixed')
    const same = fx()
    expect(setGrowth(same, 'use')).toBe(same) // already 'use': nothing changes
  })
  it('keeps keys it does not know', () => {
    const d = { ...fx(), ranks: { ...fx().ranks!, mystery: 1 } }
    expect(removeRank(setGrowth(d, 'fixed'), 'b').ranks).toMatchObject({ mystery: 1 })
  })
})

describe('a rank is renamed or removed', () => {
  it('an id change follows into weapons, classes, characters and placed units', () => {
    let d = withPlaced(fx())
    d = setRankId(d, 'd', 'second')
    expect(ids(d)).toEqual(['e', 'second', 'c', 'b'])
    expect(d.weapons!.find((w) => w.id === 'steel_sword')!.rank).toBe('second')
    expect(d.units!.find((u) => u.id === 'grunt')!.ranks).toEqual({ sword: 'second' })
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bow: 'second' })
    d = setRankId(d, 'c', 'third')
    expect(d.maps[mapId(d)]!.units![0].ranks).toEqual({ sword: 'third' })
  })
  it('an id that is taken, empty or has capitals is refused', () => {
    const d = fx()
    expect(setRankId(d, 'd', 'c')).toBe(d)
    expect(setRankId(d, 'd', 'D')).toBe(d)
    expect(setRankId(d, 'd', '')).toBe(d)
  })
  it('removing a rank: weapons need nothing, starting entries fall away, numbers stay, the rest is untouched', () => {
    let d = removeRank(withPlaced(fx()), 'c')
    expect(d.weapons!.find((w) => w.id === 'long_bow')!.rank).toBeUndefined()
    expect(d.units!.find((u) => u.id === 'knight')!.ranks).toBeUndefined()
    expect(d.units!.find((u) => u.id === 'fighter')!.ranks).toEqual({ sword: 'e' })
    expect(d.characters!.find((c) => c.id === 'kell')!.ranks).toEqual({ sword: 4 })
    expect(d.maps[mapId(d)]!.units![0]).not.toHaveProperty('ranks')
    expect(d.weapons!.find((w) => w.id === 'steel_sword')!.rank).toBe('d')
    d = removeRank(fx(), 'b')
    expect(d.weapons!.find((w) => w.id === 'war_sword')).not.toHaveProperty('rank')
  })
  it('the delete question lists who is affected', () => {
    const lines = rankUsageLines(withPlaced(fx()), 'c').map((l) => l.text)
    expect(lines.some((t) => t.includes('Long Bow'))).toBe(true)
    expect(lines.some((t) => t.includes('Knight'))).toBe(true)
    expect(lines.some((t) => t.includes('1 placed unit has'))).toBe(true)
  })
})

describe('weapon groups', () => {
  it('moving one of two weapons to another group is not a rename: nothing else follows', () => {
    const d = setWeaponGroup(fx(), 'short_bow', 'archery')
    expect(d.weapons!.find((w) => w.id === 'short_bow')!.group).toBe('archery')
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bow: 'd' })
    // Both weapons moved to a name that already exists: "bow" is gone, its entries are left for the check to name.
    const e = setWeaponGroup(setWeaponGroup(d, 'long_bow', 'archery'), 'short_bow', 'archery')
    expect(e.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bow: 'd' })
    expect(rankChecks(e).some((c) => c.text.includes('no weapon is in that group'))).toBe(true)
  })
  it('a group with a single weapon: typing a new name carries the ranks and the wielding class', () => {
    let d = fx()
    d = { ...d, weapons: d.weapons!.filter((w) => w.id !== 'long_bow') }
    d = setWeaponGroup(d, 'short_bow', 'bows')
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bows: 'd' })
    expect(d.units!.find((u) => u.id === 'archer')!.wields).toEqual(['bows'])
    d = setWeaponGroup(d, 'short_bow', 'bowss')
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bowss: 'd' })
  })
  it('clearing a group keeps the entries (the check names them) and nothing is lost while typing', () => {
    let d = fx()
    d = { ...d, weapons: d.weapons!.filter((w) => w.id !== 'long_bow') }
    d = setWeaponGroup(d, 'short_bow', '')
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bow: 'd' })
    expect(groupsOf(d)).not.toContain('bow')
    expect(rankChecks(d).some((c) => c.text.includes('no weapon is in that group'))).toBe(true)
  })
  it('deleting the last weapon of a group takes the ranks in it away; the question says so', () => {
    let d = fx()
    expect(groupRankUsageLines(d, 'bow').map((l) => l.text).join('|')).toContain('Archer')
    d = deleteWeapon(d, 'short_bow')
    expect(d.units!.find((u) => u.id === 'archer')!.ranks).toEqual({ bow: 'd' }) // long_bow remains
    d = deleteWeapon(d, 'long_bow')
    expect(d.units!.find((u) => u.id === 'archer')).not.toHaveProperty('ranks')
    expect(d.units!.find((u) => u.id === 'fighter')!.ranks).toEqual({ sword: 'e' })
  })
})

describe('pickers', () => {
  it('weapon rank and experience per use (1 is the default and is not written)', () => {
    let d = setWeaponRank(fx(), 'bronze_sword', 'c')
    expect(d.weapons![0].rank).toBe('c')
    d = setWeaponRank(d, 'bronze_sword', undefined)
    expect(d.weapons![0]).not.toHaveProperty('rank')
    d = setWeaponRankXp(d, 'bronze_sword', 4)
    expect(d.weapons![0].rankXp).toBe(4)
    expect(setWeaponRankXp(d, 'bronze_sword', 1).weapons![0]).not.toHaveProperty('rankXp')
  })
  it('a ranks map drops its key when "none" is chosen, and the map when empty', () => {
    expect(setRankIn({ sword: 'c' }, 'bow', 'd')).toEqual({ sword: 'c', bow: 'd' })
    expect(setRankIn({ sword: 'c' }, 'sword', undefined)).toBeUndefined()
    expect(setCapIn({ str: 3 }, 'str', undefined)).toBeUndefined()
  })
  it('a placed unit keeps its own rank in the file and loses it when set back to "its own"', () => {
    const d = fx()
    const m = d.maps[mapId(d)]!
    const a = updateMapUnit(m, m.units![0].x, m.units![0].y, { ranks: { sword: 'b' } })
    expect(a.units![0].ranks).toEqual({ sword: 'b' })
    const b = updateMapUnit(a, m.units![0].x, m.units![0].y, { ranks: undefined })
    expect(b.units![0]).not.toHaveProperty('ranks')
  })
})

describe('caps', () => {
  it('the cap of a character is the class cap (else the game cap) plus the adjustment, never below 0', () => {
    const d = fx()
    expect(gameCap(d, 'str')).toBe(10)
    expect(classCap(d, 'knight', 'str')).toBe(14)
    expect(classCap(d, 'fighter', 'str')).toBe(10)
    expect(classCap(d, 'fighter', 'def')).toBeUndefined()
    expect(resultingCap(14, 2)).toBe(16)
    expect(resultingCap(3, -9)).toBe(0)
    expect(resultingCap(undefined, 5)).toBeUndefined()
  })
  it('the game-wide cap is set and cleared per stat; empty keeps the file on', () => {
    let d = setGameCap(fx(), 'def', 20)
    expect(d.caps!.caps).toEqual({ str: 10, hp: 40, def: 20 })
    d = setGameCap(setGameCap(setGameCap(d, 'def', undefined), 'str', undefined), 'hp', undefined)
    expect(d.caps).toEqual({ caps: {} })
  })
  it('deleting a stat takes its caps and adjustments with it', () => {
    const d = removeStat(fx(), 'str')
    expect(d.caps!.caps).toEqual({ hp: 40 })
    expect(d.units!.find((u) => u.id === 'knight')).not.toHaveProperty('caps')
    expect(d.characters!.find((c) => c.id === 'mara')).not.toHaveProperty('capAdjust')
  })
})

describe('the two features', () => {
  const plain = (): ProjectDoc => ({ ...fx(), ranks: null, caps: null })
  it('switching on needs carried weapons for ranks; both create an empty section that stays', () => {
    const on = switchOnCaps(switchOnRanks(plain()))
    expect(on.ranks).toEqual({ growth: 'fixed', ranks: [] })
    expect(on.caps).toEqual({ caps: {} })
    const fixedWeapons = loadTemplate('strategy-rpg')
    expect(canSwitchOnRanks(fixedWeapons)).toBe(false)
    expect(switchOnRanks(fixedWeapons)).toBe(fixedWeapons)
  })
  it('the save body writes the empty sections, and false when they are removed (undo brings them back)', () => {
    const a = plain()
    const on = switchOnCaps(switchOnRanks(a))
    const body = buildBody(on, a, 'r', false)!
    expect(body.ranks).toEqual({ growth: 'fixed', ranks: [] })
    expect(body.caps).toEqual({ caps: {} })
    const start = fx()
    let h = initHistory(start)
    h = pushEdit(h, switchOffRanks(h.present), undefined, 0)
    expect(buildBody(h.present, start, 'r', false)!.ranks).toBe(false)
    h = undo(h)
    expect(h.present.ranks).toEqual(start.ranks)
    expect(buildBody(h.present, start, 'r', false)).toBeNull()
  })
  it('switching ranks off removes every rank on weapons, classes, characters and placed units', () => {
    const d = switchOffRanks(withPlaced(fx()))
    expect(d.ranks).toBeNull()
    expect(JSON.stringify(d.weapons)).not.toMatch(/"rank/)
    expect(JSON.stringify(d.units)).not.toMatch(/"ranks"/)
    expect(JSON.stringify(d.characters)).not.toMatch(/"ranks"/)
    expect(JSON.stringify(d.maps)).not.toMatch(/"ranks"/)
    expect(JSON.stringify(d.units)).toContain('"caps"') // caps are another feature
  })
  it('switching caps off removes class caps and adjustments', () => {
    const d = switchOffCaps(fx())
    expect(d.caps).toBeNull()
    expect(JSON.stringify(d.units)).not.toMatch(/"caps"/)
    expect(JSON.stringify(d.characters)).not.toMatch(/capAdjust/)
    expect(JSON.stringify(d.units)).toContain('"ranks"')
  })
  it('a game without the files is untouched by all of this', () => {
    const d = loadDemo('demo-srpg')
    expect(d.ranks).toBeNull()
    expect(switchOffRanks(d)).toBe(d)
    expect(rankChecks(d)).toEqual([])
    expect(capChecks(d)).toEqual([])
  })
})

describe('checks', () => {
  it('the fixture has none; a ladder no weapon asks for is worth a look', () => {
    expect(rankChecks(fx())).toEqual([])
    expect(capChecks(fx())).toEqual([])
    const none = { ...fx(), weapons: fx().weapons!.map(({ rank: _r, ...w }) => w) }
    expect(rankChecks(none).map((c) => c.text)).toContain('No weapon needs a rank yet, so ranks change nothing. Pick a required rank on a weapon (Weapons screen).')
  })
  it('names the mistakes the loader would refuse', () => {
    const d = fx()
    const texts = (x: ProjectDoc) => rankChecks(x).map((c) => c.text).join('\n')
    expect(texts({ ...d, weapons: d.weapons!.map((w) => (w.id === 'stick' ? { ...w, rank: 'd' } : w)) })).toContain('Stick needs a rank, but it has no group')
    expect(texts({ ...d, weapons: d.weapons!.map((w) => (w.id === 'stick' ? { ...w, rankXp: 2 } : w)) })).toContain('no group')
    expect(texts({ ...d, weapons: d.weapons!.map((w) => (w.id === 'war_sword' ? { ...w, rank: 'zz' } : w)) })).toContain('not in the ladder')
    expect(texts({ ...d, ranks: { ...d.ranks!, ranks: [{ id: 'e', name: 'E', xp: 5 }, { id: 'd', name: 'D', xp: 5 }] } })).toContain('needs 0 experience')
    expect(texts({ ...d, ranks: { ...d.ranks!, ranks: [{ id: 'e', name: 'E', xp: 0 }, { id: 'd', name: 'D', xp: 0 }] } })).toContain('Experience must rise')
    expect(texts({ ...d, units: d.units!.map((u) => (u.id === 'fighter' ? { ...u, ranks: { axe: 'e' } } : u)) })).toContain('no weapon is in that group')
    expect(texts({ ...d, units: d.units!.map((u) => (u.id === 'fighter' ? { ...u, ranks: { sword: 'q' } } : u)) })).toContain('not in the ladder')
    expect(texts({ ...d, units: d.units!.map((u) => (u.id === 'fighter' ? { ...u, ranks: { sword: 99 } } : u)) })).toContain('between 0 and 12')
    expect(capChecks({ ...d, caps: { caps: { str: 0, nope: 3 } } }).map((c) => c.text).join('\n')).toMatch(/must be from 1 to 99999[\s\S]*not a stat/)
  })
  it('weapons carried again as fixed make a ladder a problem', () => {
    const d = fx()
    const fixed = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, items: { ...d.game!.rules!.items, weapons: 'fixed' as const } } } }
    expect(rankChecks(fixed).map((c) => c.text).join()).toContain('Weapon ranks need weapons that units carry')
  })
})
