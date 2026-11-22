// A map's exits and the game's drop rule in the editor's model (docs/AI.md "Looting and escaping", docs/ITEMS.md "Drops").
import { describe, expect, it } from 'vitest'
import { exitProblems, exitsOf, freeExitSquare, setExits } from '../../src/model/exits'
import { itemDrops, setItemDrops } from '../../src/model/itemRules'
import { sellChecks } from '../../src/model/sell'
import { resizeMap } from '../../src/model/mapResize'
import type { MapFile, ProjectDoc } from '../../src/model/types'

const map = (exits?: unknown): MapFile => ({ name: 'M', grid: { width: 4, height: 3 }, legend: {}, tiles: [], ...(exits ? { exits } : {}) }) as unknown as MapFile
const doc = (m: MapFile): ProjectDoc => ({ maps: { m } }) as unknown as ProjectDoc

describe('map exits', () => {
  it('are written as a list and the key goes with the last one', () => {
    let d = setExits(doc(map()), 'm', [{ x: 0, y: 1 }])
    expect(d.maps.m!.exits).toEqual([{ x: 0, y: 1 }])
    d = setExits(d, 'm', [])
    expect(d.maps.m).not.toHaveProperty('exits')
    expect(exitsOf(d.maps.m!)).toEqual([])
  })
  it('a new exit takes the first square that is not one yet', () => {
    expect(freeExitSquare(map())).toEqual({ x: 0, y: 0 })
    expect(freeExitSquare(map([{ x: 0, y: 0 }, { x: 1, y: 0 }]))).toEqual({ x: 2, y: 0 })
  })
  it('are checked like the loader does', () => {
    expect(exitProblems(map())).toEqual([])
    expect(exitProblems(map([{ x: 3, y: 2 }]))).toEqual([])
    expect(exitProblems(map([]))[0]).toContain('1 to 64')
    expect(exitProblems(map([{ x: 4, y: 0 }])).join()).toContain('outside the map')
    expect(exitProblems(map([{ x: 1, y: 1 }, { x: 1, y: 1 }])).join()).toContain('an exit twice')
    expect(exitProblems(map([{ x: 1 }])).join()).toContain('needs a column and a row')
    expect(exitProblems(map(Array.from({ length: 65 }, (_, i) => ({ x: i % 4, y: 0 }))))[0]).toContain('1 to 64')
  })
  it('follow a resize: they move with the map and the ones that fall outside go', () => {
    const m = map([{ x: 0, y: 0 }, { x: 3, y: 2 }])
    expect(resizeMap(m, [], [], 4, 2, 'nw').exits).toEqual([{ x: 0, y: 0 }])
    expect(resizeMap(m, [], [], 6, 3, 'se').exits).toEqual([{ x: 2, y: 0 }, { x: 5, y: 2 }])
    expect(resizeMap(map([{ x: 3, y: 2 }]), [], [], 3, 2, 'nw')).not.toHaveProperty('exits')
  })
})

describe('rules.items drops', () => {
  it('"marked" is the default and is not written; "last" is, and an all-default block goes', () => {
    const d = { game: { rules: {} } } as unknown as ProjectDoc
    expect(itemDrops(d)).toBe('marked')
    const last = setItemDrops(d, 'last')
    expect(last.game!.rules!.items).toEqual({ drops: 'last' })
    expect(itemDrops(last)).toBe('last')
    const bad = { game: { rules: { items: { drops: 'first' } } } } as unknown as ProjectDoc
    expect(sellChecks(bad).map((c) => c.text).join()).toContain('only its last item')
    expect(sellChecks(last)).toEqual([])
    expect(setItemDrops(last, 'marked').game!.rules).not.toHaveProperty('items')
  })
})
