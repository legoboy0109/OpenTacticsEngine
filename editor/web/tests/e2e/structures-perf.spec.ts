// Painting must stay fast with structures on the map. Same drag as art-perf.spec.ts / maps-list.spec.ts: 200 squares on a
// 64 x 64 map, with ~85 structures (1 x 1 and 2 x 2) and a few units placed; once with placeholder blocks, once with pictures.
import type { APIRequestContext } from '@playwright/test'
import { bareGame, seedArt, sheetPng, tilesGraphics } from './artkit'
import { expect, readDisk, saved, test } from './fixtures'
import { openMap, pickTerrain } from './mapkit'

const KINDS = [
  { id: 'tower', name: 'Tower', color: '#8a7a6a', glyph: 'T' },
  { id: 'castle', name: 'Castle', color: '#9a9a9a', glyph: 'C', width: 2, height: 2 },
]

async function bigMap(request: APIRequestContext, id: string): Promise<number> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const m = doc.maps.ford
  const row = (y: number) => (y % 7 === 3 ? '~'.repeat(64) : Array.from({ length: 64 }, (_, x) => ((x + y) % 11 === 0 ? 'f' : '.')).join(''))
  m.grid.width = 64; m.grid.height = 64
  m.tiles = Array.from({ length: 64 }, (_, y) => row(y))
  m.units = m.units.map((u: any, i: number) => ({ ...u, x: 5 + i * 3, y: 20 + i }))
  m.structures = []
  for (let y = 16; y < 62; y += 6) {
    if (y >= 19 && y <= 24) continue // the units stand here
    for (let x = 2; x < 61; x += 5) m.structures.push((x + y) % 2 === 0 ? { type: 'castle', x, y } : { type: 'tower', x, y, side: x % 2 ? 'blue' : 'red' })
  }
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, structures: KINDS, maps: doc.maps } })
  expect(put.ok(), await put.text()).toBeTruthy()
  return m.structures.length
}

async function stroke(page: import('@playwright/test').Page): Promise<{ ms: number; n: number }> {
  return page.evaluate(async () => {
    const cv = document.querySelector<HTMLCanvasElement>('[data-testid="map-canvas"]')!
    const r = cv.getBoundingClientRect()
    const [scale, vx, vy] = cv.dataset.view!.split(',').map(Number)
    const at = (x: number, y: number) => ({ clientX: r.left + vx + (x + 0.5) * scale, clientY: r.top + vy + (y + 0.5) * scale, pointerId: 1, pointerType: 'mouse', bubbles: true })
    const frame = () => new Promise((res) => requestAnimationFrame(() => requestAnimationFrame(res)))
    const t0 = performance.now()
    cv.dispatchEvent(new PointerEvent('pointerdown', { ...at(5, 10), button: 0, buttons: 1 }))
    let n = 1
    for (let rowN = 0; rowN < 4; rowN++) {
      for (let i = 0; i < 50; i++) {
        const x = rowN % 2 === 0 ? 5 + i : 54 - i
        if (rowN === 0 && i === 0) continue
        cv.dispatchEvent(new PointerEvent('pointermove', { ...at(x, 10 + rowN), buttons: 1 }))
        n++
      }
    }
    cv.dispatchEvent(new PointerEvent('pointerup', { ...at(5, 13), button: 0 }))
    await frame()
    return { ms: performance.now() - t0, n }
  })
}

test('a 64 x 64 map with ~85 structures (placeholder blocks) paints a 200-square stroke fast (timing is reported)', async ({ page, request, game }, info) => {
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  const id = await bareGame(request, game, 'strategy-rpg', 'Big Structures')
  const count = await bigMap(request, id)
  expect(count).toBeGreaterThan(80)
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'Mountain').click()
  await saved(page)
  const ms = await stroke(page)
  expect(ms.n).toBe(200)
  console.log(`STROKE_TIMING with ${count} structures, no art, 64x64, ${ms.n} squares: ${ms.ms.toFixed(0)} ms`)
  info.annotations.push({ type: 'stroke-ms-structures', description: `${ms.ms.toFixed(0)} ms` })
  expect(ms.ms).toBeLessThan(1500)
  await saved(page)
  const file = readDisk(id, 'maps/ford.json')
  expect(file.tiles[10].slice(5, 55).split('').every((c: string) => file.legend[c] === 'mountain')).toBe(true)
  expect(file.structures).toHaveLength(count)
  expect(errors).toEqual([])
})

test('the same with pictures on terrain, units and structures', async ({ page, request, game }, info) => {
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  const id = await game.create('strategy-rpg', 'Big Structure Art')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({
    terrain: {
      plain: { sheet: 'tiles', variants: [[0, 0], [1, 0], [2, 0]] },
      mountain: { sheet: 'tiles', join: [0, 0] },
      water: { sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 200 },
      forest: { sheet: 'tiles', cell: [2, 1] },
    },
    units: { lord: { sheet: 'tiles', cell: [3, 3] }, cavalier: { sheet: 'tiles', cell: [3, 2] }, archer: { sheet: 'tiles', cell: [3, 1] }, soldier: { sheet: 'tiles', cell: [3, 0] } },
    structures: { tower: { sheet: 'tiles', cell: [1, 3], sides: { red: { sheet: 'tiles', cell: [2, 3] } } }, castle: { sheet: 'tiles', frames: [[0, 3], [1, 3]], frameMs: 300 } },
  }))
  const count = await bigMap(request, id)
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'Mountain').click()
  await saved(page)
  const ms = await stroke(page)
  expect(ms.n).toBe(200)
  console.log(`STROKE_TIMING with ${count} structures, with art, 64x64, ${ms.n} squares: ${ms.ms.toFixed(0)} ms`)
  info.annotations.push({ type: 'stroke-ms-structures-art', description: `${ms.ms.toFixed(0)} ms` })
  expect(ms.ms).toBeLessThan(1500)
  await saved(page)
  expect(errors).toEqual([])
})
