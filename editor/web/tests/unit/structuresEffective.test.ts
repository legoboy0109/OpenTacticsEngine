// Cases taken from tests/test_structures.cpp ("structure_movement_override_inherit_and_blocked",
// "structure_movement_cost_is_set_per_move_type", "structure_numbers_merge_with_terrain_for_formulas"),
// so the editor's effective numbers cannot drift from the engine's rule.
import { describe, expect, it } from 'vitest'
import { canEnter, effectiveNumbers, enterCost } from '../../src/model/structuresEffective'
import type { Structure, Terrain, Unit } from '../../src/model/types'

const forest: Terrain = { id: 'forest', name: 'Forest', moveCost: { foot: 2, wheel: 2 }, props: { defense: 1 } }
const plain: Terrain = { id: 'plain', name: 'Plain' }
const foot: Unit = { id: 'soldier', name: 'Soldier', moveType: 'foot' }

const city: Structure = { id: 'city', name: 'City' }
const castle: Structure = { id: 'castle', name: 'Castle', width: 2, height: 2, props: { defense: 3 } }
const gate: Structure = { id: 'gate', name: 'Gate', width: 2, height: 1, moveCost: { foot: 1 }, props: { defense: 2, cover: 7 } }
const mire: Structure = { id: 'mire', name: 'Mire', moveCost: { foot: 4 } }
const block: Structure = { id: 'block', name: 'Block', moveCost: { foot: 0 } }

describe('movement', () => {
  it('override cheaper, inherit, override dearer, cannot enter (forest ground costs 2)', () => {
    expect(enterCost(forest, undefined, 'foot')).toEqual({ cost: 2, from: 'ground' })
    expect(enterCost(forest, gate, 'foot')).toEqual({ cost: 1, from: 'structure' })
    expect(enterCost(forest, city, 'foot')).toEqual({ cost: 2, from: 'ground' })
    expect(enterCost(forest, mire, 'foot')).toEqual({ cost: 4, from: 'structure' })
    expect(enterCost(forest, block, 'foot')).toEqual({ cost: 0, from: 'structure' })
  })
  it('plain ground without a listed cost costs 1', () => {
    expect(enterCost(plain, undefined, 'foot')).toEqual({ cost: 1, from: 'ground' })
  })
  it('a structure sets a cost per movement type: the others keep the terrain\'s', () => {
    const wheelMire: Structure = { id: 'mire', name: 'Mire', moveCost: { wheel: 5 } }
    expect(enterCost(forest, wheelMire, 'foot').cost).toBe(2)
    expect(enterCost(forest, wheelMire, 'wheel').cost).toBe(5)
  })
  it('a negative number means "not set" (the engine\'s in-memory -1)', () => {
    expect(enterCost(forest, { id: 'x', name: 'x', moveCost: { foot: -1 } }, 'foot')).toEqual({ cost: 2, from: 'ground' })
  })
  it('canEnter judges with the structure: a block makes a unit\'s square impassable, a pier makes water enterable', () => {
    expect(canEnter(forest, block, foot)).toBe(false)
    expect(canEnter(forest, gate, foot)).toBe(true)
    const water: Terrain = { id: 'water', name: 'Water', moveCost: { foot: 0 } }
    expect(canEnter(water, undefined, foot)).toBe(false)
    expect(canEnter(water, { id: 'pier', name: 'Pier', moveCost: { foot: 1 } }, foot)).toBe(true)
    expect(canEnter(undefined, block, foot)).toBe(true) // nothing to judge
    expect(canEnter(forest, block, undefined)).toBe(true)
  })
})

describe('numbers', () => {
  it('no structure: the terrain\'s numbers untouched', () => {
    expect(effectiveNumbers(forest, undefined)).toEqual([{ key: 'defense', value: 1, from: 'ground' }])
  })
  it('a structure number replaces the terrain\'s of the same name, not added (castle on forest: defense 3, one entry)', () => {
    expect(effectiveNumbers(forest, castle)).toEqual([{ key: 'defense', value: 3, from: 'structure' }])
  })
  it('plain underneath has no numbers: the structure\'s alone', () => {
    expect(effectiveNumbers(plain, castle)).toEqual([{ key: 'defense', value: 3, from: 'structure' }])
  })
  it('a number the terrain lacks is added (gate: defense 2 replaces 1, cover 7 added: two entries)', () => {
    expect(effectiveNumbers(forest, gate)).toEqual([{ key: 'defense', value: 2, from: 'structure' }, { key: 'cover', value: 7, from: 'structure' }])
  })
  it('a structure that sets no numbers leaves the terrain\'s as they are', () => {
    expect(effectiveNumbers(forest, city)).toEqual([{ key: 'defense', value: 1, from: 'ground' }])
  })
})
