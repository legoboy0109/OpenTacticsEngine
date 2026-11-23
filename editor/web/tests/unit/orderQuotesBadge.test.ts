// Four small engine features in the editor's model (docs/EVENTS.md, BATTLE_SCENE.md, GRAPHICS.md): the beforePrep trigger, the actor as a target, the quote timing
// and "one quote per fight", and unit badges: defaults, sentences, the checks that mirror the loader.
import { describe, expect, it } from 'vitest'
import { QUOTE_DEFAULT, quoteTiming, sceneTimingProblems, setQuoteTiming } from '../../src/model/battleHold'
import { QUOTES_PER_FIGHT, quotesPerFight, setQuotesPerFight } from '../../src/model/battleTables'
import { addBadge, badgeProblems, badgesUsingSheet, moveBadge, patchBadge, removeBadge } from '../../src/model/badges'
import { removeSheet } from '../../src/model/graphics'
import { newFilter, newTrigger } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { filterText, triggerText } from '../../src/model/storySummary'
import { filterKind, triggerKind, type Filter } from '../../src/model/storyTypes'
import { groupOf } from '../../src/model/timelineEvents'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id
function mapWith(d: ProjectDoc, events: unknown[], objectives?: unknown): ProjectDoc {
  const m = d.maps[mapId(d)]!
  const next: MapFile = { ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm' } : u)), events: events as MapFile['events'], ...(objectives ? { objectives: objectives as MapFile['objectives'] } : {}) }
  return { ...d, maps: { ...d.maps, [mapId(d)]: next } }
}
const problems = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text).join(' | ')
const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })

describe('the beforePrep trigger', () => {
  const d = mapWith(loadDemo('demo-economy'), [])
  it('is its own kind, reads as a sentence, and sits with the battle start in the timeline', () => {
    const t = newTrigger(ctx(d), 'beforePrep')
    expect(t).toEqual({ battle: 'beforePrep' })
    expect(triggerKind(t)).toBe('beforePrep')
    expect(triggerText(ctx(d), t)).toBe('Before the preparation screen')
    expect(groupOf('beforePrep')).toBe('start')
    expect(triggerKind({ battle: 'start' })).toBe('start')
  })
  it('refuses, like the loader, the commands that make no sense before deployment (also nested)', () => {
    const ev = (cmds: unknown[]) => mapWith(d, [{ id: 'p', when: { battle: 'beforePrep' }, do: cmds }])
    expect(problems(ev([{ say: 'Hello' }, { setSwitch: 'x' }, { focus: { x: 1, y: 1 } }]))).not.toContain('makes no sense')
    for (const c of [{ win: true }, { lose: true }, { endTurn: true }, { fight: { attacker: 'gorm', defender: 'gorm' } }])
      expect(problems(ev([c])), JSON.stringify(c)).toContain('makes no sense before the preparation screen')
    expect(problems(ev([{ if: [{ switch: 'x' }], then: [{ win: true }] }]))).toContain('“win” makes no sense')
    const start = mapWith(d, [{ id: 'p', when: { battle: 'start' }, do: [{ win: true }] }])
    expect(problems(start)).not.toContain('makes no sense')
  })
})

describe('the actor as a target', () => {
  const d = mapWith(loadDemo('demo-economy'), [])
  it('is a filter kind of its own: { actor: true }', () => {
    const f = newFilter(ctx(d), 'actor')
    expect(f).toEqual({ actor: true })
    expect(filterKind(f)).toBe('actor')
    expect(filterKind({ unit: 'gorm' } as Filter)).toBe('unit')
    expect(filterText(ctx(d), f)).toBe('the unit that set the event off')
  })
  it('is fine in a command of an event with a unit that sets it off, refused elsewhere', () => {
    const give = [{ giveItem: 'x', to: { actor: true } }]
    const reach = mapWith(d, [{ id: 'a', when: { reach: { x: 1, y: 1, w: 1, h: 1 } }, do: give }])
    expect(problems(reach)).not.toContain('set the event off')
    for (const when of [{ battle: 'start' }, { battle: 'beforePrep' }, { turn: 2 }, { battle: 'won' }])
      expect(problems(mapWith(d, [{ id: 'a', when, do: give }])), JSON.stringify(when)).toContain('nothing here sets it off with a unit')
    const nested = mapWith(d, [{ id: 'a', when: { turn: 1 }, do: [{ if: [{ switch: 'x' }], then: give }] }])
    expect(problems(nested)).toContain('nothing here sets it off with a unit')
  })
  it('is refused in a trigger, an event’s own condition and the objectives', () => {
    expect(problems(mapWith(d, [{ id: 'a', when: { reach: { x: 1, y: 1, w: 1, h: 1 }, by: { actor: true } }, do: [{ setSwitch: 'x' }] }]))).toContain('which only a command of an event')
    expect(problems(mapWith(d, [{ id: 'a', when: { fight: {} }, if: [{ alive: { actor: true } }], do: [{ setSwitch: 'x' }] }]))).toContain('which only a command of an event')
    expect(problems(mapWith(d, [], { win: [{ defeated: { actor: true } }] }))).toContain('which only a command of an event')
  })
})

