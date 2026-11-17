// The computer unit's behaviour in the editor (docs/AI.md, docs/EDITOR_UI.md "Behaviour"): the block's short form, mode switches, the checks, the story
// commands (defaults, sentence, walk) and the follow-through when a unit or a place is renamed or deleted. Uses the engine's fixture tests/fixtures/srpg-doors.
import { describe, expect, it } from 'vitest'
import {
  behaviourChecks, behaviourMark, behaviourObj, behaviourPhrase, contextOf, dropBehaviourUnit, groupCounts, groupNames, isDefaultBehaviour, renameBehaviourUnit,
  renameBehaviourPlace, tidyBehaviour, withMode, withSeekKind,
} from '../../src/model/behaviour'
import { changeUnitStoryId, removeUnitAndStory } from '../../src/model/storyUnits'
import { storyChecks } from '../../src/model/storyChecks'
import { newCommand } from '../../src/model/storyDefaults'
import { commandText } from '../../src/model/storySummary'
import { commandKind } from '../../src/model/storyTypes'
import type { MapFile, MapUnit, ProjectDoc } from '../../src/model/types'
import { parseLoose } from './fixtures'
import { readFileSync } from 'node:fs'
import path from 'node:path'

const fixture = (): ProjectDoc => {
  const root = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-doors')
  const read = (rel: string) => parseLoose(readFileSync(path.join(root, rel), 'utf8'))
  const game = read('game.json')
  const maps: ProjectDoc['maps'] = {}
  for (const m of game.maps) maps[m.id] = read(m.file)
  return {
    game, terrain: read('data/terrain.json').terrain, structures: read('data/structures.json').structures, items: read('data/items.json').items, research: null,
    weapons: read('data/weapons.json').weapons, units: read('data/units.json').units, abilities: read('data/abilities.json').abilities, maps,
  } as unknown as ProjectDoc
}
const withUnits = (d: ProjectDoc, f: (u: MapUnit[]) => MapUnit[]): ProjectDoc => ({ ...d, maps: { ...d.maps, market: { ...d.maps.market!, units: f(d.maps.market!.units!) } } })
const texts = (d: ProjectDoc) => behaviourChecks(d, 'market', d.maps.market!).map((c) => c.text)

describe('the short form of a block', () => {
  it('writes nothing for the default, a bare name for a lone hold or guard, an object otherwise', () => {
    expect(tidyBehaviour({})).toBeUndefined()
    expect(tidyBehaviour({ mode: 'advance' })).toBeUndefined()
    expect(tidyBehaviour({ mode: 'hold' })).toBe('hold')
    expect(tidyBehaviour({ mode: 'guard' })).toBe('guard')
    expect(tidyBehaviour({ mode: 'guard', wake: 'once' })).toBe('guard')
    expect(tidyBehaviour({ mode: 'guard', wake: 'react', group: 'pack' })).toEqual({ mode: 'guard', wake: 'react', group: 'pack' })
    expect(tidyBehaviour({ fromTurn: 1 })).toBeUndefined()
    expect(tidyBehaviour({ fromTurn: 3 })).toEqual({ fromTurn: 3 })
    expect(tidyBehaviour({ mode: 'seek', x: 2, y: 3 })).toEqual({ mode: 'seek', x: 2, y: 3 })
  })
  it('reads a name or an object the same way and keeps keys it does not know', () => {
    expect(behaviourObj('hold')).toEqual({ mode: 'hold' })
    expect(behaviourObj(undefined)).toEqual({})
    expect(tidyBehaviour({ mode: 'hold', later: 7 })).toEqual({ mode: 'hold', later: 7 })
    expect(isDefaultBehaviour(undefined)).toBe(true)
    expect(isDefaultBehaviour('hold')).toBe(false)
  })
  it('drops the keys a mode does not use when the mode changes, and starts the new one sensibly', () => {
    const d = fixture()
    const ctx = contextOf(d, d.maps.market!)
    const g = withMode({ mode: 'guardArea', area: { x: 1, y: 1, w: 2, h: 2 }, within: 4, group: 'pack', fromTurn: 3 }, 'guard', ctx)
    expect(tidyBehaviour(g)).toEqual({ mode: 'guard', group: 'pack', fromTurn: 3 })
    expect(withMode({}, 'guardArea', ctx).within).toBe(3)
    const s = withMode({ mode: 'hold' }, 'seek', ctx)
    expect(s.place).toBe('c1') // the first placed structure with a story name
    expect(withSeekKind(s, 'unit', ctx).unit).toBe('lord')
    expect(tidyBehaviour(withSeekKind(s, 'square', ctx))).toMatchObject({ mode: 'seek', x: 5, y: 2 })
  })
})

