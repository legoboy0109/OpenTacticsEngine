// Units for different difficulties that share a square and a story name (docs/DIFFICULTY.md "One unit, different numbers per
// difficulty"): who conflicts, the story-name rule, the stack helpers and the clean-up when one variant is renamed or removed.
import { describe, expect, it } from 'vitest'
import { startDifficulties } from '../../src/model/difficulty'
import { mapDifficultyChecks } from '../../src/model/difficultyChecks'
import { ctxOf } from '../../src/model/mapCtx'
import { addUnitVariant, moveMapUnit, removeMapUnit, unitAt, unitIndexesAt, updateMapUnit } from '../../src/model/mapUnits'
import { takenUnitIds } from '../../src/model/storyIds'
import { changeUnitStoryId, removeUnitAndStory, unitRemovalUsages } from '../../src/model/storyUnits'
import { coexistOn } from '../../src/model/unitVariants'
import type { MapFile, MapUnit, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const ALL = ['easy', 'normal', 'hard']

/** The srpg demo with Easy / Normal / Hard and an enemy "orgrim" on (2, 2), one placement per list given. */
function setup(diffs: Array<string[] | undefined> = [['easy'], ['normal'], ['hard']]): { doc: ProjectDoc; id: string; map: MapFile } {
  const d0 = startDifficulties(loadDemo('demo-srpg'), true)
  const id = d0.game!.maps[0].id
  const m0 = d0.maps[id]!
  const side = (m0.sides ?? []).find((s) => s.controller === 'ai')!.id
  const type = d0.units![0].id
  const units: MapUnit[] = [...(m0.units ?? []).filter((u) => !(u.x === 2 && u.y === 2)),
    ...diffs.map((difficulty, i): MapUnit => ({ type, side, x: 2, y: 2, id: 'orgrim', stats: { hp: 10 + i }, ...(difficulty ? { difficulty } : {}) }))]
  const map = { ...m0, units, events: [{ id: 'e', when: { defeated: { unit: 'orgrim' } }, do: [{ say: 'He fell.' }] }] } as MapFile
  return { doc: { ...d0, maps: { ...d0.maps, [id]: map } }, id, map }
}

describe('who can share', () => {
  it('lists that never meet may share; unrestricted units meet everything', () => {
    expect(coexistOn(['easy'], ['hard'])).toBeNull()
    expect(coexistOn(['easy', 'normal'], ['normal', 'hard'])).toEqual(['normal'])
    expect(coexistOn(undefined, ['hard'])).toEqual(['hard'])
    expect(coexistOn(undefined, undefined)).toEqual([])
  })
})

describe('the checks', () => {
  it('say nothing about disjoint variants on one square with one story name', () => {
    const { doc, id, map } = setup()
    expect(mapDifficultyChecks(doc, id, map)).toEqual([])
  })
  it('refuse overlapping lists, naming the difficulty, for the square and for the story name', () => {
    const { doc, id, map } = setup([['easy', 'normal'], ['normal', 'hard']])
    const texts = mapDifficultyChecks(doc, id, map).map((c) => c.text)
    expect(texts.some((t) => t.includes('shares its square') && t.includes('on the difficulty “Normal”'))).toBe(true)
    expect(texts.some((t) => t.includes('same story name “orgrim”') && t.includes('“Normal”'))).toBe(true)
  })
  it('refuse a variant and an unrestricted unit on one square', () => {
    const { doc, id, map } = setup([['easy'], undefined])
    expect(mapDifficultyChecks(doc, id, map).some((c) => c.text.includes('shares its square') && c.text.includes('“Easy”'))).toBe(true)
  })
})

describe('story names', () => {
  it('a variant does not take the names of units for other difficulties, but takes those it meets', () => {
    const { map } = setup()
    const hard = map.units!.find((u) => u.difficulty?.[0] === 'hard')!
    expect(takenUnitIds(map, hard)).not.toContain('orgrim')
    expect(takenUnitIds(map, { ...hard, difficulty: ['normal', 'hard'] })).toContain('orgrim')
    expect(takenUnitIds(map)).toContain('orgrim')
  })
  it('renaming or removing one variant leaves the name, and what points at it, with the others', () => {
    const { doc, id } = setup()
    expect(unitRemovalUsages(doc, id, 2, 2, 1)).toEqual([])  // the event still finds orgrim on the other difficulties
    const gone = removeUnitAndStory(doc, id, 2, 2, 1)
    expect(unitIndexesAt(gone.maps[id]!, 2, 2)).toHaveLength(2)
    expect(gone.maps[id]!.events).toHaveLength(1)
    const renamed = changeUnitStoryId(doc, id, 2, 2, 'org', 0)
    expect(renamed.error).toBeNull()
    expect(renamed.doc.maps[id]!.units!.filter((u) => u.id === 'orgrim')).toHaveLength(2)
    expect(JSON.stringify(renamed.doc.maps[id]!.events)).toContain('orgrim')
    // the last holder of the name takes the events with it, as before
    let one = doc
    for (const v of [0, 0]) one = removeUnitAndStory(one, id, 2, 2, v)
    expect(unitRemovalUsages(one, id, 2, 2, 0).length).toBeGreaterThan(0)
    expect(removeUnitAndStory(one, id, 2, 2, 0).maps[id]!.events ?? []).toHaveLength(0)
  })
})

describe('the stack', () => {
  it('edits go to the chosen variant', () => {
    const { map } = setup()
    expect(unitIndexesAt(map, 2, 2)).toHaveLength(3)
    expect(unitAt(map, 2, 2, 2)!.difficulty).toEqual(['hard'])
    const next = updateMapUnit(map, 2, 2, { name: 'Ogre' }, 1)
    expect(unitAt(next, 2, 2, 1)!.name).toBe('Ogre')
    expect(unitAt(next, 2, 2, 0)!.name).toBeUndefined()
    expect(unitIndexesAt(removeMapUnit(map, 2, 2, 0), 2, 2)).toHaveLength(2)
  })
  it('moving a variant onto a square that holds a unit it would meet is refused', () => {
    const { doc, map } = setup()
    const ctx = ctxOf(doc)!
    const other = map.units!.find((u) => !(u.x === 2 && u.y === 2))!
    expect(moveMapUnit(map, ctx, { x: 2, y: 2 }, { x: other.x, y: other.y }, 0).error).toMatch(/already a unit/)
  })
  it('"add a variant" copies the unit for the difficulties nothing on the square covers', () => {
    const one = setup([['hard']])
    const r = addUnitVariant(one.map, 2, 2, 0, ALL)
    expect(r.error).toBeNull()
    const stack = unitIndexesAt(r.map, 2, 2).map((i) => r.map.units![i])
    expect(stack.map((u) => u.difficulty)).toEqual([['hard'], ['easy', 'normal']])
    expect(stack[1].id).toBe('orgrim')
    expect(stack[1].stats).toEqual(stack[0].stats)
    expect(stack[1].stats).not.toBe(stack[0].stats)
    expect(addUnitVariant(r.map, 2, 2, 0, ALL).error).toMatch(/already cover every difficulty/)
    expect(addUnitVariant(setup([undefined]).map, 2, 2, 0, ALL).error).toMatch(/every difficulty/)
  })
})
