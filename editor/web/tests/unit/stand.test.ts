import { describe, expect, it } from 'vitest'
import { removeSheet, lookUsesSheet } from '../../src/model/graphics'
import { dropArtOf } from '../../src/model/graphicsClean'
import { getLook, setLook } from '../../src/model/graphicsLooks'
import { outsideUsers, sheetUsers } from '../../src/model/graphicsUsage'
import {
  addStandModel, removeStandModel, setStand, setStandHeight, setStandModels, setStandPicture, setStandScatter, setStandTurn, setStandWidth,
  standFields, standKind, standModels, standOf, standPictureLook,
} from '../../src/model/stand'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const slot = { kind: 'terrain' as const, id: 'forest', style: 'solid' }
/** A game with one image, a 3D style and a forest that has its own ground picture in it. */
const withGround = (): ProjectDoc => {
  const d = loadTemplate('strategy-rpg')
  return setLook({ ...d, graphics: { sheets: [{ id: 'g', file: 'art/g.png', cellWidth: 24, cellHeight: 24 }, { id: 'trees', file: 'art/trees.png', cellWidth: 24, cellHeight: 32 }] } }, slot, { sheet: 'g', cell: [0, 0] })
}

describe('kinds', () => {
  it('tells a picture from models from nothing', () => {
    expect(standKind(undefined)).toBe('none')
    expect(standKind({})).toBe('none')
    expect(standKind({ sheet: 'trees', cell: [0, 0] })).toBe('picture')
    expect(standKind({ model: 'models/t.glb' })).toBe('model')
    expect(standKind({ variants: ['models/a.glb', 'models/b.glb'] })).toBe('model')
    expect(standKind({ sheet: 'trees', variants: [[0, 0], [1, 0]] })).toBe('picture')
    expect(standModels({ variants: ['models/a.glb', 'models/b.glb'] })).toEqual(['models/a.glb', 'models/b.glb'])
    expect(standModels({ model: 'models/t.glb' })).toEqual(['models/t.glb'])
    expect(standModels({ sheet: 'trees', cell: [0, 0] })).toEqual([])
  })
  it('a picture stand reads as an ordinary look', () => {
    expect(standPictureLook({ sheet: 'trees', frames: [[0, 0], [1, 0]], frameMs: 300, width: 30, scatter: true })).toEqual({ sheet: 'trees', frames: [[0, 0], [1, 0]], frameMs: 300 })
    expect(standPictureLook({ model: 'models/t.glb' })).toBeNull()
  })
})

describe('editing a picture stand', () => {
  it('a piece, then a random mix: written as cell / variants, width and scatter stay', () => {
    let d = setStandPicture(withGround(), slot, { sheet: 'trees', cell: [0, 0] })
    expect(standOf(d, slot)).toEqual({ sheet: 'trees', cell: [0, 0] })
    d = setStandWidth(d, slot, 30)
    d = setStandScatter(d, slot, true)
    d = setStandPicture(d, slot, { sheet: 'trees', variants: [[0, 0], [1, 0], [2, 0]] })
    expect(standOf(d, slot)).toEqual({ sheet: 'trees', variants: [[0, 0], [1, 0], [2, 0]], width: 30, scatter: true })
    expect(getLook(d, slot)).toMatchObject({ sheet: 'g', cell: [0, 0] }) // the ground picture is untouched
  })
  it('an animation keeps its speed; defaults are not written', () => {
    let d = setStandPicture(withGround(), slot, { sheet: 'trees', frames: [[0, 0], [1, 0]], frameMs: 250 })
    expect(standOf(d, slot)).toEqual({ sheet: 'trees', frames: [[0, 0], [1, 0]], frameMs: 250 })
    d = setStandWidth(d, slot, 24)
    d = setStandWidth(d, slot, undefined)
    d = setStandScatter(d, slot, true)
    d = setStandScatter(d, slot, false)
    expect(standOf(d, slot)).toEqual({ sheet: 'trees', frames: [[0, 0], [1, 0]], frameMs: 250 })
  })
  it('a terrain with no picture of its own in the style cannot have one', () => {
    const d = loadTemplate('strategy-rpg')
    expect(setStandPicture(d, slot, { sheet: 'trees', cell: [0, 0] })).toBe(d)
    const none = setLook(withGround(), slot, null)
    expect(setStandModels(none, slot, ['models/t.glb'])).toBe(none)
  })
})

