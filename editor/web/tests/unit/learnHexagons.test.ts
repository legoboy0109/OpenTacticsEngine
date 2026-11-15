// Every done test of the lesson "Hexagons" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { hexagons } from '../../src/learn/lessons/hexagons'
import type { ProjectDoc } from '../../src/model/types'
import { cellsOfTerrain } from '../../src/learn/helpers'
import { doneOf, fresh, lessonBasics } from './learnTopicKit'

const f = () => fresh(hexagons)
const done = doneOf(hexagons)
const step = (id: string) => hexagons.steps.find((s) => s.id === id)!
const addCastle = (d: ProjectDoc, rings?: number) => d.structures!.push({ id: 'castle', name: 'Castle', color: '#aa8844', glyph: 'C', ...(rings === undefined ? {} : { rings }) })
const castle = (d: ProjectDoc) => d.structures!.find((s) => s.id === 'castle')!
const map = (d: ProjectDoc) => d.maps.crossing!
/** Paints hexagons of the map with mountains. */
function mountains(d: ProjectDoc, cells: Array<[number, number]>): void {
  for (const [x, y] of cells) map(d).tiles[y] = map(d).tiles[y].slice(0, x) + 'm' + map(d).tiles[y].slice(x + 1)
}

describe('lesson data', () => {
  lessonBasics(hexagons, 5)
  it('starts from the pointy hexagon template as the steps assume', () => {
    const d = f()
    expect(hexagons.template).toBe('hex-tactics-pointy')
    expect(d.game!.maps[0].id).toBe('crossing')
    expect(map(d).grid.type).toBe('hex')
    expect(cellsOfTerrain(d, 'mountain')).toBe(6)
    expect(d.structures!.map((s) => s.id)).toEqual(['city', 'factory', 'hq', 'fortress'])
    expect(map(d).units!.filter((u) => u.side === 'blue')).toHaveLength(3)
  })
})

describe('step: paint', () => {
  it('needs four more mountain hexagons than the map began with', () => {
    const d = f()
    mountains(d, [[0, 0], [1, 0], [2, 0]])
    expect(done('paint', d)).toBe(false)
    mountains(d, [[3, 0]])
    expect(done('paint', d)).toBe(true)
  })
  it('painting something else does not count', () => {
    const d = f()
    map(d).tiles[0] = 'rrrrrrrrrrrr'
    expect(done('paint', d)).toBe(false)
  })
  it('looks at the first map only', () => {
    const d = f()
    d.game!.maps.push({ id: 'second', name: 'Second', file: 'maps/second.json' })
    d.maps.second = structuredClone(map(d))
    d.maps.second.tiles[0] = 'mmmmmmmmmmmm'
    expect(done('paint', d)).toBe(false)
  })
})

describe('steps: new-kind, covers', () => {
  it('a new kind ticks new-kind; covers needs seven hexagons on the NEW kind (the Fortress already has them)', () => {
    const d = f()
    d.structures![3].name = 'Keep'
    expect(done('new-kind', d)).toBe(false)
    expect(done('covers', d)).toBe(false)
    addCastle(d)
    expect(done('new-kind', d)).toBe(true)
    expect(done('covers', d)).toBe(false)
    castle(d).rings = 0
    expect(done('covers', d)).toBe(false)
    castle(d).rings = 1
    expect(done('covers', d)).toBe(true)
  })
  it('"Take me there" opens the new kind, not the Fortress', () => {
    const d = f()
    expect(step('covers').where!.item!(d)).toBeUndefined()
    addCastle(d, 1)
    expect(step('covers').where!.item!(d)).toBe('castle')
  })
})

describe('steps: place, unit', () => {
  it('place needs a seven-hexagon new kind owned by Blue: a one-hexagon kind, a neutral one or the Fortress do not count', () => {
    const d = f()
    addCastle(d, 0)
    map(d).structures!.push({ type: 'castle', x: 4, y: 7, side: 'blue' })
    expect(done('place', d)).toBe(false)
    castle(d).rings = 1
    expect(done('place', d)).toBe(true)
    map(d).structures!.pop()
    map(d).structures!.push({ type: 'castle', x: 4, y: 7 })
    expect(done('place', d)).toBe(false)
    map(d).structures!.push({ type: 'fortress', x: 4, y: 1, side: 'blue' })
    expect(done('place', d)).toBe(false)
  })
  it('unit needs one more blue unit than the map began with', () => {
    const d = f()
    map(d).units!.push({ type: 'infantry', side: 'red', x: 8, y: 1 })
    expect(done('unit', d)).toBe(false)
    map(d).units!.push({ type: 'infantry', side: 'blue', x: 4, y: 6 })
    expect(done('unit', d)).toBe(true)
  })
})

describe('step: play', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', f(), ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', f(), ['played'])).toBe(true)
  })
})
