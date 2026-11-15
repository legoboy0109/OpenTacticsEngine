import { describe, expect, it } from 'vitest'
import { setStyleStructureColor } from '../../src/model/game'
import { setLook } from '../../src/model/graphicsLooks'
import { removeSheet } from '../../src/model/graphics'
import { sheetUsers } from '../../src/model/graphicsUsage'
import { removeMoveType, removeResource } from '../../src/model/rulesLists'
import {
  addStructure, deleteStructure, duplicateStructure, isHeadquarters, moveModeOf, moveStructureKind, newStructureId, ownershipMatters, setCapturePoints,
  setHeadquarters, setMoveRule, setProduces, setSize, setStructureIncome, setStructureNumber, setStructureTags, sizeOf, updateStructure,
} from '../../src/model/structures'
import { findUsages } from '../../src/model/references'
import { placedCounts, structureUsageLines } from '../../src/model/structuresRefs'
import { deleteUnit } from '../../src/model/units'
import type { ProjectDoc } from '../../src/model/types'
import { problemTarget } from '../../src/model/problems'
import { loadDemo, loadPlainEconomy } from './fixtures'

const eco = () => loadPlainEconomy()
const srpg = () => loadDemo('demo-srpg')
const none = (): ProjectDoc => ({ ...loadDemo('demo-srpg'), structures: null })

