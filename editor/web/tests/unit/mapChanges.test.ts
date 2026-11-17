// Map changes and the fog options in the editor's model (docs/EVENTS.md "Changing the map"): creating, painting into, renaming and deleting a
// change with its events, the structure swaps, the "after" view, the story commands (defaults, sentences, checks) and the checks that mirror
// the loader.
import { describe, expect, it } from 'vitest'
import { setAiSeesAll, setFog, setFogVision } from '../../src/model/fog'
import { fogChecks } from '../../src/model/fogChecks'
import { setDoorWord, setItemLight, setItemLightOn, setStructureLight, setTerrainBlocksSight } from '../../src/model/lightParts'
import { addChange, applyCount, applyToMap, baseTerrainAt, cellsOf, changeIdProblem, changeIds, changesOf, changeUsages, deleteChangeAndStory, nextChangeId, paintChange, removeSwap, renameChange, setSwap, swapKinds, swapsOf } from '../../src/model/mapChanges'
import { changeChecks } from '../../src/model/mapChangeChecks'
import { dropTerrainFromChanges } from '../../src/model/mapChangeRefs'
import { mapWarnings } from '../../src/model/mapChecks'
import { editMap } from '../../src/model/mapCtx'
import { newCommand } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { commandKind, type Command } from '../../src/model/storyTypes'
import { updateSide } from '../../src/model/mapSides'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => loadDemo('demo-srpg')
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const edit = (d: ProjectDoc, fn: (m: MapFile) => MapFile) => editMap(d, mapId(d), (m) => fn(m))
const second = (d: ProjectDoc) => d.terrain!.find((t) => t.id !== baseTerrainAt(d.maps[mapId(d)]!, d.terrain, 0, 0))!.id
const withEvents = (d: ProjectDoc, events: unknown[]) => edit(d, (m) => ({ ...m, events: events as MapFile['events'] }))
const here = (d: ProjectDoc) => d.maps[mapId(d)]!

describe('creating and naming changes', () => {
  it('adds an empty change and refuses bad or repeated names', () => {
    let d = base()
    expect(changeIds(here(d))).toEqual([])
    d = edit(d, (m) => addChange(m, 'bridge'))
    expect(changesOf(here(d)).bridge).toEqual({ cells: [] })
    expect(edit(d, (m) => addChange(m, 'bridge'))).toBe(d)
    expect(changeIdProblem(here(d), 'bridge')).toMatch(/already has/)
    expect(changeIdProblem(here(d), 'a b')).toMatch(/letters, digits/)
    expect(changeIdProblem(here(d), '')).toMatch(/Give the change a name/)
    expect(nextChangeId(here(d))).toBe('change1')
  })
})

describe('painting into a change', () => {
  it('records squares that differ from the map, replaces and removes them, and ignores squares outside', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    const t = second(d)
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 0, y: 0 }, { x: 1, y: 0 }, { x: 999, y: 0 }], t))
    expect(cellsOf(changesOf(here(d)).bridge)).toEqual([{ x: 0, y: 0, terrain: t }, { x: 1, y: 0, terrain: t }])
    // The base map is untouched.
    expect(baseTerrainAt(here(d), d.terrain, 0, 0)).not.toBe(t)
    // Painting the terrain the base already has takes the square out again.
    const baseId = baseTerrainAt(here(d), d.terrain, 0, 0)!
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 0, y: 0 }], baseId))
    expect(cellsOf(changesOf(here(d)).bridge).map((c) => c.x)).toEqual([1])
    // The same stroke again changes nothing (no extra undo step).
    const again = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 1, y: 0 }], t))
    expect(again).toBe(d)
  })

  it('the "after" view shows the change on the map and leaves the stored map alone', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    const t = second(d)
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 2, y: 1 }], t))
    const after = applyToMap(here(d), d.terrain, 'bridge')
    expect(baseTerrainAt(after, d.terrain, 2, 1)).toBe(t)
    expect(baseTerrainAt(here(d), d.terrain, 2, 1)).not.toBe(t)
    expect(applyToMap(here(d), d.terrain, 'nope')).toBe(here(d))
  })
})

