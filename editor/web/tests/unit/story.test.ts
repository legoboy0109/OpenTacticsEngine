// Story events in the editor's model: add / edit / delete / reorder, nesting, id rename, clean-up on delete, summaries.
import { describe, expect, it } from 'vitest'
import { addItem } from '../../src/model/items'
import { cleanUnit, findUsages } from '../../src/model/references'
import { removeResource } from '../../src/model/rulesLists'
import { ctxOf } from '../../src/model/mapCtx'
import { deleteItem } from '../../src/model/items'
import { newCommand, newCondition, newRule, newTrigger } from '../../src/model/storyDefaults'
import {
  addCommand, addEvent, deleteEvent, depthOf, getList, listAdd, listMove, moveCommand, moveEvent, newEventId, patchCommand, patchEvent,
  patchObjectives, removeCommand, renameEvent, replaceCommand, setEventConditions, setEventRepeat, setObjectives, setTriggerKind, setUnitStoryId,
  type ListPath,
} from '../../src/model/storyEdit'
import { suggestUnitId, namedUnits, idProblem, teamsOf } from '../../src/model/storyIds'
import { addStoryItem, moveStoryItem, newStoryId, removeStoryItem, renameStoryItem, storyItemUsages } from '../../src/model/storyLists'
import { eventOverlays, objectiveOverlays, rectBetween } from '../../src/model/storyOverlays'
import { renameUnitRefs, scrubMap, storyUsages } from '../../src/model/storyRefs'
import { commandText, conditionText, eventSummary, filterText, ruleText, triggerText } from '../../src/model/storySummary'
import { changeUnitStoryId, removeSideAndStory, removeUnitAndStory, sideRemovalUsages, unitRemovalUsages } from '../../src/model/storyUnits'
import { storyChecks } from '../../src/model/storyChecks'
import { addResource } from '../../src/model/rulesLists'
import type { Command, StoryEvent } from '../../src/model/storyTypes'
import { addSide } from '../../src/model/mapSides'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const map = (d: ProjectDoc) => d.maps[mapId(d)]!
const withMap = (d: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc => ({ ...d, maps: { ...d.maps, [mapId(d)]: fn(map(d)) } })
const dctx = (d: ProjectDoc) => ({ doc: d, map: map(d) })

/** The economy demo with a story name on its first two units, a switch, a variable, an item and a resource. */
function base(): ProjectDoc {
  let d = loadDemo('demo-economy')
  d = withMap(d, (m) => ({ ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm', name: 'Gorm' } : i === 1 ? { ...u, id: 'lyra' } : u)) }))
  d = addStoryItem(d, 'switches', 'Village saved')
  d = addStoryItem(d, 'variables', 'Gold found')
  d = addItem(d, 'Potion', 'potion')
  return d
}

const sideA = (d: ProjectDoc) => map(d).sides![0].id
const sideB = (d: ProjectDoc) => map(d).sides![1].id

/** An event of every trigger kind and a command of every kind (a nested if three levels deep). */
function rich(): ProjectDoc {
  let d = base()
  const c = dctx(d)
  const res = d.game!.resources![0].id
  const type = d.units![0].id
  const cmds: Command[] = [
    { say: 'Now! Strike from the forest!', speaker: 'gorm' },
    { say: 'Hello', speakerName: 'Villager', portrait: 'villager' },
    { addUnit: { type, side: sideB(d), x: 1, y: 1, id: 'scout', items: ['potion'] } },
    { removeUnit: { unit: 'scout' } },
    { moveUnit: 'lyra', x: 2, y: 2 },
    { changeSide: { unit: 'lyra' }, side: sideA(d) },
    { giveItem: 'potion', to: { unit: 'gorm' } },
    { giveResource: { [res]: 100 }, side: sideA(d) },
    { setSwitch: 'village_saved', to: true },
    { setVariable: 'gold_found', to: 3 },
    { addVariable: 'gold_found', by: 1 },
    { objective: { text: 'Hold on', win: [{ round: 5 }] } },
    { win: sideA(d) },
    { lose: sideB(d) },
    { focus: { x: 3, y: 3 } },
    { if: [{ switch: 'village_saved' }], then: [{ if: [{ variable: 'gold_found', op: '>=', value: 2 }], then: [{ if: [{ alive: { unit: 'gorm' } }], then: [{ say: 'deep' }], else: [{ say: 'other' }] }] }] },
  ]
  d = withMap(d, (m) => ({ ...m, events: [{ id: 'all', when: { battle: 'start' }, do: cmds }] }))
  void c
  return d
}

describe('events: list operations', () => {
  it('adds a battle-start event with a fresh name, renames, repeats, reorders and deletes', () => {
    let d = base()
    let m = addEvent(map(d), dctx(d))
    expect(m.events).toEqual([{ id: 'event_1', when: { battle: 'start' }, do: [] }])
    m = addEvent(m, { doc: d, map: m })
    expect(m.events!.map((e) => e.id)).toEqual(['event_1', 'event_2'])
    expect(newEventId(m)).toBe('event_3')
    m = moveEvent(m, 0, 1)
    expect(m.events!.map((e) => e.id)).toEqual(['event_2', 'event_1'])
    m = renameEvent(m, 'event_1', 'ambush')
    expect(m.events!.map((e) => e.id)).toEqual(['event_2', 'ambush'])
    m = setEventRepeat(m, 'ambush', true)
    expect(m.events![1]).toEqual({ id: 'ambush', when: { battle: 'start' }, do: [], repeat: true })
    m = setEventRepeat(m, 'ambush', false)
    expect(m.events![1]).not.toHaveProperty('repeat')
    m = deleteEvent(deleteEvent(m, 'ambush'), 'event_2')
    expect(m).not.toHaveProperty('events') // an emptied list leaves its key out
    d = withMap(d, () => m)
  })

  it('every trigger kind starts valid where it can and keeps its extras out of the file', () => {
    const d = base()
    const c = dctx(d)
    expect(newTrigger(c, 'turn')).toEqual({ turn: 1 })
    expect(newTrigger(c, 'everyTurn')).toEqual({ turn: 'every' })
    expect(newTrigger(c, 'reach')).toEqual({ reach: { x: 0, y: 0, w: 1, h: 1 } })
    expect(newTrigger(c, 'defeated')).toEqual({ defeated: { unit: 'gorm' } })
    expect(newTrigger(c, 'talk')).toEqual({ talk: 'gorm', to: 'lyra' })
    let m = addEvent(map(d), c)
    m = setTriggerKind(m, 'event_1', 'won', c)
    expect(m.events![0].when).toEqual({ battle: 'won' })
  })

  it('changing away from a talk event removes its "end the turn" commands, nested ones too', () => {
    const d = base()
    const c = dctx(d)
    let m = addEvent(map(d), c)
    m = setTriggerKind(m, 'event_1', 'talk', c)
    m = addCommand(m, 'event_1', [], { endTurn: true })
    m = addCommand(m, 'event_1', [], { if: [{ round: { op: '>=', value: 2 } }], then: [{ endTurn: true }, { say: 'x' }] })
    m = setTriggerKind(m, 'event_1', 'start', c)
    expect(m.events![0].do).toEqual([{ if: [{ round: { op: '>=', value: 2 } }], then: [{ say: 'x' }] }])
  })

  it('event conditions are written only when there are some', () => {
    const d = base()
    let m = addEvent(map(d), dctx(d))
    m = setEventConditions(m, 'event_1', [newCondition(dctx(d), 'switch')])
    expect(m.events![0].if).toEqual([{ switch: 'village_saved' }])
    m = setEventConditions(m, 'event_1', [])
    expect(m.events![0]).not.toHaveProperty('if')
    m = patchEvent(m, 'event_1', { repeat: true })
    expect(m.events![0].repeat).toBe(true)
  })
})

describe('commands: add / edit / reorder / delete, with nesting', () => {
  it('works at the top and three levels deep', () => {
    const d = base()
    let m = addEvent(map(d), dctx(d))
    m = addCommand(m, 'event_1', [], { say: 'a' })
    m = addCommand(m, 'event_1', [], { if: [{ round: { op: '>=', value: 2 } }], then: [] })
    const p1: ListPath = [[1, 'then']]
    m = addCommand(m, 'event_1', p1, { if: [{ round: { op: '>=', value: 3 } }], then: [] })
    const p2: ListPath = [[1, 'then'], [0, 'then']]
    m = addCommand(m, 'event_1', p2, { if: [{ round: { op: '>=', value: 4 } }], then: [] })
    const p3: ListPath = [...p2, [0, 'else']]
    m = addCommand(m, 'event_1', [...p2, [0, 'then']], { say: 'deep' })
    m = addCommand(m, 'event_1', p3, { say: 'else deep' })
    expect(depthOf(p3)).toBe(3)
    expect(getList(m.events![0].do, [...p2, [0, 'then']])).toEqual([{ say: 'deep' }])
    expect(getList(m.events![0].do, p3)).toEqual([{ say: 'else deep' }])
    const p3b: ListPath = [...p2, [1, 'else']] // the same else list once the level-3 "if" has moved to index 1
    // reorder and patch inside a nested list
    m = addCommand(m, 'event_1', p2, { say: 'second' })
    m = moveCommand(m, 'event_1', p2, 1, 0)
    expect(getList(m.events![0].do, p2)![0]).toEqual({ say: 'second' })
    m = patchCommand(m, 'event_1', p2, 0, { say: 'edited', speaker: 'gorm' })
    expect(getList(m.events![0].do, p2)![0]).toEqual({ say: 'edited', speaker: 'gorm' })
    m = patchCommand(m, 'event_1', p2, 0, { speaker: undefined })
    expect(getList(m.events![0].do, p2)![0]).toEqual({ say: 'edited' })
    m = replaceCommand(m, 'event_1', p2, 0, { say: 'swapped' })
    // emptying an "else" list removes the key, a "then" list stays
    m = removeCommand(m, 'event_1', p3b, 0)
    const inner = getList(m.events![0].do, p2)![1]
    expect(inner).not.toHaveProperty('else')
    m = removeCommand(m, 'event_1', [...p2, [1, 'then']], 0)
    expect(getList(m.events![0].do, p2)![1].then).toEqual([])
    // the list helpers
    expect(listMove([1, 2, 3], 0, 2)).toEqual([2, 3, 1])
    expect(listAdd([1, 3], 2, 1)).toEqual([1, 2, 3])
  })

  it('every command kind has a default that the editor can build on', () => {
    const d = addResource(base(), 'Wood', 'wood')
    const c = dctx(d)
    for (const k of ['say', 'addUnit', 'removeUnit', 'moveUnit', 'changeSide', 'giveItem', 'giveResource', 'setSwitch', 'setVariable', 'addVariable', 'objective', 'win', 'lose', 'if', 'endTurn', 'focus'] as const) {
      const cmd = newCommand(c, k)
      expect(Object.keys(cmd)).toContain(k === 'endTurn' ? 'endTurn' : k)
    }
    expect(newCommand(c, 'addUnit').addUnit).toMatchObject({ x: 0, y: 0, side: sideA(d) })
    expect(newCommand(c, 'if')).toEqual({ if: [{ round: { op: '>=', value: 2 } }], then: [] })
    expect(newRule(c, 'survive')).toEqual({ survive: 5 })
  })
})

describe('summaries', () => {
  it('describe a trigger and the first commands in plain words', () => {
    let d = rich()
    const c = dctx(d)
    expect(triggerText(c, { turn: 4, side: sideB(d) })).toBe(`Turn 4, ${map(d).sides![1].name}’s turn`)
    expect(triggerText(c, { turn: 'every' })).toMatch(/^Every turn, /)
    expect(triggerText(c, { reach: { x: 4, y: 1, w: 2, h: 1 }, by: { side: sideA(d) } })).toMatch(/^Any unit of side .* reaches the area 5, 2 \(2 × 1\)$/)
    expect(triggerText(c, { defeated: { unit: 'gorm' }, by: { type: d.units![0].id } })).toMatch(/^Gorm is defeated by any /)
    expect(triggerText(c, { talk: 'gorm', to: 'lyra' })).toMatch(/^Gorm talks to /)
    const e: StoryEvent = { id: 'ambush', when: { turn: 4, side: sideB(d) }, do: [{ say: 'Now! Strike from the forest!', speaker: 'gorm' }, { addUnit: { type: d.units![0].id, side: sideB(d), x: 9, y: 1 } }, { say: 'x' }] }
    expect(eventSummary(c, e)).toBe(`Turn 4, ${map(d).sides![1].name}’s turn → Gorm says “Now! Strike from the forest!”, adds ${/^[aeiou]/i.test(d.units![0].name) ? 'an' : 'a'} ${d.units![0].name} (${map(d).sides![1].name}) at 10, 2 and 1 more`)
    expect(eventSummary(c, { id: 'e', when: { battle: 'start' }, do: [] })).toBe('The battle starts → nothing yet')
    expect(filterText(c, {})).toBe('any unit')
    expect(filterText(c, 'gorm')).toBe('Gorm')
    expect(conditionText(c, { switch: 'village_saved', is: false })).toBe('the switch “Village saved” is off')
    expect(conditionText(c, { variable: 'gold_found', op: '>=', value: 3 })).toBe('the variable “Gold found” is at least 3')
    expect(ruleText(c, { survive: 1 })).toBe('Survive 1 round')
    // every command of the rich event reads as a sentence
    for (const cmd of map(d).events![0].do) expect(commandText(c, cmd).length).toBeGreaterThan(3)
    d = base()
  })
})

describe('unit story names', () => {
  it('suggests a name from the unit and offers only named units', () => {
    const d = base()
    const u = map(d).units![2]
    expect(suggestUnitId(d, map(d), { ...u, name: 'Captain Ann' })).toBe('captain_ann')
    expect(suggestUnitId(d, map(d), { ...u, name: 'Gorm' })).toBe('gorm_2')
    expect(namedUnits(d, map(d)).map((x) => x.id)).toEqual(['gorm', 'lyra'])
    expect(teamsOf(map(d)).length).toBe(map(d).sides!.length)
    expect(idProblem('has space', [])).not.toBeNull()
    expect(idProblem('gorm', ['gorm'])).not.toBeNull()
    expect(idProblem('ok-1_x', ['gorm'])).toBeNull()
  })

  it('renaming an id rewrites every reference in the events and objectives', () => {
    let d = rich()
    d = withMap(d, (m) => ({ ...m, objectives: { text: 'Kill Gorm', win: [{ defeated: { unit: 'gorm' } }, { reach: { x: 0, y: 0, w: 1, h: 1 }, by: { unit: 'gorm' } }], lose: [{ defeated: { unit: 'lyra' } }] },
      events: [...m.events!, { id: 'talky', when: { talk: 'gorm', to: 'lyra' }, if: [{ alive: { unit: 'gorm' } }], do: [{ say: 'hi', speaker: 'gorm' }, { endTurn: true }] },
        { id: 'def', when: { defeated: 'gorm' }, do: [] }] } as unknown as MapFile))
    const r = changeUnitStoryId(d, mapId(d), map(d).units![0].x, map(d).units![0].y, 'gorm_the_great')
    expect(r.error).toBeNull()
    const text = JSON.stringify(map(r.doc))
    expect(text).not.toContain('"gorm"')
    expect(text).toContain('"gorm_the_great"')
    // a plain string where a filter is expected stays a plain string
    expect(map(r.doc).events!.find((e) => e.id === 'def')!.when.defeated).toBe('gorm_the_great')
    // unchanged maps / events are the very same objects
    expect(renameUnitRefs(map(d), 'nobody', 'x')).toBe(map(d))
  })

  it('refuses a bad or taken name, and clearing a name that is used', () => {
    const d = rich()
    const u = map(d).units![0]
    expect(changeUnitStoryId(d, mapId(d), u.x, u.y, 'has space').error).toBeTruthy()
    expect(changeUnitStoryId(d, mapId(d), u.x, u.y, 'lyra').error).toBeTruthy()
    expect(changeUnitStoryId(d, mapId(d), u.x, u.y, '').error).toMatch(/Events or objectives/)
    const free = map(d).units![3]
    expect(changeUnitStoryId(d, mapId(d), free.x, free.y, 'scout').error).toBeTruthy() // an added unit already has it
    expect(changeUnitStoryId(d, mapId(d), free.x, free.y, 'newbie').doc.maps[mapId(d)]!.units![3].id).toBe('newbie')
    // a name nothing points at can be cleared again
    const named = changeUnitStoryId(d, mapId(d), free.x, free.y, 'newbie').doc
    expect(changeUnitStoryId(named, mapId(d), free.x, free.y, '').doc.maps[mapId(d)]!.units![3]).not.toHaveProperty('id')
  })

  it('deleting a unit that events point at lists what goes, and removes it in one edit', () => {
    let d = rich()
    d = withMap(d, (m) => ({ ...m, objectives: { win: [{ defeated: { unit: 'gorm' } }, { round: 4 }] } }))
    const u = map(d).units![0]
    const lines = unitRemovalUsages(d, mapId(d), u.x, u.y).map((l) => l.text).join('\n')
    expect(lines).toMatch(/Gorm says/)
    expect(lines).toMatch(/Gorm is defeated/)
    const next = removeUnitAndStory(d, mapId(d), u.x, u.y)
    expect(map(next).units!.some((x) => x.id === 'gorm')).toBe(false)
    expect(JSON.stringify(map(next))).not.toContain('"gorm"')
    expect(map(next).objectives).toEqual({ win: [{ round: 4 }] })
    // the clean-up is exactly the same doc minus those things, so the event itself survives
    expect(map(next).events![0].id).toBe('all')
    expect(storyChecks(next, mapId(next)).filter((c) => /not a named unit/.test(c.text))).toEqual([])
  })

  it('a unit nothing points at is removed without a list', () => {
    const d = base()
    const free = map(d).units![3]
    expect(unitRemovalUsages(d, mapId(d), free.x, free.y)).toEqual([])
    expect(map(removeUnitAndStory(d, mapId(d), free.x, free.y)).units!.length).toBe(map(d).units!.length - 1)
  })
})

describe('clean-up when other things are deleted', () => {
  it('a deleted event trigger / condition removes the event, a command removes just the command', () => {
    let d = base()
    d = withMap(d, (m) => ({
      ...m,
      events: [
        { id: 'a', when: { defeated: { unit: 'gorm' } }, do: [{ say: 'x' }] },
        { id: 'b', when: { battle: 'start' }, if: [{ switch: 'village_saved' }], do: [{ say: 'y' }] },
        { id: 'c', when: { battle: 'start' }, do: [{ say: 'keep' }, { setSwitch: 'village_saved', to: true }, { if: [{ switch: 'village_saved' }], then: [{ say: 'z' }] }, { say: 'keep too' }] },
      ],
    }))
    expect(storyItemUsages(d, 'switches', 'village_saved').length).toBe(3)
    const n = removeStoryItem(d, 'switches', 'village_saved')
    expect(n.game).not.toHaveProperty('switches')
    expect(map(n).events!.map((e) => e.id)).toEqual(['a', 'c'])
    expect(map(n).events![1].do).toEqual([{ say: 'keep' }, { say: 'keep too' }])
  })

  it('removing a variable, item or resource cleans only what names it, and leaves nothing dangling', () => {
    const d = rich()
    const res = d.game!.resources![0].id
    const noVar = removeStoryItem(d, 'variables', 'gold_found')
    expect(storyChecks(noVar, mapId(noVar)).filter((c) => /variable/.test(c.text))).toEqual([])
    expect(JSON.stringify(map(noVar))).not.toContain('gold_found')
    const noItem = deleteItem(d, 'potion')
    expect(JSON.stringify(map(noItem))).not.toContain('potion')
    // the added unit stays, only its item went
    const added = map(noItem).events![0].do.find((c) => c.addUnit)!.addUnit!
    expect(added.items).toEqual([])
    const noRes = removeResource(d, res)
    expect(map(noRes).events![0].do.some((c) => c.giveResource)).toBe(false)
    expect(findUsages(d, 'resource', res).some((u) => /Gives side/.test(u.text))).toBe(true)
  })

  it('deleting a unit type removes events about it and about its placed units, even through a chain of added units', () => {
    let d = rich()
    const type = d.units![0].id
    // gorm is of the first type? make sure by retyping the two named units
    d = withMap(d, (m) => ({ ...m, units: m.units!.map((u) => (u.id === 'gorm' ? { ...u, type } : u)) }))
    const usage = findUsages(d, 'unit', type).map((u) => u.text).join('\n')
    expect(usage).toMatch(/Adds a/)
    const next = cleanUnit(d, type)
    // the addUnit command (type gone) is removed; 'scout' (its id) was only referenced by the removeUnit after it: gone too
    const ids = JSON.stringify(map(next).events ?? [])
    expect(ids).not.toContain('scout')
    expect(ids).not.toContain('"gorm"')
    expect(storyChecks(next, mapId(next)).filter((c) => /not a named unit|not a unit type/.test(c.text))).toEqual([])
  })

  it('removing a side removes what names the side, its team and its units', () => {
    let d = rich()
    d = withMap(d, (m) => addSide(m))
    const b = map(d).sides![2].id
    d = withMap(d, (m) => ({
      ...m,
      units: m.units!.map((u) => (u.id === 'lyra' ? { ...u, side: b } : u)),
      events: [...m.events!, { id: 'third', when: { turn: 2, side: b }, do: [{ lose: b }, { addUnit: { type: d.units![0].id, side: b, x: 0, y: 0 } }, { say: 'stay' }] }],
    }))
    const lines = sideRemovalUsages(d, mapId(d), b).map((l) => l.text).join('\n')
    expect(lines).toMatch(/event “third” is removed/)
    const next = removeSideAndStory(d, mapId(d), b)
    expect(map(next).sides!.some((s) => s.id === b)).toBe(false)
    expect(storyChecks(next, mapId(next)).filter((c) => /not a (named unit|side|team)/.test(c.text))).toEqual([])
    expect(JSON.stringify(map(next))).not.toContain('lyra')
    expect(map(next).events!.some((e) => e.id === 'third')).toBe(false)
  })

  it('scrubMap returns the very same map when nothing is affected', () => {
    const d = rich()
    expect(scrubMap(d, map(d), [{ kind: 'item', id: 'nothing-here' }])).toBe(map(d))
    expect(storyUsages(d, [{ kind: 'item', id: 'nothing-here' }])).toEqual([])
  })
})

describe('objectives', () => {
  it('are written only when they say something', () => {
    const d = base()
    let m = setObjectives(map(d), { text: '  ' })
    expect(m).not.toHaveProperty('objectives')
    m = patchObjectives(map(d), { text: 'Defeat Gorm', win: [{ defeated: { unit: 'gorm' } }] })
    expect(m.objectives).toEqual({ text: 'Defeat Gorm', win: [{ defeated: { unit: 'gorm' } }] })
    m = patchObjectives(m, { win: [] })
    expect(m.objectives).toEqual({ text: 'Defeat Gorm' })
    m = patchObjectives(m, { text: '' })
    expect(m).not.toHaveProperty('objectives')
    expect(objectiveOverlays({ win: [{ reach: { x: 1, y: 1, w: 2, h: 2 } }], lose: [{ reach: { x: 0, y: 0, w: 1, h: 1 } }] }).map((o) => o.tone)).toEqual(['win', 'lose'])
  })
})

describe('switches and variables', () => {
  it('get an id from the name, rename without changing it, reorder, and leave the key out when empty', () => {
    let d = loadDemo('demo-economy')
    expect(d.game!.switches).toBeUndefined()
    expect(newStoryId(d, 'switches', 'Village saved!')).toBe('village_saved')
    d = addStoryItem(d, 'switches', 'Village saved')
    d = addStoryItem(d, 'switches', 'Village saved')
    expect(d.game!.switches).toEqual([{ id: 'village_saved', name: 'Village saved' }, { id: 'village_saved_2', name: 'Village saved' }])
    d = renameStoryItem(d, 'switches', 'village_saved', 'Rescued')
    d = moveStoryItem(d, 'switches', 0, 1)
    expect(d.game!.switches!.map((s) => s.id)).toEqual(['village_saved_2', 'village_saved'])
    expect(d.game!.switches![1].name).toBe('Rescued')
    d = removeStoryItem(removeStoryItem(d, 'switches', 'village_saved'), 'switches', 'village_saved_2')
    expect(d.game).not.toHaveProperty('switches')
  })
})

describe('checks', () => {
  it('a valid rich map has none', () => {
    const d = rich()
    // the portrait has no picture: the one hint the loader would not give
    expect(storyChecks(d, mapId(d)).map((c) => c.text)).toEqual([expect.stringContaining('portrait “villager” has no picture')])
  })

  it('name the mistakes the loader would refuse, with the event', () => {
    let d = base()
    d = withMap(d, (m) => ({
      ...m,
      units: m.units!.map((u, i) => (i === 2 ? { ...u, id: 'gorm' } : u)),
      events: [
        { id: 'one', when: { battle: 'start' }, do: [{ say: '  ' }, { endTurn: true }, { moveUnit: 'gorm', x: 99, y: 0 }, { setSwitch: 'gone', to: true }, { addUnit: { type: 'dragon', side: 'purple', x: 0, y: 99 } }] },
        { id: 'one', when: { reach: { x: 0, y: 0, w: 500, h: 1 } }, do: [] },
        { id: 'talk', when: { talk: 'gorm', to: 'gorm' }, do: [{ if: [], then: [] }] },
      ],
      objectives: { win: [{ reach: { x: 50, y: 0, w: 1, h: 1 } }], lose: [{ survive: 3 }] },
    }))
    const texts = storyChecks(d, mapId(d)).map((c) => c.text).join('\n')
    expect(texts).toMatch(/Event “one”, command 1: the text to say is empty/)
    expect(texts).toMatch(/command 2: “End the turn” only works/)
    expect(texts).toMatch(/command 3: the square is outside the map/)
    expect(texts).toMatch(/command 4: “gone” is not a switch/)
    expect(texts).toMatch(/command 5: “dragon” is not a unit type/)
    expect(texts).toMatch(/command 5: “purple” is not a side/)
    expect(texts).toMatch(/another event has the same name/)
    expect(texts).toMatch(/“When”: the area does not fit inside the map/)
    expect(texts).toMatch(/a unit cannot talk to itself/)
    expect(texts).toMatch(/This “If” has no conditions|this “If” has no conditions/i)
    expect(texts).toMatch(/the story name “gorm” is used by more than one unit/)
    expect(texts).toMatch(/The objectives, “Win” entry 1: an area does not fit/)
    expect(texts).toMatch(/“Survive” is a way to win/)
  })

  it('give each event its own list for the panel', () => {
    let d = base()
    d = withMap(d, (m) => ({ ...m, events: [{ id: 'x', when: { battle: 'start' }, do: [{ say: '' }] }, { id: 'y', when: { battle: 'end' }, do: [] }] }))
    const list = storyChecks(d, mapId(d))
    expect(list.every((c) => c.eventId === 'x')).toBe(true)
    expect(list.length).toBe(1)
  })
})

describe('overlays', () => {
  it('show the trigger area and the squares commands use', () => {
    const e: StoryEvent = { id: 'o', when: { reach: { x: 2, y: 1, w: 3, h: 2 } }, do: [{ addUnit: { type: 't', side: 's', x: 5, y: 5 } }, { moveUnit: 'a', x: 1, y: 1 }, { focus: { x: 0, y: 4 } }] }
    expect(eventOverlays(e).map((o) => `${o.tone}:${o.x},${o.y},${o.w},${o.h}`)).toEqual(['area:2,1,3,2', 'add:5,5,1,1', 'move:1,1,1,1', 'view:0,4,1,1'])
    expect(eventOverlays(undefined)).toEqual([])
    expect(rectBetween({ x: 4, y: 3 }, { x: 1, y: 5 })).toEqual({ x: 1, y: 3, w: 4, h: 3 })
  })
})

describe('the rest of the model still works with a map that has story data', () => {
  it('ctxOf and unit edits keep the story keys', () => {
    const d = rich()
    expect(ctxOf(d)).not.toBeNull()
    const m = setUnitStoryId(map(d), map(d).units![3].x, map(d).units![3].y, 'zed')
    expect(m.units![3].id).toBe('zed')
    expect(m.events).toBe(map(d).events)
  })
})