describe('the list', () => {
  it('the first structure creates the list from "none"; the new kind has a colour, a letter and no size keys', () => {
    const d = addStructure(none(), 'Watch Tower')
    expect(d.structures).toHaveLength(1)
    expect(d.structures![0]).toMatchObject({ id: 'watch_tower', name: 'Watch Tower', glyph: 'W' })
    expect(d.structures![0].color).toMatch(/^#[0-9a-f]{6}$/i)
    expect(d.structures![0]).not.toHaveProperty('width')
    expect(newStructureId(d, 'Watch Tower')).toBe('watch_tower_2')
  })
  it('edits on "none" that are not an add do nothing sensible (no crash, list stays null or empty)', () => {
    const d = none()
    expect(updateStructure(d, 'x', { name: 'y' }).structures ?? []).toEqual([])
    expect(deleteStructure(d, 'x')).toBe(d)
  })
  it('renames without changing the id; reorders', () => {
    let d = updateStructure(eco(), 'city', { name: 'Town' })
    expect(d.structures![0]).toMatchObject({ id: 'city', name: 'Town' })
    d = moveStructureKind(d, 0, 2)
    expect(d.structures!.map((s) => s.id)).toEqual(['factory', 'hq', 'city'])
  })
  it('duplicates with a new id and copies its pictures', () => {
    let d = eco()
    d = { ...d, graphics: { sheets: [{ id: 'b', file: 'art/b.png', cellWidth: 24, cellHeight: 24 }] } }
    d = setLook(d, { kind: 'structures', id: 'city' }, { sheet: 'b', cell: [0, 0], sides: { blue: { sheet: 'b', cell: [1, 0] } } })
    d = setLook(d, { kind: 'structures', id: 'city', style: 'night' }, { sheet: 'b', cell: [2, 0] })
    const out = duplicateStructure(d, 'city')
    expect(out.structures!.map((s) => s.id)).toEqual(['city', 'city_copy', 'factory', 'hq'])
    expect(out.structures![1].name).toBe('City copy')
    expect(out.graphics!.structures!.city_copy).toEqual(out.graphics!.structures!.city)
    expect(out.graphics!.styles!.night.structures!.city_copy).toEqual({ sheet: 'b', cell: [2, 0] })
    out.graphics!.structures!.city_copy.cell![0] = 9
    expect(out.graphics!.structures!.city.cell![0]).toBe(0) // a real copy
  })
})

describe('settings', () => {
  it('size: 1 to 8, 1 is not written, values are clamped', () => {
    let d = setSize(eco(), 'city', 3, 2)
    expect(sizeOf(d.structures![0])).toEqual({ w: 3, h: 2 })
    d = setSize(d, 'city', 1, 1)
    expect(d.structures![0]).not.toHaveProperty('width')
    expect(d.structures![0]).not.toHaveProperty('height')
    expect(sizeOf(setSize(d, 'city', 99, 0).structures![0])).toEqual({ w: 8, h: 1 })
    expect(sizeOf(undefined)).toEqual({ w: 1, h: 1 })
    expect(sizeOf({ id: 'x', name: 'x', width: 2.5 as number })).toEqual({ w: 1, h: 1 })
  })
  it('movement: same as the ground (key removed), an own cost, cannot enter (0)', () => {
    let d = setMoveRule(eco(), 'city', 'foot', 'cost', 3)
    expect(d.structures![0].moveCost).toEqual({ foot: 3 })
    expect(moveModeOf(d.structures![0], 'foot')).toBe('cost')
    d = setMoveRule(d, 'city', 'foot', 'blocked')
    expect(d.structures![0].moveCost).toEqual({ foot: 0 })
    expect(moveModeOf(d.structures![0], 'foot')).toBe('blocked')
    expect(moveModeOf(d.structures![0], 'tread')).toBe('ground')
    d = setMoveRule(d, 'city', 'foot', 'ground')
    expect(d.structures![0]).not.toHaveProperty('moveCost')
    expect(setMoveRule(eco(), 'city', 'foot', 'cost', 0).structures![0].moveCost).toEqual({ foot: 1 }) // a cost is at least 1
  })
  it('numbers: a value replaces the ground\'s; empty = same as the ground', () => {
    let d = setStructureNumber(eco(), 'city', 'defense', 5)
    expect(d.structures![0].props).toEqual({ defense: 5 })
    d = setStructureNumber(d, 'city', 'defense', undefined)
    expect(d.structures![0]).not.toHaveProperty('props')
  })
  it('capture points, income, build list, headquarters, labels', () => {
    let d = setCapturePoints(eco(), 'city', undefined)
    expect(d.structures![0]).not.toHaveProperty('capturePoints')
    d = setCapturePoints(d, 'city', 12)
    expect(d.structures![0].capturePoints).toBe(12)
    d = setStructureIncome(d, 'city', 'funds', 500)
    expect(d.structures![0].income).toEqual({ funds: 500 })
    d = setStructureIncome(d, 'city', 'funds', undefined)
    expect(d.structures![0]).not.toHaveProperty('income')
    d = setProduces(d, 'city', ['infantry'])
    expect(d.structures![0].produces).toEqual(['infantry'])
    expect(setProduces(d, 'city', []).structures![0]).not.toHaveProperty('produces')
    d = setHeadquarters(d, 'city', true)
    expect(isHeadquarters(d.structures![0])).toBe(true)
    d = setStructureTags(d, 'city', ['hq', 'wet'])
    expect(d.structures![0].tags).toEqual(['hq', 'wet'])
    d = setHeadquarters(d, 'city', false)
    expect(d.structures![0].tags).toEqual(['wet'])
    expect(setStructureTags(d, 'city', []).structures![0]).not.toHaveProperty('tags')
  })
  it('knows which kinds ownership matters on', () => {
    const s = eco().structures!
    expect(s.every((x) => ownershipMatters(x))).toBe(true)
    expect(ownershipMatters({ id: 'f', name: 'Fort', props: { defense: 2 } })).toBe(false)
    expect(ownershipMatters(undefined)).toBe(false)
    expect(ownershipMatters(srpg().structures![0])).toBe(false)
  })
  it('unknown keys on a structure survive edits', () => {
    let d = eco()
    d = { ...d, structures: d.structures!.map((s) => ({ ...s, __keep: 1 })) }
    d = setSize(setCapturePoints(updateStructure(d, 'city', { color: '#112233' }), 'city', 5), 'city', 2, 2)
    expect((d.structures![0] as Record<string, unknown>).__keep).toBe(1)
  })
})

describe('deleting a kind', () => {
  it('removes the placed ones on every map, its pictures and its style colours in one edit', () => {
    let d = eco()
    d = { ...d, game: { ...d.game!, styles: [{ id: 'night', name: 'Night', structureColors: { city: '#111111', hq: '#222222' } }] }, graphics: { sheets: [{ id: 'b', file: 'art/b.png', cellWidth: 24, cellHeight: 24 }] } }
    d = setLook(d, { kind: 'structures', id: 'city' }, { sheet: 'b', cell: [0, 0] })
    d = setLook(d, { kind: 'structures', id: 'city', style: 'night' }, null)
    const cities = d.maps.twin_rivers!.structures!.filter((s) => s.type === 'city').length
    expect(cities).toBeGreaterThan(0)
    expect(placedCounts(d, 'city')).toEqual([{ mapId: 'twin_rivers', name: expect.any(String), count: cities }])
    expect(structureUsageLines(d, 'city').map((u) => u.section).sort()).toEqual(['art', 'game', 'maps'])
    const out = deleteStructure(d, 'city')
    expect(out.structures!.map((s) => s.id)).toEqual(['factory', 'hq'])
    expect(out.maps.twin_rivers!.structures!.some((s) => s.type === 'city')).toBe(false)
    expect(out.maps.twin_rivers!.structures!.length).toBe(d.maps.twin_rivers!.structures!.length - cities)
    expect(out.game!.styles![0].structureColors).toEqual({ hq: '#222222' })
    expect(out.graphics!.structures).toBeUndefined()
    expect(out.graphics!.styles).toBeUndefined()
    // the terrain and everything else is untouched
    expect(out.terrain).toBe(d.terrain)
    expect(out.maps.twin_rivers!.tiles).toBe(d.maps.twin_rivers!.tiles)
  })
  it('may delete the last kind (a game can have none)', () => {
    let d = none()
    d = addStructure(d, 'A')
    expect(deleteStructure(d, 'a').structures).toEqual([])
  })
  it('deleting a unit removes it from structures\' build lists; deleting a resource from their income; a movement type from their movement rules', () => {
    let d = eco()
    d = setMoveRule(d, 'city', 'tread', 'cost', 2)
    expect(findUsages(d, 'unit', 'tank').some((u) => u.section === 'structures' && /Factory/.test(u.text))).toBe(true)
    expect(deleteUnit(d, 'tank').structures!.find((s) => s.id === 'factory')!.produces).not.toContain('tank')
    expect(findUsages(d, 'resource', 'funds').filter((u) => u.section === 'structures')).toHaveLength(3)
    const noFunds = removeResource(d, 'funds')
    expect(noFunds.structures!.every((s) => !s.income)).toBe(true)
    expect(findUsages(d, 'moveType', 'tread').some((u) => u.section === 'structures' && /City/.test(u.text))).toBe(true)
    const noTread = removeMoveType(d, 'tread')
    expect(noTread.structures!.find((s) => s.id === 'city')).not.toHaveProperty('moveCost')
  })
  it('removing an image takes it from structures\' pictures; sheetUsers lists structures', () => {
    let d = eco()
    d = { ...d, graphics: { sheets: [{ id: 'b', file: 'art/b.png', cellWidth: 24, cellHeight: 24 }] } }
    d = setLook(d, { kind: 'structures', id: 'hq' }, { sheet: 'b', cell: [0, 0], sides: { red: { sheet: 'b', cell: [1, 0] } } })
    expect(sheetUsers(d, 'b')).toEqual([{ kind: 'structures', id: 'hq', name: 'Headquarters' }])
    const out = removeSheet(d, 'b')
    expect(out.graphics!.structures).toBeUndefined()
  })
})

describe('styles', () => {
  it('a style\'s structure colour is set and reset; an emptied map leaves the key', () => {
    let d: ProjectDoc = { ...eco(), game: { ...eco().game!, styles: [{ id: 'night', name: 'Night' }] } }
    d = setStyleStructureColor(d, 'night', 'city', '#123456')
    expect(d.game!.styles![0].structureColors).toEqual({ city: '#123456' })
    d = setStyleStructureColor(d, 'night', 'city', undefined)
    expect(d.game!.styles![0]).not.toHaveProperty('structureColors')
  })
})

describe('problems', () => {
  it('problems in data/structures.json open the Structures section', () => {
    expect(problemTarget('data/structures.json', eco())).toEqual({ section: 'structures' })
  })
})
