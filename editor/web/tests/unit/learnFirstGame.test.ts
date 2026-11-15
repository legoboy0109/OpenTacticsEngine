// Every done test of the lesson "Your first game" against small documents: false at the start, false for the near
// misses a maker could make, true once the step's work is done. Also checks the lesson's data is well formed.
import { describe, expect, it } from 'vitest'
import { advance } from '../../src/learn/runner'
import { LESSONS } from '../../src/learn/lessons'
import { firstGame } from '../../src/learn/lessons/first-game'
import type { SignalName } from '../../src/learn/signals'
import type { LessonProgress } from '../../src/learn/state'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

/** The practice game as the editor makes it: the blank template, named "Practice: <lesson title>". */
const fresh = (): ProjectDoc => { const d = structuredClone(loadTemplate('blank')); d.game!.name = 'Practice: Your first game'; return d }
const NONE: ReadonlySet<SignalName> = new Set()
const step = (id: string) => firstGame.steps.find((s) => s.id === id)!
const done = (id: string, doc: ProjectDoc, signals: SignalName[] = []) => step(id).done(doc, 'game', new Set(signals))

/** Paints squares of the first map with terrain `id` (a new legend symbol when needed). */
function paint(doc: ProjectDoc, id: string, cells: Array<[number, number]>): void {
  const map = doc.maps.start!
  let ch = Object.keys(map.legend).find((c) => map.legend[c] === id)
  if (!ch) { ch = 'w'; map.legend[ch] = id }
  for (const [x, y] of cells) map.tiles[y] = map.tiles[y].slice(0, x) + ch + map.tiles[y].slice(x + 1)
}
const addTerrain = (doc: ProjectDoc) => doc.terrain!.push({ id: 'water', name: 'Water', color: '#3366cc', glyph: '~' })

