// Kinds of view (docs/VIEWS.md) and heights: the model the Game screen, the picture cards and the map editor build on.
import { describe, expect, it } from 'vitest'
import { buildArtScene, type ImageSource } from '../../src/art/scene'
import { updateStyle, updateStyleWith } from '../../src/model/game'
import { effectiveLook } from '../../src/model/graphicsLooks'
import { applyHeights, clampHeight, heightAt } from '../../src/model/mapHeights'
import { repairMap, gridProblems } from '../../src/model/mapGrid'
import { resizeMap } from '../../src/model/mapResize'
import { inheritsGeneralArt, isoDefaults, ISO_RANGE, viewKind, withIsoSize, withView } from '../../src/model/styleView'
import type { Graphics, ProjectDoc, Style } from '../../src/model/types'
import { beginStroke, type StrokeEnv, type ToolState } from '../../src/screens/maps/editor/tools'
import { MapViewState } from '../../src/screens/maps/editor/viewState'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import { loadDemo, loadTemplate } from './fixtures'

describe('kind of view of a style', () => {
  it('a missing key is top-down; isometric is editable; the reserved kinds are kept apart', () => {
    expect(viewKind({})).toBe('top-down')
    expect(viewKind({ view: 'top-down' })).toBe('top-down')
    expect(viewKind({ view: 'isometric' })).toBe('isometric')
    expect(viewKind({ view: 'isometric-3d' })).toBe('future')
    expect(viewKind({ view: '3d' })).toBe('future')
  })
  it('switching to Isometric writes view; switching back removes view and the sizes (byte-stable)', () => {
    const s: Style = { id: 'classic', name: 'Classic', background: '#101010' }
    const iso = withIsoSize(withView(s, 'isometric'), 'tileWidth', 64)
    expect(iso).toEqual({ id: 'classic', name: 'Classic', background: '#101010', view: 'isometric', isometric: { tileWidth: 64 } })
    const back = withView(iso, 'top-down')
    expect(JSON.stringify(back)).toBe(JSON.stringify(s))
  })
  it('an emptied size leaves; the last one takes the isometric object with it', () => {
    let s = withIsoSize(withView({ id: 'a', name: 'A' }, 'isometric'), 'heightStep', 6)
    s = withIsoSize(s, 'unitWidth', 20)
    expect(withIsoSize(s, 'heightStep', undefined).isometric).toEqual({ unitWidth: 20 })
    expect(JSON.stringify(withIsoSize(withIsoSize(s, 'heightStep', undefined), 'unitWidth', undefined))).toBe(JSON.stringify({ id: 'a', name: 'A', view: 'isometric' }))
  })
  it('the placeholder sizes follow VIEWS.md: 2 x tile size, then halves', () => {
    expect(isoDefaults({ view: { tileSize: 24 } } as never, undefined)).toEqual({ tileWidth: 48, tileHeight: 24, heightStep: 12, unitWidth: 24 })
    expect(isoDefaults(null, undefined)).toEqual({ tileWidth: 48, tileHeight: 24, heightStep: 12, unitWidth: 24 })
    expect(isoDefaults({ view: { tileSize: 16 } } as never, undefined).tileWidth).toBe(32)
    // A size the maker typed feeds the ones that are derived from it.
    expect(isoDefaults(null, { tileWidth: 64 })).toEqual({ tileWidth: 64, tileHeight: 32, heightStep: 16, unitWidth: 32 })
    expect(isoDefaults(null, { tileHeight: 10 })).toEqual({ tileWidth: 48, tileHeight: 10, heightStep: 5, unitWidth: 24 })
  })
  it('the ranges are the loader\'s', () => {
    expect(ISO_RANGE).toEqual({
      tileWidth: { min: 8, max: 512, even: true }, tileHeight: { min: 4, max: 256, even: true },
      heightStep: { min: 0, max: 128, even: false }, unitWidth: { min: 4, max: 512, even: false },
    })
  })
  it('a future style survives an edit of another style untouched', () => {
    const d = loadTemplate('strategy-rpg')
    const g = { ...d.game!, styles: [{ id: 'a', name: 'A' }, { id: 'b', name: 'B', view: '3d', isometric: { tileWidth: 7 } }] }
    const next = updateStyleWith({ ...d, game: g }, 'a', (s) => withView(s, 'isometric'))
    expect(next.game!.styles![1]).toBe(g.styles[1])
    expect(updateStyle({ ...d, game: g }, 'a', { name: 'Z' }).game!.styles![1]).toBe(g.styles[1])
  })
})