describe('quote timing and one quote per fight', () => {
  const base = { game: {}, files: {}, graphics: { sheets: [], battle: { sets: {} } } } as unknown as ProjectDoc
  it('writes only what differs from the defaults and removes an emptied section', () => {
    expect(quoteTiming(base)).toEqual(QUOTE_DEFAULT)
    let d = setQuoteTiming(base, 'slideOutMs', 500)
    expect(d.graphics!.battle!.scene).toEqual({ quote: { slideOutMs: 500 } })
    expect(quoteTiming(d).slideOutMs).toBe(500)
    d = setQuoteTiming(d, 'slideOutMs', QUOTE_DEFAULT.slideOutMs)
    expect(d.graphics!.battle!.scene).toBeUndefined()
  })
  it('checks the numbers like the loader', () => {
    const bad = (q: unknown) => sceneTimingProblems({ ...base, graphics: { sheets: [], battle: { sets: {}, scene: { quote: q } } } } as unknown as ProjectDoc).join(' | ')
    expect(bad({ leadMs: 0, slideOutMs: 5000, slideInMs: 1 })).toBe('')
    expect(bad({ leadMs: -1 })).toContain('whole number from 0 to 5000')
    expect(bad({ slideInMs: 1.5 })).toContain('whole number')
    expect(bad({ nope: 1 })).toContain('does not know')
  })
  it('keeps "both" as no key and "one" as the key', () => {
    expect(QUOTES_PER_FIGHT).toEqual(['both', 'one'])
    expect(quotesPerFight(base)).toBe('both')
    const one = setQuotesPerFight(base, 'one')
    expect(one.game!.quotesPerFight).toBe('one')
    expect('quotesPerFight' in setQuotesPerFight(one, 'both').game!).toBe(false)
  })
})

