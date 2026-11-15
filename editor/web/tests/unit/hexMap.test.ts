// Hexagon maps in the document model: structures with rings (place / refuse / move / resize), unit placing, fill, the sight
// aid, drawing order and join pieces, on the engine's own hexagon fixtures.
import { describe, expect, it } from 'vitest'
import { gridOfGame } from '../../src/model/gridSpec'
import { neighbours } from '../../src/model/hex'
import { seenAtStart } from '../../src/model/fogSight'
import { ctxOf, editMap } from '../../src/model/mapCtx'
import { makeGeo } from '../../src/model/mapGeo'
import { mapWarnings } from '../../src/model/mapChecks'
import { fillFrom } from '../../src/model/mapPaint'
import { resizeLoss, resizeMap } from '../../src/model/mapResize'
import { fitProblem, footprint, misfits, moveStructureBy, placeStructure, structureAt } from '../../src/model/mapStructures'
import { addMap } from '../../src/model/maps'
import { placeUnit } from '../../src/model/mapUnits'
import { joinBlockSide, pickCell } from '../../src/model/pickCell'
import { ringsOf, setRings, setSize } from '../../src/model/structures'
import { upgradeTargets } from '../../src/model/structuresEconomy'
import { drawOrder } from '../../src/screens/maps/editor/drawStructures'
import type { Scene } from '../../src/screens/maps/editor/drawTypes'
import { loadHexFixture } from './fixtures'

for (const layout of ['pointy', 'flat'] as const) {
  describe(`hexagon game (${layout})`, () => {
    const doc = () => loadHexFixture(layout)
    const map = () => doc().maps.skirmish!

    it('reads the grid from game.json and a new map is a hexagon map', () => {
      const d = doc()
      expect(gridOfGame(d.game)).toEqual({ type: 'hex', layout })
      expect(addMap(d, 'Another').maps.another!.grid.type).toBe('hex')
    })

    it('a structure with rings covers the middle hexagon and its six neighbours in the contract order', () => {
      const d = doc()
      const c = ctxOf(d)!
      const hq = d.structures!.find((s) => s.id === 'hq')!
      expect(ringsOf(hq)).toBe(1)
      const cells = footprint(c.grid, hq, 5, 3)
      expect(cells[0]).toEqual({ x: 5, y: 3 })
      expect(cells.slice(1)).toEqual(neighbours(layout, 5, 3))
      expect(footprint(c.grid, d.structures!.find((s) => s.id === 'city'), 5, 3)).toEqual([{ x: 5, y: 3 }])
    })

    it('structureAt finds a seven-hexagon structure from every hexagon it covers, and not from beyond its ring', () => {
      const d = doc(); const c = ctxOf(d)!; const m = map()
      for (const cell of footprint(c.grid, d.structures!.find((s) => s.id === 'hq'), 1, 4)) expect(structureAt(m, c, cell.x, cell.y)?.type).toBe('hq')
      expect(structureAt(m, c, 4, 6)).toBeUndefined()
    })

    it('placing is refused at the map edge and on overlap with a plain sentence, and allowed where it fits', () => {
      const d = doc(); const c = ctxOf(d)!; const m = map()
      expect(placeStructure(m, c, { type: 'hq', x: 0, y: 4 }).error).toMatch(/covers a hexagon and the six around it, and would stick out/)
      expect(placeStructure(m, c, { type: 'hq', x: 5, y: 0 }).error).toMatch(/stick out/)
      expect(placeStructure(m, c, { type: 'hq', x: 3, y: 4 }).error).toMatch(/would overlap/) // reaches the blue HQ's ring
      expect(placeStructure(m, c, { type: 'city', x: 1, y: 4 }).error).toMatch(/would overlap the Headquarters/)
      expect(placeStructure(m, c, { type: 'city', x: 99, y: 4 }).error).toMatch(/outside the map/)
      const ok = placeStructure(m, c, { type: 'hq', x: 6, y: 1 })
      expect(ok.error).toBeNull()
      expect(ok.map.structures!.at(-1)).toEqual({ type: 'hq', x: 6, y: 1 })
    })

    it('moving keeps the structure whole; a move that would overlap is refused', () => {
      const d = doc(); const c = ctxOf(d)!; const m = map()
      const i = m.structures!.findIndex((s) => s.type === 'hq' && s.x === 1)
      expect(fitProblem(m, c, 'hq', 1, 4, i)).toBeNull()
      expect(moveStructureBy(m, c, { x: 1, y: 4 }, 0, 0).error).toBeNull()
      expect(moveStructureBy(m, c, { x: 1, y: 4 }, 9, 0).error).toMatch(/overlap/) // lands on the red HQ at (10,4)
    })

    it('units: one per hexagon', () => {
      const d = doc(); const c = ctxOf(d)!; const m = map()
      expect(placeUnit(m, c, { type: 'infantry', side: 'blue', x: 0, y: 2 }).error).toBeNull()
      expect(placeUnit(m, c, { type: 'infantry', side: 'blue', x: 3, y: 4 }).error).toMatch(/already a unit/)
    })

    it('fill goes through the six neighbours: it never reaches fewer hexagons than the four-neighbour fill', () => {
      const d = doc(); const c = ctxOf(d)!; const m = map()
      const filled = fillFrom(m, c.terrain, 4, 0, 'forest', c.grid)
      const sq = fillFrom(m, c.terrain, 4, 0, 'forest')
      const count = (mm: typeof m) => mm.tiles.join('').split('').filter((ch) => mm.legend[ch] === 'forest').length
      expect(count(filled)).toBeGreaterThanOrEqual(count(sq))
      expect(filled.grid.type).toBe('hex')
    })

    it('resize keeps contents by column and row and drops structures that no longer fit completely', () => {
      const d = doc(); const m = map(); const types = d.structures!
      const grid = gridOfGame(d.game)
      // cut away the two right-hand columns: the red HQ at (10,4) (its ring reaches column 11) goes, the blue one stays
      const loss = resizeLoss(m, types, 10, 9, 'nw', grid)
      const names = loss.structures.map((s) => `${s.type}@${s.x},${s.y}`)
      expect(names).toContain('hq@10,4')
      expect(names).not.toContain('hq@1,4')
      const out = resizeMap(m, d.terrain!, types, 10, 9, 'nw', grid)
      expect(out.grid).toMatchObject({ type: 'hex', width: 10, height: 9 })
      expect(out.structures!.some((s) => s.type === 'hq' && s.x === 10)).toBe(false)
      expect(out.tiles).toHaveLength(9)
    })

    it('flags a structure that no longer fits', () => {
      const d = doc(); const c = ctxOf(d)!
      expect(misfits(map(), c)).toEqual([])
      const bad = { ...map(), structures: [...map().structures!, { type: 'hq', x: 0, y: 8 }] }
      expect(misfits(bad, c)).toEqual([bad.structures!.length - 1])
      expect(mapWarnings({ ...d, maps: { ...d.maps, skirmish: bad } }, bad).some((w) => w.key.startsWith('misfit'))).toBe(true)
    })

    it('the sight aid counts hexagon distance: sight 1 shows the hexagon and its six neighbours', () => {
      const d = doc()
      const m = { ...map(), fog: true, units: [{ type: 'infantry', side: 'blue', x: 6, y: 6 }], structures: [] }
      const seen = seenAtStart({ ...d, units: d.units!.map((u) => ({ ...u, vision: 1 })) }, m, 'blue')
      expect([...seen].reduce((n, v) => n + v, 0)).toBe(7)
      for (const n of neighbours(layout, 6, 6)) expect(seen[n.y * 12 + n.x]).toBe(1)
    })

    it('drawing order never goes backwards and lists every structure and unit once', () => {
      const d = doc(); const m = map()
      const geo = makeGeo(gridOfGame(d.game), 24, 12, 9)
      const scene: Scene = { map: m, terrain: new Map(), types: new Map(), structures: new Map(d.structures!.map((s) => [s.id, s])), flagged: new Set(), grid: geo.grid, geo }
      const order = drawOrder(scene)
      expect(order).toHaveLength(m.structures!.length + m.units!.length)
      for (let i = 1; i < order.length; i++) expect(order[i].key).toBeGreaterThanOrEqual(order[i - 1].key)
    })
  })
}