describe('the marks and sentences', () => {
  it('has a letter for every non-default behaviour', () => {
    expect(behaviourMark(undefined)).toBeNull()
    expect(behaviourMark('hold')).toBe('H')
    expect(behaviourMark({ mode: 'guardArea', within: 2 })).toBe('G')
    expect(behaviourMark({ mode: 'seek', x: 1, y: 1 })).toBe('S')
    expect(behaviourMark({ fromTurn: 4 })).toBe('T')
  })
  it('says what the units are told in plain words', () => {
    expect(behaviourPhrase('hold')).toBe('stay where they are')
    expect(behaviourPhrase({ mode: 'seek', unit: 'lord', fromTurn: 3 })).toBe('head for lord, from turn 3')
    expect(behaviourPhrase({ mode: 'guard', group: 'pack' })).toBe('wait until someone comes into range (group pack)')
  })
})

describe('the checks', () => {
  it('has nothing to say about the fixture, and a clean block says nothing', () => {
    const d = withUnits(fixture(), (u) => u.map((x, i) => (i === 1 ? { ...x, behaviour: 'hold' } : x)))
    expect(texts(d).filter((t) => !/can never do anything/.test(t))).toEqual([])
  })
  it('says what the loader refuses: an unknown unit or place, a square outside the map, a start turn past the bound, a person’s side', () => {
    const d = withUnits(fixture(), (u) => [
      { ...u[0], behaviour: 'hold' },
      { ...u[1], behaviour: { mode: 'seek', unit: 'ghost' } },
      { ...u[2], behaviour: { mode: 'seek', place: 'tower', prefer: { unit: 'phantom' } } },
      { ...u[3], behaviour: { mode: 'seek', x: 40, y: 1, fromTurn: 150 } },
    ])
    const t = texts(d).join('\n')
    expect(t).toMatch(/this unit is on a side a person plays/)
    expect(t).toMatch(/“ghost”, which is not a named unit/)
    expect(t).toMatch(/“tower”, which is not a structure/)
    expect(t).toMatch(/“phantom”, which is not a named unit/)
    expect(t).toMatch(/outside the map/)
    expect(t).toMatch(/from 1 to 99/)
  })
  it('notes a group of one, a start turn that is far too late and a hold with nothing to do', () => {
    const d = withUnits(fixture(), (u) => u.map((x, i) => (i === 1 ? { ...x, behaviour: { mode: 'guard', group: 'solo', fromTurn: 60 } } : x)))
    const t = texts(d).join('\n')
    expect(t).toMatch(/only unit in the group “solo”/)
    expect(t).toMatch(/does nothing before turn 60/)
    const mute = { ...d, units: d.units!.map((x) => (x.id === 'raider' ? { ...x, weapons: [] } : x)), items: [] }
    expect(texts(withUnits(mute, (u) => u.map((x, i) => (i === 2 ? { ...x, items: [], behaviour: 'hold' } : x)))).join('\n')).toMatch(/can never do anything/)
  })
  it('counts the groups of units and of addUnit commands', () => {
    const d = withUnits(fixture(), (u) => u.map((x, i) => (i === 1 || i === 2 ? { ...x, behaviour: { mode: 'guard', group: 'pack' } } : x)))
    expect(groupCounts(d.maps.market!).get('pack')).toBe(2)
    expect(groupNames(d.maps.market!)).toEqual(['pack'])
    expect(texts(d).join('\n')).not.toMatch(/only unit in the group/)
  })
})

