import { describe, expect, it } from 'vitest'
import { getLook, setLook } from '../../src/model/graphicsLooks'
import { removeSheet } from '../../src/model/graphics'
import { addWalk, editWalk, frameCount, handWritten, hasOwnSides, lookOf, removeWalk, rowOfRun, runOfRow, setOwnSides, setWalkCount, setWalkMs, setWalkRow, setWalkSheet, shownDirs, walkProblems } from '../../src/model/walking'
import type { ProjectDoc, WalkDef } from '../../src/model/types'

const slot = { kind: 'units' as const, id: 'knight' }
const doc = (): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
  terrain: [], structures: null, weapons: null, items: null, research: null, units: [{ id: 'knight', name: 'Knight' }], maps: {}, scripts: {},
  graphics: {
    sheets: [{ id: 'men', file: 'art/men.png', cellWidth: 16, cellHeight: 16 }, { id: 'walk', file: 'art/walk.png', cellWidth: 32, cellHeight: 32 }],
    units: { knight: { sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', cell: [1, 0] } } } },
  },
  campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)

const walkOfDoc = (d: ProjectDoc) => getLook(d, slot)?.walk

describe('walking frames of a unit picture', () => {
  it('rows and runs', () => {
    expect(runOfRow(2, 3)).toEqual([[0, 2], [1, 2], [2, 2]])
    expect(runOfRow(0, 999)).toHaveLength(64)
    expect(rowOfRun([[0, 1], [1, 1]])).toBe(1)
    expect(rowOfRun([[1, 1], [2, 1]])).toBeUndefined()   // not from the left
    expect(rowOfRun(undefined)).toBeUndefined()
  })

  it('a first walk is four frames facing down, and the image, rows, count and time are edited as the form does', () => {
    const t = { slot }
    let d = addWalk(doc(), t)
    expect(walkOfDoc(d)).toEqual({ down: [[0, 0], [1, 0], [2, 0], [3, 0]] })
    expect(addWalk(d, t)).toBe(d)   // already there
    d = setWalkRow(d, t, 'up', 1)
    d = setWalkRow(d, t, 'side', 2)
    d = setWalkCount(d, t, 3)
    d = setWalkMs(d, t, 90)
    d = setWalkSheet(d, t, 'walk')
    expect(walkOfDoc(d)).toEqual({ sheet: 'walk', down: [[0, 0], [1, 0], [2, 0]], up: [[0, 1], [1, 1], [2, 1]], side: [[0, 2], [1, 2], [2, 2]], frameMs: 90 })
    expect(frameCount(walkOfDoc(d)!)).toBe(3)
    d = setWalkMs(d, t, 150)   // the default is written as no key
    expect(walkOfDoc(d)!.frameMs).toBeUndefined()
    d = setWalkSheet(d, t, 'men')   // the picture's own image is written as no key
    expect(walkOfDoc(d)!.sheet).toBeUndefined()
    d = setWalkRow(d, t, 'up', undefined)
    expect(walkOfDoc(d)!.up).toBeUndefined()
    expect(getLook(d, slot)!.cell).toEqual([0, 0])   // the standing picture is untouched
    d = removeWalk(d, t)
    expect(walkOfDoc(d)).toBeUndefined()
    expect('walk' in getLook(d, slot)!).toBe(false)
  })

  it('left and right of their own, and back to one side row', () => {
    const t = { slot }
    let d = setWalkRow(addWalk(doc(), t), t, 'side', 2)
    d = setOwnSides(d, t, true)
    expect(walkOfDoc(d)).toMatchObject({ right: [[0, 2], [1, 2], [2, 2], [3, 2]], left: [[0, 2], [1, 2], [2, 2], [3, 2]] })
    expect(walkOfDoc(d)!.side).toBeUndefined()
    expect(hasOwnSides(walkOfDoc(d)!)).toBe(true)
    d = setWalkRow(d, t, 'left', 3)
    expect(walkOfDoc(d)!.left![0]).toEqual([0, 3])
    d = setOwnSides(d, t, false)
    expect(walkOfDoc(d)).toMatchObject({ side: [[0, 2], [1, 2], [2, 2], [3, 2]] })
    expect(walkOfDoc(d)!.left).toBeUndefined()
  })

  it('a side\'s own picture has its own walk, and the base walk is not touched', () => {
    const t = { slot, side: 'red' }
    let d = addWalk(doc(), t)
    expect(lookOf(d, t)!.walk).toEqual({ down: [[0, 0], [1, 0], [2, 0], [3, 0]] })
    expect(walkOfDoc(d)).toBeUndefined()
    d = setWalkCount(d, t, 2)
    expect(lookOf(d, t)!.walk!.down).toHaveLength(2)
    d = removeWalk(d, t)
    expect(lookOf(d, t)!.walk).toBeUndefined()
    expect(getLook(d, slot)!.sides!.red).toEqual({ sheet: 'men', cell: [1, 0] })
  })

  it('hand-written frames are noticed and a change replaces them; the rest of the walk is kept', () => {
    const t = { slot }
    let d = editWalk(doc(), t, () => ({ down: [[1, 0], [2, 0]], up: [[0, 1], [1, 1]], frameMs: 80 }))
    expect(handWritten(walkOfDoc(d)!)).toEqual(['down'])
    d = setWalkCount(d, t, 3)
    expect(walkOfDoc(d)).toEqual({ down: [[0, 0], [1, 0], [2, 0]], up: [[0, 1], [1, 1], [2, 1]], frameMs: 80 })
  })

  it('the preview directions: side turned round, own left and right, left alone', () => {
    const cells = (r: number) => runOfRow(r, 2)
    expect(shownDirs({ down: cells(0), side: cells(2) }).map((s) => [s.dir, s.mirror])).toEqual([['down', false], ['right', false], ['left', true]])
    expect(shownDirs({ left: cells(1), right: cells(2) }).map((s) => [s.dir, s.mirror])).toEqual([['right', false], ['left', false]])
    expect(shownDirs({ left: cells(1) }).map((s) => [s.dir, s.mirror])).toEqual([['right', true], ['left', false]])
    expect(shownDirs({ up: cells(1) }).map((s) => s.dir)).toEqual(['up'])
    expect(shownDirs({})).toEqual([])
  })

  it('checks mirror the game: no direction, a bad time, frames outside the image, a missing image', () => {
    const d = doc()
    const look = (walk: WalkDef) => ({ ...getLook(d, slot)!, walk })
    expect(walkProblems(d, look({}))).toEqual(['Give at least one direction a row of frames.'])
    expect(walkProblems(d, look({ down: runOfRow(0, 2), frameMs: 5 }))[0]).toMatch(/between 20 and 60000/)
    const dims = () => ({ width: 64, height: 64 })   // the walk image: 2 x 2 pieces of 32
    expect(walkProblems(d, look({ sheet: 'walk', down: runOfRow(0, 3) }), dims)[0]).toMatch(/outside the image \(it has 2 × 2 pieces\)/)
    expect(walkProblems(d, look({ sheet: 'walk', down: runOfRow(1, 2) }), dims)).toEqual([])
    expect(walkProblems(d, look({ sheet: 'gone', down: runOfRow(0, 2) }))[0]).toMatch(/no longer in the game/)
  })

  it('removing the walking image takes only the walk away, also from a side\'s picture', () => {
    let d = doc()
    d = editWalk(d, { slot }, () => ({ sheet: 'walk', down: runOfRow(0, 2) }))
    d = editWalk(d, { slot, side: 'red' }, () => ({ sheet: 'walk', down: runOfRow(0, 2) }))
    const gone = removeSheet(d, 'walk')
    expect(getLook(gone, slot)).toEqual({ sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', cell: [1, 0] } } })
    expect(setLook(gone, slot, null)).toBeDefined()
  })
})
