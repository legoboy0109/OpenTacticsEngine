// Every done test of the lesson "Art" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { art } from '../../src/learn/lessons/art'
import type { ProjectDoc } from '../../src/model/types'
import { doneOf, fresh, lessonBasics } from './learnTopicKit'

const f = () => fresh(art)
const done = doneOf(art)
const step = (id: string) => art.steps.find((s) => s.id === id)!
const addRoad = (d: ProjectDoc) => d.terrain!.push({ id: 'road', name: 'Road', color: '#c9c3b0', glyph: 'R' })
const addArcher = (d: ProjectDoc) => d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot', weapons: [] })
/** Paints squares of the first map with terrain `id` (a new legend symbol when needed). */
function paint(d: ProjectDoc, id: string, cells: Array<[number, number]>): void {
  const map = d.maps.start!
  let ch = Object.keys(map.legend).find((c) => map.legend[c] === id)
  if (!ch) { ch = 'r'; map.legend[ch] = id }
  for (const [x, y] of cells) map.tiles[y] = map.tiles[y].slice(0, x) + ch + map.tiles[y].slice(x + 1)
}
const five: Array<[number, number]> = [[0, 0], [1, 0], [2, 0], [3, 0], [4, 0]]

describe('lesson data', () => {
  lessonBasics(art, 7)
  it('starts from the blank template with ready-made images and pictures only for the soldier', () => {
    const d = f()
    expect(art.template).toBe('blank')
    expect(d.terrain!.map((t) => t.id)).toEqual(['plain', 'wall'])
    expect(d.units!.map((u) => u.id)).toEqual(['soldier'])
    expect(d.graphics!.sheets.map((s) => s.id)).toEqual(['tiles', 'units'])
    expect(Object.keys(d.graphics!.units!)).toEqual(['soldier'])
    expect(d.maps.start!.units!.map((u) => `${u.side}:${u.type}`)).toEqual(['blue:soldier', 'red:soldier'])
  })
})

describe('steps: new-terrain, open-chooser, join-picture', () => {
  it('a new terrain ticks new-terrain; a picture for Plain does not', () => {
    const d = f()
    d.graphics!.terrain!.plain = { sheet: 'tiles', cell: [1, 0] }
    expect(done('new-terrain', d)).toBe(false)
    addRoad(d)
    expect(done('new-terrain', d)).toBe(true)
  })
  it('the chooser step follows its signal only', () => {
    const d = f()
    addRoad(d)
    expect(done('open-chooser', d, ['saved', 'played'])).toBe(false)
    expect(done('open-chooser', d, ['pictureChooser'])).toBe(true)
  })
  it('join-picture needs a joining picture on the NEW terrain: one piece, a mix or a picture for Plain does not count', () => {
    const d = f()
    addRoad(d)
    d.graphics!.terrain!.road = { sheet: 'tiles', cell: [0, 2] }
    expect(done('join-picture', d)).toBe(false)
    d.graphics!.terrain!.road = { sheet: 'tiles', variants: [[0, 0], [1, 0]] }
    expect(done('join-picture', d)).toBe(false)
    delete d.graphics!.terrain!.road
    d.graphics!.terrain!.plain = { sheet: 'tiles', join: [0, 2] }
    expect(done('join-picture', d)).toBe(false)
    d.graphics!.terrain!.road = { sheet: 'tiles', join: [0, 2] }
    expect(done('join-picture', d)).toBe(true)
  })
  it('"Take me there" opens the new terrain', () => {
    const d = f()
    expect(step('open-chooser').where!.item!(d)).toBeUndefined()
    addRoad(d)
    expect(step('open-chooser').where!.item!(d)).toBe('road')
  })
})

describe('step: paint', () => {
  it('needs five squares of the joining terrain; plain pieces or a road without a joining picture do not count', () => {
    const d = f()
    addRoad(d)
    paint(d, 'road', five)
    expect(done('paint', d)).toBe(false) // no joining picture yet
    d.graphics!.terrain!.road = { sheet: 'tiles', join: [0, 2] }
    expect(done('paint', d)).toBe(true)
    paint(d, 'plain', [[4, 0]])
    expect(done('paint', d)).toBe(false) // four are not enough
  })
  it('looks at the first map only', () => {
    const d = f()
    addRoad(d)
    d.graphics!.terrain!.road = { sheet: 'tiles', join: [0, 2] }
    d.game!.maps.push({ id: 'second', name: 'Second', file: 'maps/second.json' })
    d.maps.second = structuredClone(d.maps.start!)
    d.maps.second.legend.r = 'road'
    d.maps.second.tiles[0] = 'rrrrrrrr'
    expect(done('paint', d)).toBe(false)
  })
})

describe('steps: new-unit, unit-picture, place', () => {
  it('a second unit ticks new-unit; a picture for the soldier does not tick unit-picture', () => {
    const d = f()
    expect(done('new-unit', d)).toBe(false)
    addArcher(d)
    expect(done('new-unit', d)).toBe(true)
    expect(done('unit-picture', d)).toBe(false)
    d.graphics!.units!.soldier = { sheet: 'units', frames: [[1, 0]] }
    expect(done('unit-picture', d)).toBe(false)
    d.graphics!.units!.archer = { sheet: 'units', cell: [1, 0] }
    expect(done('unit-picture', d)).toBe(true)
  })
  it('"Take me there" opens the new unit', () => {
    const d = f()
    expect(step('unit-picture').where!.item!(d)).toBeUndefined()
    addArcher(d)
    expect(step('unit-picture').where!.item!(d)).toBe('archer')
  })
  it('place needs the new unit for Blue on the map', () => {
    const d = f()
    addArcher(d)
    d.maps.start!.units!.push({ type: 'soldier', side: 'blue', x: 0, y: 3 })
    expect(done('place', d)).toBe(false)
    d.maps.start!.units!.push({ type: 'archer', side: 'red', x: 7, y: 3 })
    expect(done('place', d)).toBe(false)
    d.maps.start!.units!.push({ type: 'archer', side: 'blue', x: 0, y: 4 })
    expect(done('place', d)).toBe(true)
  })
})

describe('step: play', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', f(), ['saved', 'pictureChooser'])).toBe(false)
    expect(done('play', f(), ['played'])).toBe(true)
  })
})
