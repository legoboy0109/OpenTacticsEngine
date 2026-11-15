// The done-test helpers (src/learn/helpers.ts) on small documents.
import { describe, expect, it } from 'vitest'
import { addedCount, cellsOfOtherTerrain, cellsOfTerrain, firstMapId, mapOf, nameChanged, structuresOnMap, unitTypesWithWeapon, unitsOnMap } from '../../src/learn/helpers'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const blank = (): ProjectDoc => structuredClone(loadTemplate('blank'))

describe('maps', () => {
  it('mapOf defaults to the first map and finds others by id; unknown = null', () => {
    const d = blank()
    expect(firstMapId(d)).toBe('start')
    expect(mapOf(d)).toBe(d.maps.start)
    expect(mapOf(d, 'start')).toBe(d.maps.start)
    expect(mapOf(d, 'nope')).toBeNull()
    expect(mapOf({ ...d, game: null })).toBeNull()
  })
})

describe('terrain on a map', () => {
  it('counts squares of one terrain and of everything else', () => {
    const d = blank() // 8 x 6 with four walls
    expect(cellsOfTerrain(d, 'wall')).toBe(4)
    expect(cellsOfTerrain(d, 'plain')).toBe(44)
    expect(cellsOfTerrain(d, 'water')).toBe(0)
    expect(cellsOfOtherTerrain(d, ['plain', 'wall'])).toBe(0)
    expect(cellsOfOtherTerrain(d, ['plain'])).toBe(4)
  })
  it('is 0 for a map that cannot be read or does not exist', () => {
    const d = blank()
    d.maps.start = null
    expect(cellsOfTerrain(d, 'plain')).toBe(0)
    expect(cellsOfTerrain(d, 'plain', 'nope')).toBe(0)
  })
  it('ignores a square whose symbol has no terrain', () => {
    const d = blank()
    d.maps.start!.tiles[0] = '?.......'
    expect(cellsOfTerrain(d, 'plain')).toBe(43)
    expect(cellsOfOtherTerrain(d, ['plain', 'wall'])).toBe(0)
  })
})

describe('units and structures on a map', () => {
  it('filters units by side, type and excluded types', () => {
    const d = blank()
    d.maps.start!.units!.push({ type: 'archer', side: 'blue', x: 0, y: 0 })
    expect(unitsOnMap(d)).toBe(3)
    expect(unitsOnMap(d, { side: 'blue' })).toBe(2)
    expect(unitsOnMap(d, { side: 'red' })).toBe(1)
    expect(unitsOnMap(d, { type: 'archer' })).toBe(1)
    expect(unitsOnMap(d, { side: 'red', type: 'archer' })).toBe(0)
    expect(unitsOnMap(d, { notTypes: ['soldier'] })).toBe(1)
    expect(unitsOnMap(d, { side: 'blue', notTypes: ['soldier'] })).toBe(1)
    expect(unitsOnMap(d, { side: 'blue' }, 'nope')).toBe(0)
  })
  it('counts structures, optionally by kind and owner', () => {
    const d = blank()
    expect(structuresOnMap(d)).toBe(0)
    d.maps.start!.structures = [{ type: 'town', x: 0, y: 0, side: 'blue' }, { type: 'town', x: 2, y: 0 }, { type: 'fort', x: 4, y: 0, side: 'red' }]
    expect(structuresOnMap(d)).toBe(3)
    expect(structuresOnMap(d, { type: 'town' })).toBe(2)
    expect(structuresOnMap(d, { side: 'blue' })).toBe(1)
    expect(structuresOnMap(d, { type: 'town', side: 'red' })).toBe(0)
  })
})

describe('lists in the game', () => {
  it('addedCount ignores what the template began with, and a missing list', () => {
    const d = blank()
    expect(addedCount(d.terrain, ['plain', 'wall'])).toBe(0)
    d.terrain!.push({ id: 'water', name: 'Water' })
    expect(addedCount(d.terrain, ['plain', 'wall'])).toBe(1)
    expect(addedCount(null, [])).toBe(0)
  })
  it('unitTypesWithWeapon counts unit types that have a weapon, leaving some out', () => {
    const d = blank()
    expect(unitTypesWithWeapon(d)).toBe(1)
    expect(unitTypesWithWeapon(d, ['soldier'])).toBe(0)
    d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot', weapons: ['sword'] }, { id: 'monk', name: 'Monk', moveType: 'foot' })
    expect(unitTypesWithWeapon(d)).toBe(2)
    expect(unitTypesWithWeapon(d, ['soldier'])).toBe(1)
  })
  it('nameChanged: set and not starting with the prefix', () => {
    const d = blank()
    d.game!.name = 'Practice: X'
    expect(nameChanged(d, 'Practice:')).toBe(false)
    d.game!.name = 'Mine'
    expect(nameChanged(d, 'Practice:')).toBe(true)
    d.game!.name = ''
    expect(nameChanged(d, 'Practice:')).toBe(false)
  })
})
