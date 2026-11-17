// Roster conditions (rosterAlive / rosterDead / inRoster) and the showUnit command in map events and campaign scenes: their summaries, their checks, and that
// renaming or deleting a character follows into them like every other reference (docs/EVENTS.md "Campaign characters in map events").
import { describe, expect, it } from 'vitest'
import { renameCharacter } from '../../src/model/characterRename'
import { deleteCharacter } from '../../src/model/srpgRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText, conditionText } from '../../src/model/storySummary'
import type { StoryEvent } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadRenameFixture } from './fixtures'

const events = (id: string): StoryEvent[] => [{
  id,
  when: { fight: { unit: 'g1' } },
  if: [{ rosterAlive: 'hero' }, { rosterDead: 'rival', party: 'active' }, { inRoster: 'hero' }],
  do: [
    { showUnit: 'g1', x: 2, y: 1 },
    { if: [{ inRoster: 'hero' }], then: [{ showUnit: 'g2', x: 3, y: 1 }] },
  ],
}]

const withRoster = (): ProjectDoc => {
  const d = loadRenameFixture()
  const field = d.maps.field!
  return { ...d, maps: { ...d.maps, field: { ...field, events: [...(field.events ?? []), ...events('roster_event')] } } }
}
const roster = (d: ProjectDoc): StoryEvent | undefined => d.maps.field!.events?.find((e) => e.id === 'roster_event')
const text = (d: unknown): string => JSON.stringify(d)
const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps.field! }) as Parameters<typeof conditionText>[0]

describe('summaries', () => {
  it('say who and, for a party, whose roster', () => {
    const d = withRoster()
    const c = ctx(d)
    expect(conditionText(c, { rosterAlive: 'hero' })).toMatch(/is alive in the campaign$/)
    expect(conditionText(c, { rosterDead: 'rival', party: 'active' })).toMatch(/has fallen in the campaign \(active party\)$/)
    expect(conditionText(c, { inRoster: 'hero' })).toMatch(/is in the roster$/)
    expect(conditionText(c, { inRoster: 'hero', party: 'any' })).not.toMatch(/party/)
  })
  it('say where a hidden unit comes back', () => {
    const c = ctx(withRoster())
    expect(commandText(c, { showUnit: 'g1', x: 2, y: 1 })).toMatch(/^Brings back .* at 3, 2$/)
  })
})

describe('checks', () => {
  it('accept known characters and a named unit on the map', () => {
    const list = storyChecks(withRoster(), 'field').filter((x) => x.eventId === 'roster_event')
    expect(list).toEqual([])
  })
  it('refuse a character that is not in the game, an empty pick, a unit the map does not name and a square outside the map', () => {
    const d = withRoster()
    const bad: StoryEvent = {
      id: 'roster_event', when: { fight: { unit: 'g1' } },
      if: [{ rosterAlive: 'nobody' }, { inRoster: '' }],
      do: [{ showUnit: 'ghost', x: 1, y: 1 }, { showUnit: 'g1', x: 99, y: 1 }],
    }
    const field = d.maps.field!
    const doc = { ...d, maps: { ...d.maps, field: { ...field, events: [...field.events!.filter((e) => e.id !== 'roster_event'), bad] } } }
    const texts = storyChecks(doc, 'field').filter((x) => x.eventId === 'roster_event').map((x) => x.text).join('\n')
    expect(texts).toMatch(/“nobody” is not a character of this game/)
    expect(texts).toMatch(/pick a character/)
    expect(texts).toMatch(/“ghost” is not a named unit of this map/)
    expect(texts).toMatch(/square is outside the map/)
  })
})

describe('renaming a character', () => {
  it('follows into the roster conditions and the showUnit of a unit with that id', () => {
    const d = withRoster()
    const r = renameCharacter(d, 'hero', 'captain')
    expect(r.error).toBeNull()
    const e = roster(r.doc)!
    expect(e.if).toEqual([{ rosterAlive: 'captain' }, { rosterDead: 'rival', party: 'active' }, { inRoster: 'captain' }])
    expect(text(e.do)).toContain('"inRoster":"captain"')
    expect(text(r.doc)).not.toContain('"hero"')
  })
  it('renames a showUnit that names the character’s unit', () => {
    const d = withRoster()
    const field = d.maps.field!
    const doc = { ...d, maps: { ...d.maps, field: { ...field, events: field.events!.map((e) => (e.id === 'roster_event' ? { ...e, do: [{ showUnit: 'hero', x: 0, y: 0 }] } : e)) } } }
    expect(roster(renameCharacter(doc, 'hero', 'captain').doc)!.do).toEqual([{ showUnit: 'captain', x: 0, y: 0 }])
  })
})

describe('deleting a character', () => {
  it('takes the roster conditions and commands that name it away (the smallest thing that cannot work: an event whose condition it was)', () => {
    const after = deleteCharacter(withRoster(), 'hero')
    expect(text(after.maps.field)).not.toContain('"rosterAlive":"hero"')
    expect(text(after.maps.field)).not.toContain('"inRoster":"hero"')
  })
  it('keeps the ones about other characters', () => {
    const d = withRoster()
    const field = d.maps.field!
    const only: StoryEvent = { id: 'roster_event', when: { fight: { unit: 'g1' } }, if: [{ rosterDead: 'rival' }], do: [{ endTurn: true }] }
    const doc = { ...d, maps: { ...d.maps, field: { ...field, events: [...field.events!.filter((e) => e.id !== 'roster_event'), only] } } }
    expect(roster(deleteCharacter(doc, 'hero'))!.if).toEqual([{ rosterDead: 'rival' }])
  })
})