describe('structure swaps', () => {
  it('offers kinds of the same size and keeps one swap per structure', () => {
    const d0 = base()
    const d = { ...d0, structures: [...(d0.structures ?? []), { ...d0.structures![0], id: 'twin', name: 'Twin' }, { ...d0.structures![0], id: 'wide', name: 'Wide', width: 3 }] }
    const kinds = d.structures
    const first = kinds[0]
    const offered = swapKinds(kinds, first.id)
    expect(offered.every((k) => k.id !== first.id)).toBe(true)
    expect(offered.map((k) => k.id)).toContain('twin')
    expect(offered.map((k) => k.id)).not.toContain('wide')  // another size
    let e = edit(d, (m) => addChange(m, 'gate'))
    e = edit(e, (m) => setSwap(m, 'gate', 'g', 'x'))
    e = edit(e, (m) => setSwap(m, 'gate', 'g', 'y'))
    expect(swapsOf(changesOf(here(e)).gate)).toEqual([{ id: 'g', kind: 'y' }])
    e = edit(e, (m) => removeSwap(m, 'gate', 'g'))
    expect('structures' in changesOf(here(e)).gate).toBe(false)
  })
})

describe('events follow a change', () => {
  const events = [{ id: 'e1', when: { battle: 'start' }, do: [{ applyChange: 'bridge' }, { revertChange: 'bridge' }, { setSwitch: 'x', to: true }] }]

  it('renaming rewrites the commands that name it', () => {
    let d = withEvents(edit(base(), (m) => addChange(m, 'bridge')), events)
    d = renameChange(d, mapId(d), 'bridge', 'ford')
    expect(changeIds(here(d))).toEqual(['ford'])
    expect(here(d).events![0].do[0]).toEqual({ applyChange: 'ford' })
    expect(here(d).events![0].do[1]).toEqual({ revertChange: 'ford' })
    // A taken name is refused.
    expect(renameChange(d, mapId(d), 'ford', 'ford')).toBe(d)
  })

  it('deleting removes the commands with it and lists them first', () => {
    const d = withEvents(edit(base(), (m) => addChange(m, 'bridge')), events)
    expect(applyCount(here(d), 'bridge')).toBe(1)
    expect(changeUsages(d, mapId(d), 'bridge').length).toBe(2)
    const gone = deleteChangeAndStory(d, mapId(d), 'bridge')
    expect(changeIds(here(gone))).toEqual([])
    expect(here(gone).events![0].do).toEqual([{ setSwitch: 'x', to: true }])
  })
})

describe('story commands', () => {
  const ctxOf = (d: ProjectDoc) => ({ doc: d, map: here(d) })

  it('new commands start with sensible values and say what they do', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    const c = ctxOf(d)
    const kinds = ['setTerrain', 'applyChange', 'revertChange', 'setFog', 'setVision'] as const
    for (const k of kinds) expect(commandKind(newCommand(c, k))).toBe(k)
    expect(newCommand(c, 'applyChange')).toEqual({ applyChange: 'bridge' })
    expect(commandText(c, { applyChange: 'bridge' })).toMatch(/Applies the map change “bridge”/)
    expect(commandText(c, { revertChange: 'bridge' })).toMatch(/Takes back/)
    expect(commandText(c, { setFog: false })).toBe('The fog of war lifts')
    expect(commandText(c, { setFog: true })).toBe('Fog of war comes down')
    expect(commandText(c, { setVision: -1 })).toMatch(/-1/)
    expect(commandText(c, { setTerrain: d.terrain![0].id, area: { x: 1, y: 1, w: 2, h: 2 } })).toMatch(/Turns the area/)
    expect(commandText(c, { setTerrain: d.terrain![0].id, cells: [[1, 1], [2, 2]] })).toMatch(/Turns 2 squares/)
    d = d
  })

  const check = (d: ProjectDoc, cmd: Command) => storyChecks(withEvents(d, [{ id: 'e', when: { battle: 'start' }, do: [cmd] }]), mapId(d)).map((x) => x.text)
  it('mirrors what the loader refuses', () => {
    const d = edit(base(), (m) => addChange(m, 'bridge'))
    const t = d.terrain![0].id
    expect(check(d, { applyChange: 'bridge' })).toEqual([])
    expect(check(d, { applyChange: 'nope' }).join()).toMatch(/not one of this map’s changes/)
    expect(check(d, { revertChange: '' }).join()).toMatch(/pick a change/)
    expect(check(d, { setTerrain: 'lava', area: { x: 0, y: 0, w: 1, h: 1 } }).join()).toMatch(/not a terrain/)
    expect(check(d, { setTerrain: t })).toEqual(expect.arrayContaining([expect.stringMatching(/area or as a list/)]))
    expect(check(d, { setTerrain: t, area: { x: 0, y: 0, w: 1, h: 1 }, cells: [[0, 0]] }).join()).toMatch(/exactly one/)
    expect(check(d, { setTerrain: t, area: { x: 9999, y: 0, w: 1, h: 1 } }).join()).toMatch(/does not fit inside the map/)
    expect(check(d, { setTerrain: t, cells: [[9999, 0]] }).join()).toMatch(/outside the map/)
    expect(check(d, { setVision: 11 }).join()).toMatch(/-10 to 10/)
    expect(check(d, { setVision: -10 })).toEqual([])
    expect(check(base(), { applyChange: 'x' }).join()).toMatch(/no changes/)
  })
})