describe('structure size on a hexagon game', () => {
  it('rings 1 is written, 0 is not; width and height are left alone', () => {
    const d = loadHexFixture('pointy')
    expect(setRings(d, 'city', 1).structures!.find((s) => s.id === 'city')!.rings).toBe(1)
    expect(setRings(setRings(d, 'city', 1), 'city', 0).structures!.find((s) => s.id === 'city')).not.toHaveProperty('rings')
    expect(setSize(d, 'city', 1, 1).structures!.find((s) => s.id === 'city')).not.toHaveProperty('width')
  })
  it('upgrades may only target kinds with the same rings', () => {
    const d = loadHexFixture('pointy')
    expect(upgradeTargets(d, 'city').map((s) => s.id)).toEqual(['factory'])
    expect(upgradeTargets(d, 'hq')).toEqual([])
  })
})

describe('join pieces on hexagons', () => {
  const look = { sheet: 's', join: [2, 3] as [number, number] }
  it('the 64 pieces sit at column n % 8, row n / 8 of the 8 x 8 block; squares keep 4 x 4', () => {
    expect(joinBlockSide(true)).toBe(8)
    for (let n = 0; n < 64; n++) expect(pickCell(look, 0, 0, n, 0, 8)).toEqual([2 + (n % 8), 3 + Math.floor(n / 8)])
    expect(pickCell(look, 0, 0, 0xff, 0, 8)).toEqual([2 + 7, 3 + 7]) // only the low 6 bits count
    expect(pickCell(look, 0, 0, 5, 0)).toEqual([2 + 1, 3 + 1])
  })
})

describe('map edits keep the grid kind', () => {
  it('editMap on a hexagon map keeps grid.type hex', () => {
    const d = loadHexFixture('flat')
    const next = editMap(d, 'skirmish', (m, c) => fillFrom(m, c.terrain, 0, 0, 'forest', c.grid))
    expect(next.maps.skirmish!.grid.type).toBe('hex')
  })
})
