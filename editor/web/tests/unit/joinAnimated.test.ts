import { describe, expect, it } from 'vitest'
import { buildArtScene, type ImageSource } from '../../src/art/scene'
import { dropArtOf } from '../../src/model/graphicsClean'
import { emptyPick, lookFromPick, pickFromLook, type Pick } from '../../src/model/graphicsLooks'
import { savingMode, setSavingMode } from '../../src/model/saving'
import { deleteTerrain } from '../../src/model/terrain'
import type { Graphics, Look, ProjectDoc } from '../../src/model/types'
import { loadDemo, loadTemplate } from './fixtures'

const p = (o: Partial<Pick>): Pick => ({ ...emptyPick('tiles'), ...o })

describe('picker choice <-> animated join and joinExcept', () => {
  it('one block keeps the one-cell form; several make a list with a speed', () => {
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 4]] }))).toEqual({ sheet: 'tiles', join: [0, 4] })
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 4], [4, 4]], frameMs: 700 }))).toEqual({ sheet: 'tiles', join: [[0, 4], [4, 4]], frameMs: 700 })
    const many = Array.from({ length: 20 }, (_, i) => [i, 0] as [number, number])
    expect((lookFromPick(p({ kind: 'join', cells: many }))!.join as unknown[]).length).toBe(16)
  })
  it('"everything except" writes joinExcept and never joinWith (an empty list is kept)', () => {
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 4]], joinMode: 'except', joinExcept: ['water'], joinWith: ['x'] })))
      .toEqual({ sheet: 'tiles', join: [0, 4], joinExcept: ['water'] })
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 4]], joinMode: 'except' }))).toEqual({ sheet: 'tiles', join: [0, 4], joinExcept: [] })
  })
  it('reading and writing back is lossless for the new shapes', () => {
    const looks: Look[] = [
      { sheet: 'tiles', join: [[0, 4], [4, 4], [8, 4]], frameMs: 250, joinWith: ['bridge'] },
      { sheet: 'tiles', join: [0, 4], joinExcept: ['water'], joinEdge: false },
      { sheet: 'tiles', join: [[0, 4], [4, 4]], frameMs: 400, joinExcept: [] },
    ]
    for (const l of looks) expect(lookFromPick(pickFromLook(l, 'x'))).toEqual(l)
    expect(pickFromLook(looks[1], 'x').joinMode).toBe('except')
  })
})

describe('the canvas follows joinExcept', () => {
  const src: ImageSource = { image: () => ({ status: 'ready', img: {} as CanvasImageSource, width: 96, height: 96 }) as never }
  it('"same terrain" is every terrain except the listed ones; animated joins keep the clock running', () => {
    const d = loadDemo('demo-srpg')
    const ids = d.terrain!.map((t) => t.id)
    const water = ids[0], bridge = ids[1]
    const g: Graphics = {
      sheets: [{ id: 't', file: 'art/t.png', cellWidth: 8, cellHeight: 8 }],
      terrain: { [bridge]: { sheet: 't', join: [0, 0], joinExcept: [water] }, [water]: { sheet: 't', join: [[0, 4], [4, 4]] } },
    }
    const scene = buildArtScene({ ...d, graphics: g }, undefined, src)!
    expect([...scene.joinSame.get(bridge)!].sort()).toEqual(ids.filter((x) => x !== water).sort())
    expect([...scene.joinSame.get(water)!]).toEqual([water])
    expect(scene.animated).toBe(true)
    const still = buildArtScene({ ...d, graphics: { ...g, terrain: { [bridge]: g.terrain![bridge] } } }, undefined, src)!
    expect(still.animated).toBe(false)
  })
})

describe('deleting a terrain cleans joinExcept too', () => {
  const g: Graphics = {
    sheets: [{ id: 't', file: 'art/t.png', cellWidth: 24, cellHeight: 24 }],
    terrain: { plain: { sheet: 't', cell: [0, 0] }, water: { sheet: 't', cell: [1, 0] }, bridge: { sheet: 't', join: [0, 2], joinExcept: ['water', 'plain'] } },
  }
  it('removes the id from the list; the emptied list stays (it still means "everything")', () => {
    const d0 = loadTemplate('strategy-rpg')
    const doc: ProjectDoc = { ...d0, terrain: ['plain', 'water', 'bridge'].map((id) => ({ id, name: id })), graphics: g }
    expect(dropArtOf(doc, 'terrain', 'water').graphics!.terrain!.bridge).toEqual({ sheet: 't', join: [0, 2], joinExcept: ['plain'] })
    const e = dropArtOf(dropArtOf(doc, 'terrain', 'water'), 'terrain', 'plain')
    expect(e.graphics!.terrain!.bridge).toEqual({ sheet: 't', join: [0, 2], joinExcept: [] })
    expect(deleteTerrain(doc, 'water').graphics!.terrain!.bridge).toEqual({ sheet: 't', join: [0, 2], joinExcept: ['plain'] })
  })
})

describe('saving setting', () => {
  it('any is the default and writes no key; the others write their word; an untouched game is the same object', () => {
    const d = loadTemplate('blank')
    expect(savingMode(d)).toBe('any')
    expect(setSavingMode(d, 'any')).toBe(d)
    const c = setSavingMode(d, 'continue')
    expect(c.game!.saving).toBe('continue')
    expect(savingMode(c)).toBe('continue')
    expect(setSavingMode(c, 'off').game!.saving).toBe('off')
    expect('saving' in setSavingMode(c, 'any').game!).toBe(false)
  })
})