describe('checks on a map', () => {
  const t = (d: ProjectDoc) => second(d)
  it('an empty change, a change nobody applies and squares outside the map', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    expect(changeChecks(d, mapId(d)).map((x) => x.text).join()).toMatch(/changes nothing yet/)
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 1, y: 1 }], t(d)))
    const notes = changeChecks(d, mapId(d)).filter((x) => x.note)
    expect(notes.map((x) => x.text).join()).toMatch(/never applied/)
    d = edit(d, (m) => ({ ...m, changes: { bridge: { cells: [{ x: 999, y: 0, terrain: t(d) }, { x: 1, y: 1, terrain: t(d) }, { x: 1, y: 1, terrain: t(d) }, { x: 0, y: 0, terrain: 'lava' }] } } }))
    const text = changeChecks(d, mapId(d)).map((x) => x.text).join('\n')
    expect(text).toMatch(/outside the map/)
    expect(text).toMatch(/is in it twice/)
    expect(text).toMatch(/does not have/)
  })

  it('applying makes it not a note, and the warnings list the checks', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 1, y: 1 }], t(d)))
    d = withEvents(d, [{ id: 'e', when: { battle: 'start' }, do: [{ applyChange: 'bridge' }] }])
    expect(changeChecks(d, mapId(d)).some((x) => /never applied/.test(x.text))).toBe(false)
    expect(mapWarnings(d, here(d)).some((w) => w.key.startsWith('changes:'))).toBe(false)
  })

  it('a unit that would be left on ground it cannot enter is a note, not an error', () => {
    const d0 = base()
    const moves = Object.fromEntries((d0.game!.moveTypes ?? []).map((m) => [m.id, 0]))
    const blocked = { id: 'void', name: 'Void', moveCost: moves }
    let d: ProjectDoc = { ...d0, terrain: [...d0.terrain!, blocked] }
    const u = here(d).units![0]
    d = edit(d, (m) => addChange(m, 'cave'))
    d = edit(d, (m) => paintChange(m, d.terrain, 'cave', [{ x: u.x, y: u.y }], blocked.id))
    const mine = changeChecks(d, mapId(d)).filter((x) => /would be left on/.test(x.text))
    expect(mine.length).toBeGreaterThan(0)
    expect(mine.every((x) => x.note)).toBe(true)
  })

  it('a structure swap that is the wrong size or names nothing is refused', () => {
    let d = edit(base(), (m) => ({ ...m, structures: [{ type: d0(m), x: 0, y: 0, id: 'g' }] }))
    function d0(_m: MapFile) { return base().structures![0].id }
    d = edit(d, (m) => ({ ...m, changes: { c: { structures: [{ id: 'nobody', kind: base().structures![0].id }] } } }))
    expect(changeChecks(d, mapId(d)).map((x) => x.text).join()).toMatch(/no structure of this map has the story name/)
  })
})

describe('fog options of the map', () => {
  it('extra vision and the computer seeing everything need fog; switching fog off clears them', () => {
    let d = base()
    d = edit(d, (m) => ({ ...m, fogVision: 2, aiSeesAll: true }))
    expect(changeChecks(d, mapId(d)).map((x) => x.text).join()).toMatch(/only work on a map with fog/)
    d = setFog(d, mapId(d), true)
    expect(changeChecks(d, mapId(d)).filter((x) => !x.note)).toEqual([])
    d = setFog(d, mapId(d), false)
    expect('fogVision' in here(d)).toBe(false)
    expect('aiSeesAll' in here(d)).toBe(false)
  })

  it('setters write only what differs', () => {
    let d = setFog(base(), mapId(base()), true)
    expect(setFogVision(d, mapId(d), 0)).toBe(d)
    d = setFogVision(d, mapId(d), 3)
    expect(here(d).fogVision).toBe(3)
    expect(setFogVision(d, mapId(d), 99).maps[mapId(d)]!.fogVision).toBe(10)
    d = setFogVision(d, mapId(d), 0)
    expect('fogVision' in here(d)).toBe(false)
    d = setAiSeesAll(d, mapId(d), true)
    expect(here(d).aiSeesAll).toBe(true)
    d = setAiSeesAll(d, mapId(d), false)
    expect('aiSeesAll' in here(d)).toBe(false)
  })

  it('a side that is not the computer cannot see everything; making it human drops the key', () => {
    let d = setFog(base(), mapId(base()), true)
    const ai = here(d).sides!.find((s) => s.controller === 'ai')!
    d = edit(d, (m) => updateSide(m, ai.id, { aiSeesAll: true }))
    expect(changeChecks(d, mapId(d)).filter((x) => !x.note)).toEqual([])
    d = edit(d, (m) => updateSide(m, ai.id, { controller: 'human' }))
    expect(here(d).sides!.find((s) => s.id === ai.id)!.aiSeesAll).toBeUndefined()
    d = edit(d, (m) => ({ ...m, sides: m.sides!.map((s) => (s.id === ai.id ? { ...s, aiSeesAll: true } : s)) }))
    expect(changeChecks(d, mapId(d)).map((x) => x.text).join()).toMatch(/for sides the computer plays/)
  })
})

