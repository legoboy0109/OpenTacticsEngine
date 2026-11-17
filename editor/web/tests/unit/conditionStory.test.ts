// Status conditions in the story editor (docs/EVENTS.md "Status conditions"): the commands "Put a condition on units" / "Cure units of a condition" and the
// condition "A unit has a status condition": their starting values, their sentences, their checks, and what deleting a condition does to them (reference
// tracking). Map events only (the scene lists do not offer them).
import { describe, expect, it } from 'vitest'
import { conditionUsageLines, deleteCondition } from '../../src/model/conditionRefs'
import { canBeCured, canBeInflicted } from '../../src/model/conditionChecks'
import { newCommand, newCondition } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText, conditionText } from '../../src/model/storySummary'
import { commandKind, conditionKind, type StoryEvent } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadConditionsFixture } from './fixtures'

const withEvents = (events: StoryEvent[]): { doc: ProjectDoc; mapId: string } => {
  const doc = loadConditionsFixture()
  const mapId = Object.keys(doc.maps)[0]
  return { doc: { ...doc, maps: { ...doc.maps, [mapId]: { ...doc.maps[mapId]!, events } } }, mapId }
}
const ev = (id: string, ...cmds: object[]): StoryEvent => ({ id, when: { battle: 'start' }, do: cmds } as StoryEvent)
const ctx = (doc: ProjectDoc, mapId: string) => ({ doc, map: doc.maps[mapId]! })

describe('defaults, kinds and sentences', () => {
  it('a new command starts with the first condition and a unit filter; the kinds are found', () => {
    const { doc, mapId } = withEvents([])
    const c = ctx(doc, mapId)
    expect(newCommand(c as never, 'addCondition')).toMatchObject({ addCondition: 'poison', to: expect.any(Object) })
    expect(newCommand(c as never, 'removeCondition')).toMatchObject({ removeCondition: 'poison', from: expect.any(Object) })
    expect(newCondition(c as never, 'hasCondition')).toEqual({ hasCondition: 'poison' })
    expect(commandKind({ addCondition: 'poison', to: {} })).toBe('addCondition')
    expect(commandKind({ removeCondition: 'all', from: {} })).toBe('removeCondition')
    expect(conditionKind({ hasCondition: 'poison' })).toBe('hasCondition')
  })
  it('says what they do in plain words', () => {
    const { doc, mapId } = withEvents([])
    const c = ctx(doc, mapId) as never
    expect(commandText(c, { addCondition: 'poison', to: { side: 'blue' }, turns: 2 })).toMatch(/^Puts the condition “Poison” on .* for 2 turns$/)
    expect(commandText(c, { addCondition: 'poison', to: {} })).toMatch(/^Puts the condition “Poison” on /)
    expect(commandText(c, { removeCondition: 'all', from: {} })).toMatch(/of every condition$/)
    expect(commandText(c, { removeCondition: 'sleep', from: {} })).toMatch(/of the condition “Sleep”$/)
    expect(conditionText(c, { hasCondition: 'mark' })).toMatch(/has the condition “Mark”$/)
  })
})

describe('checks', () => {
  const run = (...cmds: object[]) => {
    const { doc, mapId } = withEvents([ev('e', ...cmds)])
    return storyChecks(doc, mapId).map((c) => c.text)
  }
  it('are quiet for good commands', () => {
    expect(run({ addCondition: 'poison', to: {}, turns: 2 }, { removeCondition: 'all', from: { side: 'blue' } })).toEqual([])
  })
  it('say what the loader refuses: an unknown condition, no units, bad turns, turns on a condition that does not count them', () => {
    expect(run({ addCondition: 'nothing', to: {} }).join()).toMatch(/“nothing” is not a status condition of this game/)
    expect(run({ addCondition: 'poison' }).join()).toMatch(/pick which units get the condition/)
    expect(run({ removeCondition: 'poison' }).join()).toMatch(/pick which units are cured/)
    expect(run({ addCondition: 'poison', to: {}, turns: 0 }).join()).toMatch(/from 1 to 99/)
    expect(run({ addCondition: 'mark', to: {}, turns: 2 }).join()).toMatch(/does not count turns/)
    expect(run({ if: [{ hasCondition: 'nothing' }], then: [] }).join()).toMatch(/“nothing” is not a status condition/)
    const { doc, mapId } = withEvents([ev('e', { addCondition: 'poison', to: {} })])
    expect(storyChecks({ ...doc, conditions: null }, mapId).map((c) => c.text).join()).toMatch(/the game has no conditions yet/)
  })
})

describe('reference tracking', () => {
  const events = [
    ev('a', { addCondition: 'poison', to: {} }, { say: 'hello' }, { removeCondition: 'poison', from: {} }, { removeCondition: 'all', from: {} }),
    { id: 'b', when: { battle: 'start' }, if: [{ hasCondition: 'poison' }], do: [{ say: 'poisoned' }] } as StoryEvent,
    ev('c', { if: [{ hasCondition: 'sleep', unit: { side: 'blue' } }], then: [{ addCondition: 'sleep', to: { side: 'red' }, turns: 2 }] }),
  ]
  it('lists the commands and events that would be removed', () => {
    const { doc } = withEvents(events)
    const lines = conditionUsageLines(doc, 'poison').map((u) => u.text).join('\n')
    expect(lines).toMatch(/command 1: “Puts the condition “Poison” on .*” is removed/)
    expect(lines).toMatch(/command 3: “Cures .* of the condition “Poison”” is removed/)
    expect(lines).toMatch(/event “b” is removed/)
    expect(lines).not.toMatch(/every condition/) // “all” names no condition
  })
  it('deleting removes them in the same edit and keeps "all"', () => {
    const { doc, mapId } = withEvents(events)
    const gone = deleteCondition(doc, 'poison')
    const m = gone.maps[mapId]!
    expect(m.events!.map((e) => e.id)).toEqual(['a', 'c'])
    expect(m.events![0].do).toEqual([{ say: 'hello' }, { removeCondition: 'all', from: {} }])
    expect(gone.conditions!.some((c) => c.id === 'poison')).toBe(false)
    // the sleep event is untouched
    expect(m.events![1]).toBe(doc.maps[mapId]!.events![2])
    const sleepGone = deleteCondition(doc, 'sleep')
    // the "if" command that asked about it goes (with what it ran); the event is left, with nothing to do
    expect(sleepGone.maps[mapId]!.events!.map((e) => e.id)).toEqual(['a', 'b', 'c'])
    expect(sleepGone.maps[mapId]!.events![2].do).toEqual([])
  })
  it('an event command counts as a way to inflict and to cure', () => {
    const { doc } = withEvents(events)
    expect(canBeInflicted(doc, 'sleep')).toBe(true)
    expect(canBeCured(doc, 'sleep')).toBe(true)
    const bare = withEvents([]).doc
    const own = { ...bare, items: bare.items!.filter((i) => i.kind !== 'staff' && i.kind !== 'usable'), weapons: bare.weapons!.map(({ inflict: _i, ...w }) => w) }
    expect(canBeInflicted(own, 'stun')).toBe(false)
    expect(canBeCured(own, 'poison')).toBe(false)
  })
})
