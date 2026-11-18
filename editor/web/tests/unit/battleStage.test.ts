import { describe, expect, it } from 'vitest'
import { addBackground, setDefaultBackground } from '../../src/model/battleArt'
import { addPlatform, clearPlatformFar, platformList, setPlatformPicture, setPlatformRight } from '../../src/model/battleTables'
import {
  defaultBackdropSize, enableStage, removeStage, setStageAlign, setStageFit, setStageGroup, setStagePoint, setStageRangeLine, setStageSize,
  stageAlign, stageFit, stageOf, stageProblems, stageRangeLine,
} from '../../src/model/battleStage'
import type { ProjectDoc } from '../../src/model/types'

const doc = (): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
  terrain: [{ id: 'plain', name: 'Plain' }], structures: null, weapons: null, items: null, research: null, units: null, maps: {}, scripts: {},
  graphics: { sheets: [{ id: 'bg', file: 'art/bg.png', cellWidth: 240, cellHeight: 160 }, { id: 'plat', file: 'art/plat.png', cellWidth: 64, cellHeight: 32 }] },
  campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)

describe('the fixed stage', () => {
  it('starts from the default backdrop size (else 240 x 160) and the fighters in the lower middle', () => {
    expect(stageOf(doc())).toBeUndefined()
    let d = enableStage(doc())
    expect(stageOf(d)).toEqual({ width: 240, height: 160, feet: { left: [89, 104], right: [151, 104] } })
    expect(enableStage(d)).toBe(d)
    d = addBackground(doc(), 'arena')  // the first image, 240 x 160
    d = setDefaultBackground(d, 'arena')
    expect(defaultBackdropSize(d)).toEqual({ width: 240, height: 160 })
    d = removeStage(enableStage(d))
    expect(stageOf(d)).toBeUndefined()
    expect('stage' in (d.graphics!.battle ?? {})).toBe(false)
  })
  it('edits size, points, align, fit and the range line; defaults are written as no key', () => {
    let d = enableStage(doc())
    d = setStageSize(d, 'width', 320)
    d = setStagePoint(d, 'feet', 'right', 0, 200)
    expect(stageOf(d)).toMatchObject({ width: 320, feet: { left: [89, 104], right: [200, 104] } })
    d = setStageAlign(d, 'bottom')
    d = setStageFit(d, 'contain')
    d = setStageRangeLine(d, false)
    expect(stageOf(d)).toMatchObject({ align: 'bottom', fit: 'contain', rangeLine: false })
    expect([stageAlign(stageOf(d)!), stageFit(stageOf(d)!), stageRangeLine(stageOf(d)!)]).toEqual(['bottom', 'contain', false])
    d = setStageAlign(setStageFit(setStageRangeLine(d, true), 'cover'), 'center')
    const s = stageOf(d)!
    expect('align' in s || 'fit' in s || 'rangeLine' in s).toBe(false)
    expect(setStagePoint(d, 'feetFar', 'left', 0, 5)).toBe(d)  // no such group yet
  })
  it('optional groups start as copies; removing the platform removes its far twin', () => {
    let d = enableStage(doc())
    d = setStageGroup(d, 'feetFar', true)
    expect(stageOf(d)!.feetFar).toEqual(stageOf(d)!.feet)
    d = setStagePoint(d, 'feetFar', 'left', 0, 30)
    expect(stageOf(d)!.feetFar!.left).toEqual([30, 104])
    expect(stageOf(d)!.feet.left).toEqual([89, 104])  // a copy, not the same list
    expect(setStageGroup(d, 'platformFar', true)).toBe(d)  // needs the platform first
    d = setStageGroup(d, 'platform', true)
    expect(stageOf(d)!.platform).toEqual({ left: [57, 96], right: [119, 96] })
    d = setStageGroup(d, 'platformFar', true)
    expect(stageOf(d)!.platformFar).toEqual(stageOf(d)!.platform)
    d = setStageGroup(d, 'platform', false)
    expect(stageOf(d)!.platform).toBeUndefined()
    expect(stageOf(d)!.platformFar).toBeUndefined()
  })
  it('keeps unknown keys', () => {
    let d = enableStage(doc())
    d = { ...d, graphics: { ...d.graphics!, battle: { stage: { ...stageOf(d)!, mystery: 7 } } } } as ProjectDoc
    d = setStageAlign(d, 'top')
    expect(stageOf(d)).toMatchObject({ mystery: 7, align: 'top' })
  })
  it('checks like the loader: size 16..1024, whole numbers, points inside the stage, platformFar needs platform', () => {
    let d = enableStage(doc())
    expect(stageProblems(d)).toEqual([])
    expect(stageProblems(setStageSize(d, 'width', 8))).toEqual([expect.stringMatching(/between 16 and 1024/)])
    expect(stageProblems(setStageSize(d, 'height', 2000))).toEqual([expect.stringMatching(/between 16 and 1024/)])
    expect(stageProblems(setStageSize(d, 'width', 100))).toEqual([expect.stringMatching(/right one of the foot points \(151, 104\) is outside the stage \(100 × 160\)/)])
    expect(stageProblems(setStagePoint(d, 'feet', 'left', 1, 160))).toEqual([expect.stringMatching(/left one of the foot points .* outside/)])
    expect(stageProblems(setStagePoint(d, 'feet', 'left', 1, -1))).toHaveLength(1)
    expect(stageProblems(setStagePoint(d, 'feet', 'left', 1, 159))).toEqual([])  // the last row is inside
    d = { ...d, graphics: { ...d.graphics!, battle: { stage: { ...stageOf(d)!, platformFar: { left: [0, 0], right: [1, 1] } } } } } as ProjectDoc
    expect(stageProblems(d)).toEqual([expect.stringMatching(/need the platform places/)])
    expect(stageProblems(doc())).toEqual([])
  })
})

describe('platform right-hand pictures', () => {
  it('a right and a farRight cell on a platform; far removal takes farRight with it', () => {
    let d = addPlatform(doc(), 'slab')
    expect(setPlatformRight(d, 'slab', [2, 0], true)).toBe(d)  // no far picture yet
    d = setPlatformPicture(d, 'slab', 'plat', [1, 0], true)
    d = setPlatformRight(d, 'slab', [2, 0])
    d = setPlatformRight(d, 'slab', [3, 0], true)
    expect(platformList(d)[0]).toMatchObject({ right: [2, 0], farRight: [3, 0], far: [1, 0] })
    expect(d.graphics!.battle!.platforms!.slab).toEqual({ sheet: 'plat', cell: [0, 0], far: [1, 0], right: [2, 0], farRight: [3, 0] })
    d = setPlatformRight(d, 'slab', undefined)
    expect('right' in d.graphics!.battle!.platforms!.slab).toBe(false)
    d = clearPlatformFar(d, 'slab')
    expect(d.graphics!.battle!.platforms!.slab).toEqual({ sheet: 'plat', cell: [0, 0] })
  })
})
