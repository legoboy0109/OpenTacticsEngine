import { describe, expect, it } from 'vitest'
import { assignLegend } from '../../src/model/legend'
import { gridProblems, readGrid, repairMap, terrainAt, writeGrid } from '../../src/model/mapGrid'
import { fillFrom, lineCells, paintCells } from '../../src/model/mapPaint'
import { addMap, blankMap } from '../../src/model/maps'
import { deleteTerrain } from '../../src/model/terrain'
import type { MapFile, Terrain } from '../../src/model/types'
import { loadDemo, loadTemplate, TEMPLATES } from './fixtures'

const srpg = () => loadDemo('demo-srpg')
const bridge = () => srpg().maps.bridge!
const terr = () => srpg().terrain!

describe('legend', () => {
  it('keeps the characters the file already uses', () => {
    const t = terr()
    const legend = assignLegend(['water', 'plain', 'forest'], { '~': 'water', '.': 'plain', f: 'forest' }, t)
    expect(legend).toEqual({ '.': 'plain', f: 'forest', '~': 'water' }) // ordered like the terrain list
    expect(legend['~']).toBe('water')
  })
  it('gives a new terrain its own glyph, else a free letter, and is deterministic', () => {
    const t: Terrain[] = [{ id: 'a', name: 'A', glyph: '.' }, { id: 'b', name: 'B', glyph: '.' }, { id: 'zed', name: 'Z' }]
    const l1 = assignLegend(['a', 'b', 'zed'], {}, t)
    expect(l1).toEqual({ '.': 'a', b: 'b', z: 'zed' })
    expect(assignLegend(['a', 'b', 'zed'], {}, t)).toEqual(l1)
  })
  it('never uses whitespace, quotes or backslashes, and copes with 80 terrains', () => {
    const t: Terrain[] = Array.from({ length: 80 }, (_, i) => ({ id: `t${i}`, name: `T${i}`, glyph: i % 3 === 0 ? ' ' : i % 3 === 1 ? '"' : '\\' }))
    const legend = assignLegend(t.map((x) => x.id), { ' ': 't0', '"': 't1' }, t)
    const chars = Object.keys(legend)
    expect(chars).toHaveLength(80)
    expect(new Set(chars).size).toBe(80)
    expect(chars.every((c) => c.length === 1 && c > ' ' && c <= '~' && c !== '"' && c !== '\\')).toBe(true)
  })
  it('replaces unusable existing characters', () => {
    const t = terr()
    const l = assignLegend(['plain'], { ' ': 'plain' }, t)
    expect(Object.keys(l)[0]).not.toBe(' ')
  })
})

describe('round trip', () => {
  const docs = [
    ...(['demo-srpg', 'demo-economy'] as const).map((n) => [n, loadDemo(n)] as const),
    ...TEMPLATES.map((n) => [`template ${n}`, loadTemplate(n)] as const),
  ]
  for (const [name, doc] of docs) {
    it(`${name}: grid read then write is the same map`, () => {
      for (const [id, map] of Object.entries(doc.maps)) {
        expect(gridProblems(map!, doc.terrain!), id).toEqual([])
        const out = writeGrid(map!, readGrid(map!, doc.terrain!), doc.terrain!)
        expect(out.tiles).toEqual(map!.tiles)
        expect(out.legend).toEqual(map!.legend)
        expect(out).toEqual(map)
      }
    })
  }
  it('unknown keys on the map survive painting', () => {
    const m = { ...bridge(), __keep: { a: 1 }, heights: undefined } as MapFile
    const out = paintCells(m, terr(), [{ x: 0, y: 0 }], 'water')
    expect((out as MapFile & { __keep: unknown }).__keep).toEqual({ a: 1 })
    expect(out.units).toBe(m.units)
    expect(out.sides).toBe(m.sides)
  })
})

