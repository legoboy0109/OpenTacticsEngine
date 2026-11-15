// Every done test of the lesson "Items" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { advance } from '../../src/learn/runner'
import { LESSONS } from '../../src/learn/lessons'
import { items } from '../../src/learn/lessons/items'
import type { SignalName } from '../../src/learn/signals'
import type { LessonProgress } from '../../src/learn/state'
import type { Item, ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const fresh = (): ProjectDoc => { const d = structuredClone(loadTemplate('blank')); d.game!.name = 'Practice: Items'; return d }
const NONE: ReadonlySet<SignalName> = new Set()
const step = (id: string) => items.steps.find((s) => s.id === id)!
const done = (id: string, doc: ProjectDoc, signals: SignalName[] = []) => step(id).done(doc, 'game', new Set(signals))
const potion = (patch: Partial<Item> = {}): Item => ({ id: 'potion', name: 'Potion', kind: 'usable', uses: 3, heal: 10, ...patch })
const withPotion = (patch: Partial<Item> = {}): ProjectDoc => { const d = fresh(); d.items = [potion(patch)]; return d }

describe('lesson data', () => {
  it('is registered, with unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(items)
    expect(new Set(items.steps.map((s) => s.id)).size).toBe(items.steps.length)
    expect(items.steps.length).toBeGreaterThanOrEqual(5)
    expect(items.steps.length).toBeLessThanOrEqual(10)
    expect(items.template).toBe('blank')
    expect(items.playable).toBe(true)
    for (const s of items.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
  })
  it('uses no programmer words in the texts, and never calls the style a "look"', () => {
    const all = [items.title, items.description, ...items.summary, ...items.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('starts from the blank template as the steps assume: no items, one soldier type', () => {
    const d = fresh()
    expect(d.items ?? []).toEqual([])
    expect(d.units!.map((u) => u.id)).toEqual(['soldier'])
    expect(d.units![0].items).toBeUndefined()
    expect(d.maps.start!.units!.map((u) => u.side).sort()).toEqual(['blue', 'red'])
  })
  it('has no step that is already true at the start', () => {
    for (const s of items.steps) expect(s.done(fresh(), 'game', NONE), s.id).toBe(false)
  })
})

describe('steps: new-item, heal', () => {
  it('a new item ticks new-item', () => {
    expect(done('new-item', fresh())).toBe(false)
    expect(done('new-item', withPotion())).toBe(true)
  })
  it('heal needs another amount than the 10 a new item has', () => {
    expect(done('heal', withPotion())).toBe(false)
    expect(done('heal', withPotion({ heal: 10, uses: 5 }))).toBe(false) // changing something else does not count
    expect(done('heal', withPotion({ heal: 15 }))).toBe(true)
    expect(done('heal', withPotion({ heal: 'full' }))).toBe(true)
    expect(done('heal', withPotion({ heal: undefined }))).toBe(false)
  })
  it('settles before ticking, so typing "15" is not cut off at "1"', () => {
    expect(step('heal').settleMs).toBeGreaterThan(0)
  })
  it('its "Take me there" opens the new item, or nothing while there is none', () => {
    expect(step('heal').where!.item!(fresh())).toBeUndefined()
    expect(step('heal').where!.item!(withPotion())).toBe('potion')
  })
})

describe('steps: give, drop', () => {
  it('giving needs an item of the game in a unit type\'s starting items', () => {
    const d = withPotion()
    expect(done('give', d)).toBe(false)
    d.units![0].items = ['ghost'] // not an item of the game
    expect(done('give', d)).toBe(false)
    d.units![0].items = ['potion']
    expect(done('give', d)).toBe(true)
    d.units![0].items = [{ item: 'potion', uses: 2 }]
    expect(done('give', d)).toBe(true)
  })
  it('giving to a unit on the map only (not the type) does not count', () => {
    const d = withPotion()
    d.maps.start!.units![0].items = ['potion']
    expect(done('give', d)).toBe(false)
  })
  it('drop needs the "Drops when defeated" box on an item the unit type carries', () => {
    const d = withPotion()
    d.units![0].items = ['potion']
    expect(done('drop', d)).toBe(false)
    d.units![0].items = [{ item: 'potion', drop: false }]
    expect(done('drop', d)).toBe(false)
    d.units![0].items = [{ item: 'potion', drop: true }]
    expect(done('drop', d)).toBe(true)
  })
  it('both steps open the soldier', () => {
    expect(step('give').where!.item!(fresh())).toBe('soldier')
    expect(step('drop').where!.item!(fresh())).toBe('soldier')
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
  it('ticks the steps in order as the work is done, never ahead', () => {
    const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'practice' }
    const d = withPotion({ heal: 20 })
    d.units![0].items = [{ item: 'potion', drop: true }]
    // The heal step waits for its pause; after it the other data steps tick at once and Play waits for its signal.
    expect(advance(items, start, d, 'game').waiting).toBe(1)
    expect(advance(items, start, d, 'game', 1).progress.step).toBe(4)
  })
})