describe('light, blocks sight, torch and door word', () => {
  it('a structure light is 1 to 10 and 0 removes it', () => {
    const d = base()
    const s = d.structures![0].id
    const on = setStructureLight(d, s, 4)
    expect(on.structures!.find((x) => x.id === s)!.light).toBe(4)
    expect(setStructureLight(on, s, 99).structures!.find((x) => x.id === s)!.light).toBe(10)
    expect('light' in setStructureLight(on, s, 0).structures!.find((x) => x.id === s)!).toBe(false)
    expect(fogChecks(setStructureLight(d, s, 2)).filter((x) => x.section === 'structures')).toEqual([])
    const bad = { ...d, structures: d.structures!.map((x) => (x.id === s ? { ...x, light: 11 } : x)) }
    expect(fogChecks(bad).map((x) => x.text).join()).toMatch(/light must be a whole number from 1 to 10/)
  })

  it('terrain blocks sight on or off', () => {
    const d = base()
    const id = d.terrain![0].id
    expect(setTerrainBlocksSight(d, id, true).terrain![0].blocksSight).toBe(true)
    expect('blocksSight' in setTerrainBlocksSight(setTerrainBlocksSight(d, id, true), id, false).terrain![0]).toBe(false)
  })

  it('a torch gets a light block with defaults; shrink stays under the radius; checks mirror the loader', () => {
    let d = base()
    const item = d.items!.find((i) => i.kind === 'usable')!.id
    d = setItemLightOn(d, item, true)
    expect(d.items!.find((i) => i.id === item)!.light).toEqual({ radius: 3, turns: 3 })
    d = setItemLight(d, item, 'shrink', 5)
    expect(d.items!.find((i) => i.id === item)!.light!.shrink).toBe(2)  // pulled under the radius
    d = setItemLight(d, item, 'radius', 1)
    expect(d.items!.find((i) => i.id === item)!.light!.shrink).toBeUndefined()
    d = setItemLight(d, item, 'radius', 4)
    expect(fogChecks(d).filter((x) => x.section === 'items' && !/no map has fog/.test(x.text))).toEqual([])
    const ally = { ...d, items: d.items!.map((i) => (i.id === item ? { ...i, target: 'ally' as const } : i)) }
    expect(fogChecks(ally).map((x) => x.text).join()).toMatch(/target must be/)
    d = setItemLightOn(d, item, false)
    expect('light' in d.items!.find((i) => i.id === item)!).toBe(false)
  })

  it('a door word is kept short and empty removes it', () => {
    const d = base()
    const door = d.structures!.find((s) => s.door)
    if (!door) return
    const on = setDoorWord(d, door.id, 'wall')
    expect(on.structures!.find((s) => s.id === door.id)!.door!.word).toBe('wall')
    expect('word' in on.structures!.find((s) => s.id === door.id)!.door!).toBe(true)
    const off = setDoorWord(on, door.id, '  ')
    expect('word' in off.structures!.find((s) => s.id === door.id)!.door!).toBe(false)
  })
})

describe('deleting a terrain follows into the changes', () => {
  it('drops the squares and the setTerrain commands that name it', () => {
    let d = edit(base(), (m) => addChange(m, 'bridge'))
    const t = second(d)
    d = edit(d, (m) => paintChange(m, d.terrain, 'bridge', [{ x: 1, y: 1 }], t))
    d = withEvents(d, [{ id: 'e', when: { battle: 'start' }, do: [{ setTerrain: t, area: { x: 0, y: 0, w: 1, h: 1 } }, { setSwitch: 'x', to: true }] }])
    const out = dropTerrainFromChanges(d, t)
    expect(cellsOf(changesOf(here(out)).bridge)).toEqual([])
    expect(here(out).events![0].do).toEqual([{ setSwitch: 'x', to: true }])
  })
})