describe('paint and fill', () => {
  it('paints cells, keeps the legend stable and drops unused legend entries', () => {
    const m = bridge()
    const out = paintCells(m, terr(), [{ x: 0, y: 0 }, { x: 1, y: 0 }], 'mountain')
    expect(out.tiles[0].slice(0, 3)).toBe('mm' + m.tiles[0][2])
    expect(out.legend).toEqual(expect.objectContaining({ m: 'mountain', '.': 'plain' }))
    const wiped = paintCells(m, terr(), Array.from({ length: 9 }, (_, y) => Array.from({ length: 13 }, (_, x) => ({ x, y }))).flat(), 'plain')
    expect(wiped.legend).toEqual({ '.': 'plain' })
    expect(wiped.tiles.every((r) => r === '.'.repeat(13))).toBe(true)
  })
  it('returns the same object when nothing changes', () => {
    const m = bridge()
    expect(paintCells(m, terr(), [{ x: 0, y: 0 }], 'plain')).toBe(m)
    expect(paintCells(m, terr(), [{ x: 99, y: 0 }], 'water')).toBe(m)
    expect(paintCells(m, terr(), [{ x: 0, y: 0 }], 'nope')).toBe(m)
  })
  it('flood fill is 4-connected and stays inside the same terrain', () => {
    const m: MapFile = { ...blankMap(srpg(), 'T', 4, 3), }
    let g = paintCells(m, terr(), [{ x: 1, y: 0 }, { x: 1, y: 1 }, { x: 0, y: 2 }, { x: 1, y: 2 }], 'water')
    g = paintCells(g, terr(), [{ x: 2, y: 1 }], 'forest') // an island the fill must not leak through
    const filled = fillFrom(g, terr(), 0, 0, 'mountain')
    // The left column cells (0,0),(0,1) are cut off from the rest by water; (0,2) is water.
    expect(filled.tiles.map((r) => [...r].map((c) => filled.legend[c]))).toEqual([
      ['mountain', 'water', 'plain', 'plain'],
      ['mountain', 'water', 'forest', 'plain'],
      ['water', 'water', 'plain', 'plain'],
    ])
    // Diagonal neighbours are not connected.
    const diag = fillFrom(g, terr(), 2, 0, 'mountain')
    expect(terrainAt(diag, 3, 1)).toBe('mountain')
    expect(terrainAt(diag, 2, 1)).toBe('forest')
    expect(terrainAt(diag, 0, 0)).toBe('plain')
  })
  it('fill with the same terrain changes nothing', () => {
    const m = bridge()
    expect(fillFrom(m, terr(), 0, 0, 'plain')).toBe(m)
  })
  it('lineCells has no gaps', () => {
    const cells = lineCells({ x: 0, y: 0 }, { x: 7, y: 3 })
    expect(cells[0]).toEqual({ x: 0, y: 0 })
    expect(cells.at(-1)).toEqual({ x: 7, y: 3 })
    for (let i = 1; i < cells.length; i++) expect(Math.max(Math.abs(cells[i].x - cells[i - 1].x), Math.abs(cells[i].y - cells[i - 1].y))).toBeLessThanOrEqual(1)
  })
})

describe('repair', () => {
  it('reports and fixes wrong row lengths, row counts and unknown symbols', () => {
    const t = terr()
    const bad: MapFile = { ...bridge(), tiles: ['..f', '.?....', '....'], grid: { type: 'square', width: 4, height: 2 } }
    const problems = gridProblems(bad, t)
    expect(problems.join(' ')).toMatch(/2 rows but has 3/)
    expect(problems.join(' ')).toMatch(/Row 1/)
    expect(problems.join(' ')).toMatch(/"\?"/)
    const fixed = repairMap(bad, t)
    expect(gridProblems(fixed, t)).toEqual([])
    expect(fixed.tiles).toHaveLength(2)
    expect(fixed.tiles.every((r) => r.length === 4)).toBe(true)
    expect(terrainAt(fixed, 2, 0)).toBe('forest')
    expect(terrainAt(fixed, 1, 1)).toBe('plain') // unknown "?" -> first terrain
  })
  it('handles a legend naming unknown terrain and bad heights', () => {
    const t = terr()
    const bad: MapFile = { ...bridge(), legend: { ...bridge().legend, '~': 'lava' }, heights: [[1]] }
    expect(gridProblems(bad, t).join(' ')).toMatch(/lava/)
    const fixed = repairMap(bad, t)
    expect(gridProblems(fixed, t)).toEqual([])
    expect(fixed.heights).toHaveLength(9)
    expect(fixed.heights![0]).toHaveLength(13)
    expect(fixed.heights![0][0]).toBe(1)
  })
  it('does not crash on garbage', () => {
    const t = terr()
    const junk = { name: 'x', grid: { width: 'a' }, legend: null, tiles: 'nope' } as unknown as MapFile
    expect(gridProblems(junk, t).length).toBeGreaterThan(0)
    const fixed = repairMap(junk, t)
    expect(gridProblems(fixed, t)).toEqual([])
  })
})

describe('new map', () => {
  it('is 12 x 8 of the first terrain with two sides and no units', () => {
    const d = addMap(srpg(), 'Fresh One')
    const m = d.maps.fresh_one!
    expect(m.grid).toEqual({ type: 'square', width: 12, height: 8 })
    expect(m.tiles).toHaveLength(8)
    expect(m.tiles.every((r) => r.length === 12)).toBe(true)
    expect(Object.values(m.legend)).toEqual([d.terrain![0].id])
    expect(m.sides!.map((s) => [s.name, s.color, s.controller])).toEqual([['Blue', '#3d85c6', 'human'], ['Red', '#cc0000', 'ai']])
    expect(m.units).toEqual([])
    expect(d.game!.maps.at(-1)).toEqual({ id: 'fresh_one', name: 'Fresh One', file: 'maps/fresh_one.json' })
    expect(gridProblems(m, d.terrain!)).toEqual([])
  })
})

describe('terrain deletion and maps', () => {
  it('tiles of a deleted terrain become the first remaining one and the legend stays loadable', () => {
    const d = srpg()
    const next = deleteTerrain(d, 'plain')
    const m = next.maps.bridge!
    expect(Object.values(m.legend)).not.toContain('plain')
    expect(gridProblems(m, next.terrain!)).toEqual([])
    expect(terrainAt(m, 0, 0)).toBe(next.terrain![0].id)
    // The grid model reads it and re-writes a clean legend.
    const out = writeGrid(m, readGrid(m, next.terrain!), next.terrain!)
    expect(new Set(Object.values(out.legend)).size).toBe(Object.keys(out.legend).length)
    expect(gridProblems(out, next.terrain!)).toEqual([])
  })
})