describe('a style of another kind of view inherits no general pictures', () => {
  const g: Graphics = {
    sheets: [{ id: 't', file: 'art/t.png', cellWidth: 8, cellHeight: 8 }],
    terrain: { plain: { sheet: 't', cell: [0, 0] }, forest: { sheet: 't', cell: [1, 0] } },
    styles: { iso: { terrain: { forest: { sheet: 't', cell: [2, 0] } } }, flat: { terrain: { forest: null } } },
  }
  it('effectiveLook only falls back to the base when asked to', () => {
    expect(effectiveLook(g, 'terrain', 'plain', 'iso')).toEqual({ sheet: 't', cell: [0, 0] })
    expect(effectiveLook(g, 'terrain', 'plain', 'iso', false)).toBeNull()
    expect(effectiveLook(g, 'terrain', 'forest', 'iso', false)).toEqual({ sheet: 't', cell: [2, 0] })
    expect(effectiveLook(g, 'terrain', 'plain', undefined, false)).toEqual({ sheet: 't', cell: [0, 0] })
  })
  it('the scene of an isometric style has only its own pictures; top-down styles still inherit', () => {
    const d: ProjectDoc = { ...loadTemplate('strategy-rpg'), graphics: g }
    d.game = { ...d.game!, styles: [{ id: 'flat', name: 'Flat' }, { id: 'iso', name: 'Iso', view: 'isometric' }, { id: 'later', name: 'Later', view: 'isometric-3d' }] }
    const src: ImageSource = { image: () => ({ status: 'ready', img: {} as CanvasImageSource, width: 96, height: 96 }) as never }
    expect([...buildArtScene(d, 'iso', src)!.terrain.keys()]).toEqual(['forest'])
    expect(buildArtScene(d, 'later', src)!.terrain.size).toBe(0) // no pictures of its own, none inherited
    expect([...buildArtScene(d, 'flat', src)!.terrain.keys()]).toContain('plain')
    expect([...buildArtScene(d, undefined, src)!.terrain.keys()].sort()).toEqual(['forest', 'plain'])
    expect(inheritsGeneralArt(d.game, 'iso')).toBe(false)
    expect(inheritsGeneralArt(d.game, 'later')).toBe(false)
    expect(inheritsGeneralArt(d.game, 'flat')).toBe(true)
    expect(inheritsGeneralArt(d.game, undefined)).toBe(true)
  })
})