describe('unit badges', () => {
  const art = { game: {}, files: {}, units: [{ id: 'knight', tags: ['boss'] }], characters: [{ id: 'eirik', tags: ['hero'] }], graphics: { sheets: [{ id: 'marks', file: 'art/marks.png', cellWidth: 8, cellHeight: 8 }, { id: 'other', file: 'art/o.png', cellWidth: 8, cellHeight: 8 }] } } as unknown as ProjectDoc
  it('adds a badge with the defaults a boss marker wants and writes no default keys when edited', () => {
    let d = addBadge(art)
    expect(d.graphics!.badges).toEqual([{ id: 'badge', sheet: 'marks', tag: 'boss', sides: 'computer', anchor: 'bottomRight', offset: [0, 0] }])
    d = patchBadge(d, 0, { sides: 'all', anchor: 'bottomRight', offset: [0, 0] })
    expect(d.graphics!.badges).toEqual([{ id: 'badge', sheet: 'marks', tag: 'boss' }])
    d = patchBadge(d, 0, { offset: [1, -1], anchor: 'topLeft', cell: [2, 0] })
    expect(d.graphics!.badges![0]).toMatchObject({ offset: [1, -1], anchor: 'topLeft', cell: [2, 0] })
    d = patchBadge(d, 0, { cell: undefined })
    expect('cell' in d.graphics!.badges![0]).toBe(false)
    expect(addBadge(d).graphics!.badges!.map((b) => b.id)).toEqual(['badge', 'badge2'])
  })
  it('does nothing without an image', () => {
    expect(addBadge({ ...art, graphics: { sheets: [] } } as unknown as ProjectDoc).graphics!.badges).toBeUndefined()
  })
  it('moves and removes, and goes with its image', () => {
    let d = addBadge(addBadge(art))
    d = patchBadge(d, 1, { sheet: 'other', tag: 'hero' })
    expect(moveBadge(d, 1, 0).graphics!.badges!.map((b) => b.id)).toEqual(['badge2', 'badge'])
    expect(badgesUsingSheet(d, 'other')).toEqual(['badge2'])
    expect(removeSheet(d, 'other').graphics!.badges!.map((b) => b.id)).toEqual(['badge'])
    expect(removeBadge(removeBadge(d, 0), 0).graphics!.badges).toBeUndefined()
  })
  it('checks what the loader refuses', () => {
    const one = (b: Record<string, unknown>) => badgeProblems({ ...art, graphics: { ...art.graphics!, badges: [{ id: 'b', sheet: 'marks', tag: 'boss', ...b }] } } as unknown as ProjectDoc).join(' | ')
    expect(one({})).toBe('')
    expect(one({ tag: 'hero', sides: ['red', 'blue'], anchor: 'center', offset: [-256, 256], cell: [0, 1] })).toBe('')
    expect(one({ id: 'a b' })).toContain('letters, digits')
    expect(one({ sheet: 'nope' })).toContain('pick one of the game’s images')
    expect(one({ tag: '' })).toContain('type the tag')
    expect(one({ tag: 'ghost' })).toContain('no unit type and no character has the tag “ghost”')
    expect(one({ sides: 'everyone' })).toContain('“whose units”')
    expect(one({ sides: [] })).toContain('“whose units”')
    expect(one({ anchor: 'middle' })).toContain('corner')
    expect(one({ offset: [300, 0] })).toContain('offset')
    expect(one({ offset: [1.5, 0] })).toContain('offset')
    expect(one({ cell: [-1, 0] })).toContain('cell')
    const dup = badgeProblems({ ...art, graphics: { ...art.graphics!, badges: [{ id: 'b', sheet: 'marks', tag: 'boss' }, { id: 'b', sheet: 'marks', tag: 'boss' }] } } as unknown as ProjectDoc)
    expect(dup.join(' | ')).toContain('same name')
  })
})

describe('map markers', () => {
  const art = { game: {}, files: {}, graphics: { sheets: [{ id: 'marks', file: 'art/m.png', cellWidth: 8, cellHeight: 8 }] } } as unknown as ProjectDoc
  it('adds, edits without writing defaults, removes, and goes with its image', async () => {
    const { addMarker, patchMarker, removeMarker, markersUsingSheet } = await import('../../src/model/markers')
    let d = addMarker(art)
    expect(d.graphics!.markers).toEqual([{ id: 'interest', sheet: 'marks' }])
    d = patchMarker(d, 0, { anchor: 'topLeft', offset: [1, -1], cell: [1, 0] })
    expect(d.graphics!.markers![0]).toMatchObject({ anchor: 'topLeft', offset: [1, -1], cell: [1, 0] })
    d = patchMarker(d, 0, { anchor: 'center', offset: [0, 0], cell: undefined })
    expect(d.graphics!.markers).toEqual([{ id: 'interest', sheet: 'marks' }])
    expect(markersUsingSheet(d, 'marks')).toEqual(['interest'])
    expect(removeSheet(d, 'marks').graphics!.markers).toBeUndefined()
    expect(removeMarker(d, 0).graphics!.markers).toBeUndefined()
  })
  it('checks the list and the event key like the loader', async () => {
    const { markerProblems, eventMarkerProblem, addMarker } = await import('../../src/model/markers')
    const d = addMarker(art)
    expect(markerProblems(d)).toEqual([])
    expect(markerProblems({ ...d, graphics: { ...d.graphics!, markers: [{ id: 'a b', sheet: 'nope', anchor: 'middle' as never }] } }).join('|')).toContain('letters, digits')
    expect(eventMarkerProblem(d, { when: { reach: { x: 0, y: 0, w: 1, h: 1 } }, marker: 'interest' })).toBeNull()
    expect(eventMarkerProblem(d, { when: { reach: { x: 0, y: 0, w: 1, h: 1 } }, marker: 'ghost' })).toContain('not a marker')
    expect(eventMarkerProblem(d, { when: { battle: 'start' }, marker: 'interest' })).toContain('reach an area')
    expect(eventMarkerProblem(d, { when: { turn: 1 } })).toBeNull()
  })
})