describe('lesson data', () => {
  it('is registered, with unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(firstGame)
    expect(new Set(firstGame.steps.map((s) => s.id)).size).toBe(firstGame.steps.length)
    expect(firstGame.steps.length).toBeGreaterThanOrEqual(8)
    expect(firstGame.steps.length).toBeLessThanOrEqual(10)
    expect(firstGame.template).toBe('blank')
    for (const s of firstGame.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
  })
  it('uses no programmer words in the texts, and never calls the style a "look"', () => {
    const all = [firstGame.title, firstGame.description, ...firstGame.summary, ...firstGame.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('starts from the blank template as the steps assume', () => {
    const d = fresh()
    expect(d.terrain!.map((t) => t.id)).toEqual(['plain', 'wall'])
    expect(d.units!.map((u) => u.id)).toEqual(['soldier'])
    expect(d.weapons!.map((w) => w.id)).toEqual(['sword'])
    expect(d.game!.maps[0].id).toBe('start')
    expect(d.maps.start!.units!.map((u) => `${u.side}:${u.type}`)).toEqual(['blue:soldier', 'red:soldier'])
  })
  it('has no step that is already true at the start (not even with every signal missing)', () => {
    const d = fresh()
    for (const s of firstGame.steps) expect(s.done(d, 'game', NONE), s.id).toBe(false)
  })
})

describe('step: name', () => {
  it('needs a new name, not the practice name and not an empty one', () => {
    const d = fresh()
    d.game!.name = 'Practice: Your first game'
    expect(done('name', d)).toBe(false)
    d.game!.name = 'Practice: Your first game 2'
    expect(done('name', d)).toBe(false)
    d.game!.name = '   '
    expect(done('name', d)).toBe(false)
    d.game!.name = 'Moonlit Valley'
    expect(done('name', d)).toBe(true)
  })
  it('settles before ticking, so typing is not cut off', () => {
    expect(step('name').settleMs).toBeGreaterThan(0)
  })
})

describe('step: new-terrain, paint', () => {
  it('a new terrain ticks new-terrain; renaming plain or wall does not', () => {
    const d = fresh()
    d.terrain![0].name = 'Grass'
    expect(done('new-terrain', d)).toBe(false)
    addTerrain(d)
    expect(done('new-terrain', d)).toBe(true)
  })
  it('paint needs four squares of a terrain that is not plain or wall', () => {
    const d = fresh()
    addTerrain(d)
    expect(done('paint', d)).toBe(false)
    paint(d, 'water', [[0, 0], [1, 0], [2, 0]])
    expect(done('paint', d)).toBe(false)
    paint(d, 'water', [[3, 0]])
    expect(done('paint', d)).toBe(true)
  })
  it('painting plain or wall more does not count', () => {
    const d = fresh()
    paint(d, 'wall', [[0, 0], [1, 0], [2, 0], [3, 0], [4, 0], [5, 0]])
    expect(done('paint', d)).toBe(false)
  })
  it('paint looks at the first map only', () => {
    const d = fresh()
    addTerrain(d)
    d.game!.maps.push({ id: 'second', name: 'Second', file: 'maps/second.json' })
    d.maps.second = structuredClone(d.maps.start!)
    const m2 = d.maps.second!
    m2.legend.w = 'water'
    m2.tiles[0] = 'wwww....'
    expect(done('paint', d)).toBe(false)
  })
})

describe('step: new-unit, new-weapon, give-weapon', () => {
  it('a second unit type ticks new-unit', () => {
    const d = fresh()
    expect(done('new-unit', d)).toBe(false)
    d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot', weapons: [] })
    expect(done('new-unit', d)).toBe(true)
  })
  it('a second weapon ticks new-weapon', () => {
    const d = fresh()
    d.weapons![0].name = 'Blade'
    expect(done('new-weapon', d)).toBe(false)
    d.weapons!.push({ id: 'bow', name: 'Bow' })
    expect(done('new-weapon', d)).toBe(true)
  })
  it('give-weapon needs a new unit with a weapon (the soldier already has one)', () => {
    const d = fresh()
    expect(done('give-weapon', d)).toBe(false)
    d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot', weapons: [] })
    expect(done('give-weapon', d)).toBe(false)
    d.units![1].weapons = ['sword']
    expect(done('give-weapon', d)).toBe(true)
  })
  it('its "Take me there" opens the new unit, not the soldier', () => {
    const d = fresh()
    expect(step('give-weapon').where!.item!(d)).toBeUndefined()
    d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot' })
    expect(step('give-weapon').where!.item!(d)).toBe('archer')
  })
})

describe('step: place-blue, place-red', () => {
  const withArcher = (): ProjectDoc => { const d = fresh(); d.units!.push({ id: 'archer', name: 'Archer', moveType: 'foot' }); return d }
  it('another soldier does not count; the new unit type does, per side', () => {
    const d = withArcher()
    d.maps.start!.units!.push({ type: 'soldier', side: 'blue', x: 0, y: 0 })
    expect(done('place-blue', d)).toBe(false)
    d.maps.start!.units!.push({ type: 'archer', side: 'red', x: 7, y: 0 })
    expect(done('place-blue', d)).toBe(false)
    expect(done('place-red', d)).toBe(true)
    d.maps.start!.units!.push({ type: 'archer', side: 'blue', x: 0, y: 1 })
    expect(done('place-blue', d)).toBe(true)
  })
  it('red is not ticked by blue units', () => {
    const d = withArcher()
    d.maps.start!.units!.push({ type: 'archer', side: 'blue', x: 0, y: 1 })
    expect(done('place-red', d)).toBe(false)
  })
})

describe('steps: play, problems (editor signals)', () => {
  it('follow their own signal and nothing else', () => {
    const d = fresh()
    expect(done('play', d, ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', d, ['played'])).toBe(true)
    expect(done('problems', d, ['played', 'saved'])).toBe(false)
    expect(done('problems', d, ['problemsChecked'])).toBe(true)
  })
})

describe('the lesson through the runner', () => {
  const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'practice' }
  it('ticks steps in order as the work is done, never ahead', () => {
    const d = fresh()
    // The maker does things out of order: nothing moves until the current step is done.
    addTerrain(d)
    paint(d, 'water', [[0, 0], [1, 0], [2, 0], [3, 0]])
    let p = advance(firstGame, start, d, 'game').progress
    expect(p.step).toBe(0)
    d.game!.name = 'Moonlit Valley'
    // The name step waits for its pause, then the two already-true steps tick at once and the lesson moves on.
    const waiting = advance(firstGame, p, d, 'game')
    expect(waiting.waiting).toBe(0)
    p = advance(firstGame, p, d, 'game', 0).progress
    expect(p.step).toBe(3)
  })
})