describe('the story commands', () => {
  const d0 = (): ProjectDoc => withUnits(fixture(), (u) => u.map((x, i) => (i === 1 ? { ...x, behaviour: { mode: 'guard', group: 'pack' } } : x)))
  it('are made with a sensible start and told apart from every other command', () => {
    const d = d0()
    const c = { doc: d, map: d.maps.market! }
    const set = newCommand(c as never, 'setBehaviour')
    expect(commandKind(set)).toBe('setBehaviour')
    expect(set).toEqual({ setBehaviour: 'hold', units: { side: 'red' } })
    expect(newCommand(c as never, 'wakeGroup')).toEqual({ wakeGroup: 'pack' })
    expect(commandText({ doc: d, map: d.maps.market! } as never, { setBehaviour: { mode: 'seek', unit: 'lord' }, units: { side: 'red' } })).toMatch(/^Tells .* to head for lord$/)
    expect(commandText({ doc: d, map: d.maps.market! } as never, { wakeGroup: 'pack' })).toBe('Wakes the group “pack”')
  })
  it('are checked: who gets it, a side a person plays, a group nobody is in', () => {
    const d = d0()
    const bad: MapFile = {
      ...d.maps.market!,
      events: [{ id: 'e', when: { turn: 2 }, do: [
        { setBehaviour: 'hold', units: { side: 'blue' } }, { setBehaviour: { mode: 'seek', unit: 'ghost' } as never, units: {} }, { wakeGroup: 'nobody' }, { wakeGroup: 'pack' },
      ] }],
    }
    const t = storyChecks({ ...d, maps: { ...d.maps, market: bad } }, 'market').map((x) => x.text).join('\n')
    expect(t).toMatch(/played by a person/)
    expect(t).toMatch(/“ghost”/)
    expect(t).toMatch(/no unit of this map is in a group called “nobody”/)
    expect(t).not.toMatch(/“pack”/)
  })
})

describe('renames and deletes', () => {
  const seeking = (): ProjectDoc => withUnits(fixture(), (u) => u.map((x, i) => (i === 1 ? { ...x, behaviour: { mode: 'seek', unit: 'lord', prefer: { unit: 'lord' } } } : x)))
  it('a unit that is headed for or preferred is followed when it is renamed', () => {
    const d = seeking()
    const m = renameBehaviourUnit(d.maps.market!, 'lord', 'king')
    expect(m.units![1].behaviour).toEqual({ mode: 'seek', unit: 'king', prefer: { unit: 'king' } })
    expect(renameBehaviourUnit(m, 'nobody', 'x')).toBe(m)
    const via = changeUnitStoryId(d, 'market', 10, 0, 'king').doc
    expect((via.maps.market!.units![1].behaviour as { unit: string }).unit).toBe('king')
  })
  it('when the unit is deleted a seeker goes back to advancing and a preference is dropped', () => {
    const d = seeking()
    const m = dropBehaviourUnit(d.maps.market!, 'lord')
    expect('behaviour' in m.units![1]).toBe(false)
    const gone = removeUnitAndStory(d, 'market', 10, 0)
    expect(gone.maps.market!.units!.some((u) => u.id === 'lord')).toBe(false)
    expect('behaviour' in gone.maps.market!.units![0]).toBe(false)
  })
  it('a place that is renamed is followed', () => {
    const d = withUnits(fixture(), (u) => u.map((x, i) => (i === 1 ? { ...x, behaviour: { mode: 'seek', place: 'c1' } } : x)))
    expect(renameBehaviourPlace(d.maps.market!, 'c1', 'chest_a').units![1].behaviour).toEqual({ mode: 'seek', place: 'chest_a' })
  })
})
