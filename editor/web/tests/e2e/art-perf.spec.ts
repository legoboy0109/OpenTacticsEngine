// Painting must stay fast when the map is drawn with art (random mix, joining pictures, an animation, unit sprites).
// Same drag as the placeholder test in maps-list.spec.ts (200 squares on a 64 x 64 map); the time is printed.
import { seedArt, sheetPng, tilesGraphics } from './artkit'
import { expect, readDisk, saved, test } from './fixtures'
import { openMap, pickTerrain } from './mapkit'

test('a 64 x 64 map drawn with art paints a 200-square stroke as fast as without (timing is reported)', async ({ page, request, game }, info) => {
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()) })
  const id = await game.create('strategy-rpg', 'Big Art')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({
    terrain: {
      plain: { sheet: 'tiles', variants: [[0, 0], [1, 0], [2, 0]] },
      mountain: { sheet: 'tiles', join: [0, 0] },
      water: { sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 200 },
      forest: { sheet: 'tiles', cell: [2, 1] },
    },
    units: { lord: { sheet: 'tiles', cell: [3, 3] }, cavalier: { sheet: 'tiles', cell: [3, 2] }, archer: { sheet: 'tiles', cell: [3, 1] }, soldier: { sheet: 'tiles', cell: [3, 0] } },
  }))
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const m = doc.maps.ford
  const row = (y: number) => (y % 7 === 3 ? '~'.repeat(64) : Array.from({ length: 64 }, (_, x) => ((x + y) % 11 === 0 ? 'f' : '.')).join(''))
  m.grid.width = 64; m.grid.height = 64
  m.tiles = Array.from({ length: 64 }, (_, y) => row(y))
  m.units = m.units.map((u: any, i: number) => ({ ...u, x: 5 + i * 3, y: 20 + i }))
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })).ok()).toBeTruthy()
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'Mountain').click()
  await saved(page)

  const ms = await page.evaluate(async () => {
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
  expect(ms.n).toBe(200)
  console.log(`STROKE_TIMING with art, 64x64, ${ms.n} squares: ${ms.ms.toFixed(0)} ms`)
  info.annotations.push({ type: 'stroke-ms-art', description: `${ms.ms.toFixed(0)} ms` })
  expect(ms.ms).toBeLessThan(1500)
  await saved(page)
  const file = readDisk(id, 'maps/ford.json')
  expect(file.tiles[10].slice(5, 55).split('').every((c: string) => file.legend[c] === 'mountain')).toBe(true)
  expect(errors).toEqual([])
})
