// Every done test of the lesson "Economy" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { economy } from '../../src/learn/lessons/economy'
import type { ProjectDoc } from '../../src/model/types'
import { doneOf, fresh, lessonBasics } from './learnTopicKit'

const f = () => fresh(economy)
const done = doneOf(economy)
const step = (id: string) => economy.steps.find((s) => s.id === id)!
const addBarracks = (d: ProjectDoc) => d.structures!.push({ id: 'barracks', name: 'Barracks', color: '#aa8844', glyph: 'B' })
const barracks = (d: ProjectDoc) => d.structures!.find((s) => s.id === 'barracks')!

describe('lesson data', () => {
  lessonBasics(economy, 6)
  it('starts from economy-tactics as the steps assume', () => {
    const d = f()
    expect(d.structures!.map((s) => s.id)).toEqual(['city', 'factory', 'hq'])
    expect(d.structures!.some((s) => (s.upgrades?.length ?? 0) > 0)).toBe(false)
    expect(d.units!.map((u) => u.id)).toEqual(['infantry', 'tank', 'artillery'])
    expect(d.research).toBeNull()
    expect(d.maps.crossroads!.fog).toBeUndefined()
    expect(d.game!.rules!.production!.enabled).toBe(true)
  })
})

describe('steps: new-kind, produces, place', () => {
  it('a new kind ticks new-kind; renaming the Factory does not', () => {
    const d = f()
    d.structures![1].name = 'Works'
    expect(done('new-kind', d)).toBe(false)
    addBarracks(d)
    expect(done('new-kind', d)).toBe(true)
  })
  it('produces needs a unit type on the NEW kind (the Factory already builds)', () => {
    const d = f()
    expect(done('produces', d)).toBe(false)
    addBarracks(d)
    expect(done('produces', d)).toBe(false)
    barracks(d).produces = []
    expect(done('produces', d)).toBe(false)
    barracks(d).produces = ['infantry']
    expect(done('produces', d)).toBe(true)
  })
  it('place needs a Barracks that builds units, owned by Blue', () => {
    const d = f()
    addBarracks(d)
    const m = d.maps.crossroads!
    m.structures!.push({ type: 'barracks', x: 0, y: 6, side: 'blue' }) // builds nothing yet
    expect(done('place', d)).toBe(false)
    barracks(d).produces = ['infantry']
    expect(done('place', d)).toBe(true)
    m.structures!.pop()
    m.structures!.push({ type: 'barracks', x: 0, y: 6 }) // nobody's
    expect(done('place', d)).toBe(false)
    m.structures!.push({ type: 'factory', x: 0, y: 7, side: 'blue' }) // not new
    expect(done('place', d)).toBe(false)
  })
  it('"Take me there" opens the new kind', () => {
    const d = f()
    addBarracks(d)
    expect(step('produces').where!.item!(d)).toBe('barracks')
    expect(step('upgrade').where!.item!(d)).toBe('city')
  })
})

describe('step: upgrade', () => {
  it('needs an upgrade with a cost; an upgrade with no cost, or none, does not count', () => {
    const d = f()
    expect(done('upgrade', d)).toBe(false)
    d.structures![0].upgrades = [{ to: 'factory' }]
    expect(done('upgrade', d)).toBe(false)
    d.structures![0].upgrades = [{ to: 'factory', cost: { funds: 0 } }]
    expect(done('upgrade', d)).toBe(false)
    d.structures![0].upgrades = [{ to: 'factory', cost: { funds: 500 } }]
    expect(done('upgrade', d)).toBe(true)
  })
  it('settles before ticking, so typing a cost is not cut off', () => {
    expect(step('upgrade').settleMs).toBeGreaterThan(0)
  })
})

describe('steps: new-tech, unlock', () => {
  it('a technology ticks new-tech; unlocking needs a unit type ticked', () => {
    const d = f()
    expect(done('new-tech', d)).toBe(false)
    d.research = []
    expect(done('new-tech', d)).toBe(false)
    d.research.push({ id: 'armor', name: 'Heavy Armor' })
    expect(done('new-tech', d)).toBe(true)
    expect(done('unlock', d)).toBe(false)
    d.research[0].unlocks = { units: [] }
    expect(done('unlock', d)).toBe(false)
    d.research[0].unlocks = { units: ['tank'] }
    expect(done('unlock', d)).toBe(true)
  })
  it('"Take me there" opens the technology', () => {
    const d = f()
    expect(step('unlock').where!.item!(d)).toBeUndefined()
    d.research = [{ id: 'armor', name: 'Heavy Armor' }]
    expect(step('unlock').where!.item!(d)).toBe('armor')
  })
})

describe('step: fog', () => {
  it('needs fog on the first map', () => {
    const d = f()
    d.game!.maps.push({ id: 'second', name: 'Second', file: 'maps/second.json' })
    d.maps.second = { ...structuredClone(d.maps.crossroads!), fog: true }
    expect(done('fog', d)).toBe(false)
    d.maps.crossroads!.fog = false
    expect(done('fog', d)).toBe(false)
    d.maps.crossroads!.fog = true
    expect(done('fog', d)).toBe(true)
  })
})

describe('step: play', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', f(), ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', f(), ['played'])).toBe(true)
  })
})
