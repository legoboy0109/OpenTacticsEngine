import { describe, expect, it } from 'vitest'
import { findUsages } from '../../src/model/references'
import { addMap, deleteMap, duplicateMap, renameMap } from '../../src/model/maps'
import { addMoveType, addResource, removeMoveType, removeResource } from '../../src/model/rulesLists'
import { setDefeatCondition, setFeature, setFeatureField } from '../../src/model/rules'
import { addStat, removeStat, setRole, statRemovalBlock } from '../../src/model/stats'
import { deleteTerrain, setMoveCost, terrainRemovalBlock } from '../../src/model/terrain'
import { addUnit, deleteUnit, duplicateUnit, setUnitNumber, setUnitTags } from '../../src/model/units'
import { addWeapon, deleteWeapon, removeVersus, setVersus, setWeaponRange } from '../../src/model/weapons'
import { setFontName, setGameText } from '../../src/model/game'
import { loadDemo } from './fixtures'

const srpg = () => loadDemo('demo-srpg')
const eco = () => loadDemo('demo-economy')

describe('stats', () => {
  it('adding a stat gives it a unique, non-reserved id and touches no unit', () => {
    const d = srpg()
    const next = addStat(addStat(d, 'Level'), 'Luck')
    expect(next.game!.stats.map((s) => s.id).slice(-2)).toEqual(['level_2', 'luck'])
    expect(next.units).toBe(d.units)
  })
  it('removing a stat cleans units, growths and map overrides', () => {
    const d = srpg()
    d.maps.bridge!.units![0].stats = { str: 9, hp: 30 }
    const next = removeStat(d, 'str')
    expect(next.units!.every((u) => !('str' in (u.stats ?? {})) && !('str' in (u.growths ?? {})))).toBe(true)
    expect(next.maps.bridge!.units![0].stats).toEqual({ hp: 30 })
    expect(next.units![0].stats!.hp).toBe(20)
  })
  it('refuses to remove the health or movement stat', () => {
    const d = srpg()
    expect(statRemovalBlock(d, 'hp')).toMatch(/Health/)
    expect(statRemovalBlock(d, 'mov')).toMatch(/Movement/)
    expect(removeStat(d, 'hp')).toBe(d)
    const moved = setRole(d, 'hp', 'def')
    expect(statRemovalBlock(moved, 'hp')).toBeNull()
  })
})

describe('resources and move types', () => {
  it('removing a resource cleans costs, income and starting resources', () => {
    const d = eco()
    expect(findUsages(d, 'resource', 'funds').length).toBeGreaterThan(0)
    const next = removeResource(d, 'funds')
    expect(next.game!.resources).toBeUndefined()
    expect(JSON.stringify(next.units)).not.toContain('funds')
    expect(JSON.stringify(next.terrain)).not.toContain('funds')
    expect(JSON.stringify(next.maps)).not.toContain('funds')
  })
  it('a new move type needs no terrain data; removing one reassigns units and cleans costs', () => {
    const d = srpg()
    const added = addMoveType(d, 'Boat')
    expect(added.terrain).toBe(d.terrain)
    const next = removeMoveType(d, 'foot')
    expect(next.game!.moveTypes.map((m) => m.id)).toEqual(['horse', 'fly'])
    expect(next.units!.filter((u) => u.moveType === 'foot')).toHaveLength(0)
    expect(next.units!.find((u) => u.id === 'lord')!.moveType).toBe('horse')
    expect(next.terrain!.every((t) => !('foot' in (t.moveCost ?? {})))).toBe(true)
  })
  it('keeps at least one move type', () => {
    let d = srpg()
    d = removeMoveType(removeMoveType(d, 'foot'), 'horse')
    expect(removeMoveType(d, 'fly')).toBe(d)
  })
  it('adds resources with unique ids', () => {
    expect(addResource(addResource(srpg(), 'Ore'), 'Ore').game!.resources!.map((r) => r.id)).toEqual(['gold', 'ore', 'ore_2'])
  })
})