describe('heights', () => {
  const plain = () => loadDemo('demo-economy').maps[Object.keys(loadDemo('demo-economy').maps)[0]]!
  const cells = (...c: Array<[number, number]>) => c.map(([x, y]) => ({ x, y }))

  it('raise, lower and set change the squares; a square listed twice changes once', () => {
    const m = plain()
    const up = applyHeights(m, cells([0, 0], [0, 0], [1, 0]), { kind: 'raise' })
    expect(heightAt(up, 0, 0)).toBe(1)
    expect(heightAt(up, 1, 0)).toBe(1)
    expect(up.heights).toHaveLength(m.grid.height)
    expect(up.heights![0]).toHaveLength(m.grid.width)
    expect(heightAt(applyHeights(up, cells([0, 0]), { kind: 'lower' }), 0, 0)).toBe(0)
    expect(heightAt(applyHeights(up, cells([2, 2]), { kind: 'set', value: 5 }), 2, 2)).toBe(5)
  })
  it('stays within -8..32 and does not change the map when nothing moves', () => {
    expect(clampHeight(99)).toBe(32)
    expect(clampHeight(-99)).toBe(-8)
    const m = plain()
    expect(heightAt(applyHeights(m, cells([0, 0]), { kind: 'set', value: 99 }), 0, 0)).toBe(32)
    expect(heightAt(applyHeights(m, cells([0, 0]), { kind: 'set', value: -99 }), 0, 0)).toBe(-8)
    const top = applyHeights(m, cells([0, 0]), { kind: 'set', value: 32 })
    expect(applyHeights(top, cells([0, 0]), { kind: 'raise' })).toBe(top)
    expect(applyHeights(m, cells([0, 0]), { kind: 'set', value: 0 })).toBe(m)
    expect(applyHeights(m, cells([-1, 0], [999, 0]), { kind: 'raise' })).toBe(m)
  })
  it('writes heights only when some square is not 0, and keeps a map without them byte-stable', () => {
    const m = plain()
    expect(m.heights).toBeUndefined()
    const up = applyHeights(m, cells([1, 1]), { kind: 'raise' })
    const down = applyHeights(up, cells([1, 1]), { kind: 'lower' })
    expect('heights' in down).toBe(false)
    expect(JSON.stringify(down)).toBe(JSON.stringify(m))
    // A negative square also counts as "not 0".
    expect(applyHeights(m, cells([1, 1]), { kind: 'lower' }).heights![1][1]).toBe(-1)
  })
  it('resizing keeps heights with their squares; new squares are 0', () => {
    const d = loadDemo('demo-economy')
    const m = applyHeights(plain(), cells([0, 0]), { kind: 'set', value: 7 })
    const bigger = resizeMap(m, d.terrain!, d.structures ?? [], m.grid.width + 2, m.grid.height + 1, 'nw')
    expect(heightAt(bigger, 0, 0)).toBe(7)
    expect(heightAt(bigger, m.grid.width + 1, m.grid.height)).toBe(0)
    expect(bigger.heights).toHaveLength(m.grid.height + 1)
    const moved = resizeMap(m, d.terrain!, d.structures ?? [], m.grid.width + 2, m.grid.height + 1, 'se')
    expect(heightAt(moved, 2, 1)).toBe(7)
    expect(gridProblems(moved, d.terrain!)).toEqual([])
  })
  it('a map whose heights do not match its size is reported and repaired', () => {
    const d = loadDemo('demo-economy')
    const bad = { ...plain(), heights: [[1, 2]] }
    expect(gridProblems(bad, d.terrain!)).toContain('The height data does not match the size.')
    expect(gridProblems(repairMap(bad, d.terrain!), d.terrain!)).toEqual([])
  })
  it('heightAt is 0 for a map with none, and reads the demo map that has them', () => {
    expect(heightAt(plain(), 3, 3)).toBe(0)
    const srpg = loadDemo('demo-srpg').maps.bridge!
    if (srpg.heights) expect(heightAt(srpg, 0, 0)).toBe(srpg.heights[0][0])
  })
})

describe('a Heights drag', () => {
  /** A store that only does what the stroke needs: read the document, edit it with an undo key. */
  function fake() {
    const d0 = loadDemo('demo-economy')
    const id = Object.keys(d0.maps)[0]
    let h = initHistory(d0)
    let t = 0
    const store = {
      getState: () => ({ doc: h.present }),
      edit: (fn: (d: ProjectDoc) => ProjectDoc, key?: string) => { h = pushEdit(h, fn(h.present), key, t++) },
    }
    const tools: ToolState = { tool: 'heights', terrainId: '', fill: false, unitType: '', unitSide: '', structureType: '', structureSide: '', heightMode: 'raise', heightValue: 1 }
    const env = (over: Partial<ToolState> = {}): StrokeEnv => ({ store: store as never, mapId: id, view: new MapViewState(), tools: () => ({ ...tools, ...over }), pickTerrain: () => {}, say: () => {} })
    return { env, hist: () => h, id, undo: () => { h = undo(h) } }
  }

  it('one drag is one undo step, and a square the pointer crosses again is not raised twice', () => {
    const f = fake()
    const s = beginStroke(f.env(), { x: 0, y: 0 })
    s.move({ x: 3, y: 0 })
    s.move({ x: 0, y: 0 })
    s.end({ x: 0, y: 0 })
    const m = f.hist().present.maps[f.id]!
    expect([0, 1, 2, 3].map((x) => heightAt(m, x, 0))).toEqual([1, 1, 1, 1])
    expect(f.hist().past).toHaveLength(1)
    f.undo()
    expect(f.hist().present.maps[f.id]!.heights).toBeUndefined()
  })
  it('"Set to" writes the number, a second drag is its own step', () => {
    const f = fake()
    beginStroke(f.env({ heightMode: 'set', heightValue: 4 }), { x: 1, y: 1 }).end({ x: 1, y: 1 })
    beginStroke(f.env({ heightMode: 'lower' }), { x: 1, y: 1 }).end({ x: 1, y: 1 })
    expect(heightAt(f.hist().present.maps[f.id]!, 1, 1)).toBe(3)
    expect(f.hist().past).toHaveLength(2)
  })
})