describe('editing a model stand', () => {
  it('one file is `model`, several are `variants`; height, turn and scatter stay', () => {
    let d = setStandModels(withGround(), slot, ['models/a.glb'])
    expect(standOf(d, slot)).toEqual({ model: 'models/a.glb' })
    d = setStandHeight(d, slot, 1.4)
    d = setStandTurn(d, slot, 90)
    d = setStandScatter(d, slot, true)
    d = addStandModel(d, slot, 'models/b.glb')
    expect(standOf(d, slot)).toEqual({ variants: ['models/a.glb', 'models/b.glb'], height: 1.4, turn: 90, scatter: true })
    d = addStandModel(d, slot, 'models/b.glb') // already there
    expect(standModels(standOf(d, slot))).toEqual(['models/a.glb', 'models/b.glb'])
    d = removeStandModel(d, slot, 'models/a.glb')
    expect(standOf(d, slot)).toEqual({ model: 'models/b.glb', height: 1.4, turn: 90, scatter: true })
    d = removeStandModel(d, slot, 'models/b.glb')
    expect(standOf(d, slot)).toBeUndefined()
    expect(getLook(d, slot)).toEqual({ sheet: 'g', cell: [0, 0] })
  })
  it('turn 0 and an empty height are not written; the same value returns the same document', () => {
    let d = setStandModels(withGround(), slot, ['models/a.glb'])
    expect(setStandTurn(d, slot, 0)).toBe(d)
    d = setStandHeight(d, slot, 2)
    expect(setStandHeight(d, slot, 2)).toBe(d)
    d = setStandHeight(d, slot, undefined)
    expect(standOf(d, slot)).toEqual({ model: 'models/a.glb' })
  })
  it('changing between a picture and models drops the other one\'s settings', () => {
    let d = setStandPicture(withGround(), slot, { sheet: 'trees', cell: [0, 0] })
    d = setStandWidth(d, slot, 30)
    d = setStandModels(d, slot, ['models/a.glb'])
    expect(standOf(d, slot)).toEqual({ model: 'models/a.glb' })
    d = setStandHeight(d, slot, 2)
    d = setStandPicture(d, slot, { sheet: 'trees', cell: [1, 0] })
    expect(standOf(d, slot)).toEqual({ sheet: 'trees', cell: [1, 0] })
  })
  it('fields show stored values only', () => {
    expect(standFields({ model: 'x.glb', height: 2, scatter: true })).toEqual({ width: undefined, height: 2, turn: undefined, scatter: true })
    expect(standFields(undefined)).toEqual({ width: undefined, height: undefined, turn: undefined, scatter: false })
  })
  it('unknown keys of a stand survive an edit', () => {
    let d = withGround()
    d = setStand(d, slot, { model: 'models/a.glb', future: { a: 1 } } as never)
    d = setStandTurn(d, slot, 45)
    expect(standOf(d, slot)).toEqual({ model: 'models/a.glb', future: { a: 1 }, turn: 45 })
  })
})

describe('the stand stays with its terrain', () => {
  it('changing the ground picture keeps the standing part; removing the style entry takes it along', () => {
    let d = setStandModels(withGround(), slot, ['models/a.glb'])
    d = setLook(d, slot, { sheet: 'g', cell: [3, 0] })
    expect(getLook(d, slot)).toEqual({ sheet: 'g', cell: [3, 0], stand: { model: 'models/a.glb' } })
    d = setLook(d, slot, undefined)
    expect(getLook(d, slot)).toBeUndefined()
  })
  it('deleting the terrain removes its stand with its pictures', () => {
    const d = dropArtOf(setStandModels(withGround(), slot, ['models/a.glb']), 'terrain', 'forest')
    expect(d.graphics!.styles).toBeUndefined()
  })
})

describe('images', () => {
  const withPicture = () => setStandPicture(withGround(), slot, { sheet: 'trees', cell: [0, 0] })
  it('a stand picture counts as using its image, and is listed with its terrain', () => {
    const d = withPicture()
    expect(lookUsesSheet(getLook(d, slot), 'trees')).toBe(true)
    expect(sheetUsers(d, 'trees').map((u) => u.id)).toEqual(['forest'])
    expect(sheetUsers(d, 'g').map((u) => u.id)).toEqual(['forest'])
  })
  it('removing the image removes only the standing picture; the ground stays', () => {
    const d = removeSheet(withPicture(), 'trees')
    expect(getLook(d, slot)).toEqual({ sheet: 'g', cell: [0, 0] })
  })
  it('removing the ground image removes the whole entry, stand included', () => {
    const d = removeSheet(withPicture(), 'g')
    expect(getLook(d, slot)).toBeUndefined()
  })
  it('models are not images: removing an image keeps a model stand', () => {
    const d = removeSheet(setStandModels(withGround(), slot, ['models/a.glb']), 'trees')
    expect(standOf(d, slot)).toEqual({ model: 'models/a.glb' })
  })
  it('a standing picture outside a smaller image is reported', () => {
    const d = setStandPicture(withGround(), slot, { sheet: 'trees', cell: [3, 0] })
    expect(outsideUsers(d, 'trees', 2, 1).map((u) => u.id)).toEqual(['forest'])
    expect(outsideUsers(d, 'trees', 4, 1)).toEqual([])
  })
})