describe('deleting things removes every reference', () => {
  it('weapon: off every unit', () => {
    const d = srpg()
    // The demo carries its weapons as items; give one unit type the weapon directly as well.
    d.units!.find((u) => u.id === 'cavalier')!.weapons = ['iron_lance']
    const usages = findUsages(d, 'weapon', 'iron_lance').map((u) => u.text).join()
    expect(usages).toContain('Cavalier')
    expect(usages).toContain('Iron Lance')  // the weapon item goes with it
    const next = deleteWeapon(d, 'iron_lance')
    expect(next.items!.some((i) => i.weapon === 'iron_lance')).toBe(false)
    expect(next.units!.some((u) => u.weapons?.includes('iron_lance'))).toBe(false)
    expect(next.weapons!.some((w) => w.id === 'iron_lance')).toBe(false)
  })
  it('unit: build lists, effectiveness rows and placed map units', () => {
    const d = eco()
    const id = d.units![0].id
    d.weapons![0].versus = { [id]: 2, other: 1 }
    const next = deleteUnit(d, id)
    expect(next.units!.some((u) => u.id === id)).toBe(false)
    expect(next.terrain!.some((t) => t.produces?.includes(id))).toBe(false)
    expect(next.weapons![0].versus).toEqual({ other: 1 })
    expect(Object.values(next.maps).every((m) => m!.units!.every((u) => u.type !== id))).toBe(true)
  })
  it('unit: a versus key that is also a tag of another unit stays', () => {
    const d = srpg()
    d.weapons![3].versus = { knight: 2 }
    d.units!.find((u) => u.id === 'soldier')!.tags = ['knight']
    expect(deleteUnit(d, 'knight').weapons![3].versus).toEqual({ knight: 2 })
  })
  it('terrain: map tiles switch to the first remaining terrain, looks forget its colour', () => {
    const d = srpg()
    expect(findUsages(d, 'terrain', 'forest').some((u) => u.section === 'maps')).toBe(true)
    const next = deleteTerrain(d, 'plain')
    expect(Object.values(next.maps.bridge!.legend)).not.toContain('plain')
    expect(Object.values(next.maps.bridge!.legend)).toContain('forest')
    const noColor = deleteTerrain(d, 'water')
    expect(noColor.game!.styles![1].terrainColors).not.toHaveProperty('water')
  })
  it('never deletes the last terrain, unit or map', () => {
    let d = srpg()
    d = { ...d, terrain: [d.terrain![0]] }
    expect(terrainRemovalBlock(d)).not.toBeNull()
    expect(deleteTerrain(d, d.terrain![0].id)).toBe(d)
    const one = { ...srpg(), units: [srpg().units![0]] }
    expect(deleteUnit(one, one.units![0].id)).toBe(one)
    expect(deleteMap(srpg(), 'bridge').game!.maps).toHaveLength(1)
  })
})

describe('units, weapons, terrain edits', () => {
  it('new units get a letter, a movement type and basic stats; duplicates keep unknown keys', () => {
    const d = srpg()
    const next = addUnit(d, 'Gryphon Rider')
    const u = next.units!.at(-1)!
    expect(u).toMatchObject({ id: 'gryphon_rider', glyph: 'G', moveType: 'foot' })
    expect(typeof u.stats!.hp).toBe('number')
    d.units![0].customKey = { deep: 1 }
    const dup = duplicateUnit(d, d.units![0].id)
    expect(dup.units![1]).toMatchObject({ id: 'lord_copy', name: 'Lord copy', customKey: { deep: 1 } })
  })
  it('numbers stay numbers and emptied tables vanish', () => {
    let d = setUnitNumber(srpg(), 'lord', 'stats', 'str', 12)
    expect(d.units![0].stats!.str).toBe(12)
    d = setUnitNumber(d, 'lord', 'cost', 'funds', 0)
    expect(d.units![0].cost).toEqual({ funds: 0 })
    d = setUnitNumber(d, 'lord', 'cost', 'funds', undefined)
    expect('cost' in d.units![0]).toBe(false)
    expect('tags' in setUnitTags(d, 'lord', []).units![0]).toBe(false)
  })
  it('weapon range and effectiveness', () => {
    let d = setWeaponRange(srpg(), 'iron_sword', 1, 1)
    expect('range' in d.weapons![0]).toBe(false)
    d = setWeaponRange(d, 'iron_sword', 2, 1)
    expect(d.weapons![0].range).toEqual([2, 2])
    d = setVersus(d, 'iron_sword', 'flier', 3, 1)
    expect(d.weapons![0]).toMatchObject({ versus: { flier: 3 }, versusDefault: 1 })
    expect('versus' in removeVersus(d, 'iron_sword', 'flier').weapons![0]).toBe(false)
    expect(addWeapon(eco(), 'Sword').weapons!.at(-1)!.props).toEqual({})
  })
  it('terrain move cost and blocking', () => {
    let d = setMoveCost(srpg(), 'plain', 'horse', 0)
    expect(d.terrain![0].moveCost).toEqual({ horse: 0 })
    d = setMoveCost(d, 'plain', 'horse', undefined)
    expect('moveCost' in d.terrain![0]).toBe(false)
  })
})

describe('rules', () => {
  it('toggling a feature off and on keeps every setting', () => {
    const d = srpg()
    const before = { ...d.game!.rules!.progression! }
    const off = setFeature(d, 'progression', false)
    expect(off.game!.rules!.progression).toMatchObject({ ...before, enabled: false })
    const edited = setFeatureField(off, 'progression', 'xpPerLevel', 250)
    const on = setFeature(edited, 'progression', true)
    expect(on.game!.rules!.progression).toMatchObject({ ...before, xpPerLevel: 250, enabled: true })
  })
  it('first switch-on fills engine defaults without overwriting', () => {
    const on = setFeature(eco(), 'progression', true)
    expect(on.game!.rules!.progression).toEqual({ xpPerLevel: 100, maxLevel: 20, xpHit: '10', xpKill: '30', enabled: true })
    expect(setFeature(eco(), 'capture', false).game!.rules!.capture).toEqual({ enabled: false, amount: 'a.hp / 10' })
  })
  it('at least one way to lose stays on', () => {
    const d = srpg()
    const one = setDefeatCondition(d, 'leaderDead', false)
    expect(one.game!.rules!.defeat).toEqual(['noUnits'])
    expect(setDefeatCondition(one, 'noUnits', false)).toBe(one)
    expect(setDefeatCondition(one, 'lostHq', true).game!.rules!.defeat).toEqual(['noUnits', 'lostHq'])
  })
})

describe('game settings and maps', () => {
  it('font: default removes the key, a built-in sets the name', () => {
    const d = setFontName(srpg(), 'sans')
    expect(d.game!.font).toEqual({ name: 'sans' })
    expect('font' in setFontName(d, undefined).game!).toBe(false)
    expect('description' in setGameText(d, 'description', '').game!).toBe(false)
  })
  it('map list operations keep game.maps and the map files in step', () => {
    let d = addMap(srpg(), 'Second Fight')
    expect(d.game!.maps.at(-1)).toEqual({ id: 'second_fight', name: 'Second Fight', file: 'maps/second_fight.json' })
    expect(d.maps.second_fight!.tiles).toHaveLength(8)
    d = renameMap(d, 'second_fight', 'Renamed')
    expect(d.maps.second_fight!.name).toBe('Renamed')
    d = duplicateMap(d, 'bridge')
    expect(d.game!.maps.map((m) => m.id)).toEqual(['bridge', 'skirmish_at_the_ford_copy', 'second_fight'])
    expect(d.maps.skirmish_at_the_ford_copy!.tiles).toEqual(d.maps.bridge!.tiles)
    d = deleteMap(d, 'second_fight')
    expect('second_fight' in d.maps).toBe(false)
  })
})
