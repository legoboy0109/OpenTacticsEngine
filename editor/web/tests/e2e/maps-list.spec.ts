import { existsSync } from 'node:fs'
import { diskPath, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { canvas, cellPoint, openMap, pickTerrain, tab } from './mapkit'

test('@smoke new map appears in the list and game.json, its file is created, and it opens clean', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'NewMap')
  await page.goto(editorUrl(id, 'maps'))
  await page.getByRole('button', { name: 'Show map list' }).click()
  await page.getByRole('button', { name: 'New map' }).click()
  await page.getByLabel('Name of the map').fill('Second Fight')
  await page.keyboard.press('Enter')
  await expect(page).toHaveURL(/maps\/second_fight/)
  await expect(page.getByRole('button', { name: /^Second Fight/ })).toContainText('12 × 8 squares')
  await expect(canvas(page)).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'game.json').maps).toContainEqual({ id: 'second_fight', name: 'Second Fight', file: 'maps/second_fight.json' })
  expect(existsSync(diskPath(id, 'maps/second_fight.json'))).toBe(true)
  const m = readDisk(id, 'maps/second_fight.json')
  expect(m.grid).toMatchObject({ width: 12, height: 8 })
  expect(m.sides.map((s: { name: string; color: string; controller: string }) => [s.name, s.color, s.controller])).toEqual([['Blue', '#3d85c6', 'human'], ['Red', '#cc0000', 'ai']])
  expect(m.units).toEqual([])
  expect(m.tiles).toHaveLength(8)
  expect(await game.problems(id)).toEqual([])
  // Two sides, no units: friendly warnings, not blockers.
  await expect(page.getByTestId('map-warnings')).toContainText('Blue has no units')
})

test('rename, duplicate and delete a map; the files follow', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'ListOps')
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await page.getByLabel('Name', { exact: true }).fill('The Crossing')
  await saved(page)
  expect(readDisk(id, 'game.json').maps[0].name).toBe('The Crossing')
  expect(readDisk(id, 'maps/ford.json').name).toBe('The Crossing')
  await expect(page.getByRole('heading', { name: 'The Crossing' })).toBeVisible()

  await page.getByRole('button', { name: 'Duplicate' }).click()
  await expect(page).toHaveURL(/maps\/the_crossing_copy/)
  await saved(page)
  expect(existsSync(diskPath(id, 'maps/the_crossing_copy.json'))).toBe(true)
  expect(readDisk(id, 'maps/the_crossing_copy.json').tiles).toEqual(readDisk(id, 'maps/ford.json').tiles)

  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  expect(existsSync(diskPath(id, 'maps/the_crossing_copy.json'))).toBe(false)
  expect(readDisk(id, 'game.json').maps.map((m: { id: string }) => m.id)).toEqual(['ford'])
  expect(await game.problems(id)).toEqual([])
  // The last map cannot be deleted.
  await page.goto(editorUrl(id, 'maps', 'ford'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  await expect(page.getByRole('dialog')).toContainText('at least one map')
})

test('deleting a unit type or a terrain used on a map cleans the map (grid model stays consistent)', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Cleanup')
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  await page.getByRole('dialog').getByRole('button', { name: 'Delete' }).click()
  await saved(page)
  const m = readDisk(id, 'maps/ford.json')
  expect(Object.values(m.legend)).not.toContain('forest')
  expect(new Set(Object.values(m.legend)).size).toBe(Object.keys(m.legend).length) // no two symbols for one terrain
  expect(await game.problems(id)).toEqual([])
  // The editor opens that map and painting still works afterwards.
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'Mountain').click()
  const p = await cellPoint(page, 0, 0)
  await page.mouse.click(p.x, p.y)
  await saved(page)
  expect(readDisk(id, 'maps/ford.json').legend[readDisk(id, 'maps/ford.json').tiles[0][0]]).toBe('mountain')
  expect(await game.problems(id)).toEqual([])
})

test('a 64 x 64 map paints a 200-square stroke without errors (timing is reported)', async ({ page, game }, info) => {
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()) })
  const id = await game.create('strategy-rpg', 'Big')
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill('64')
  await dlg.getByLabel('Height').fill('64')
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await expect(page.getByTestId('map-size')).toHaveText('64 × 64 squares')
  await tab(page, 'Tool').click()
  await pickTerrain(page, 'Mountain').click()
  await saved(page)

  // Drive one drag (200 squares: four rows of 50) with synthetic pointer events inside the page and
  // time it until the next two painted frames, so only the app's own work is measured.
  const ms = await page.evaluate(async () => {
    const cv = document.querySelector<HTMLCanvasElement>('[data-testid="map-canvas"]')!
    const r = cv.getBoundingClientRect()
    const [scale, vx, vy] = cv.dataset.view!.split(',').map(Number)
    const at = (x: number, y: number) => ({ clientX: r.left + vx + (x + 0.5) * scale, clientY: r.top + vy + (y + 0.5) * scale, pointerId: 1, pointerType: 'mouse', bubbles: true })
    const frame = () => new Promise((res) => requestAnimationFrame(() => requestAnimationFrame(res)))
    const t0 = performance.now()
    cv.dispatchEvent(new PointerEvent('pointerdown', { ...at(5, 10), button: 0, buttons: 1 }))
    let n = 1
    for (let row = 0; row < 4; row++) {
      for (let i = 0; i < 50; i++) {
        const x = row % 2 === 0 ? 5 + i : 54 - i
        if (row === 0 && i === 0) continue
        cv.dispatchEvent(new PointerEvent('pointermove', { ...at(x, 10 + row), buttons: 1 }))
        n++
      }
    }
    cv.dispatchEvent(new PointerEvent('pointerup', { ...at(5, 13), button: 0 }))
    await frame()
    return { ms: performance.now() - t0, n }
  })
  expect(ms.n).toBe(200)
  info.annotations.push({ type: 'stroke-ms', description: `${ms.ms.toFixed(0)} ms for ${ms.n} squares on 64x64` })
  console.log(`STROKE_TIMING 64x64, ${ms.n} squares: ${ms.ms.toFixed(0)} ms`)
  expect(ms.ms).toBeLessThan(3000)
  await saved(page)
  const file = readDisk(id, 'maps/ford.json')
  expect(file.tiles).toHaveLength(64)
  const mountains = file.tiles.flatMap((r: string) => [...r]).filter((c: string) => file.legend[c] === 'mountain').length
  expect(mountains).toBeGreaterThanOrEqual(200)
  // The whole 200-square drag is a single undo step.
  const painted = file.tiles[10].slice(5, 55)
  expect([...painted].every((c: string) => file.legend[c] === 'mountain')).toBe(true)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  const undone = readDisk(id, 'maps/ford.json')
  expect(undone.tiles[10].slice(5, 55).split('').some((c: string) => undone.legend[c] === 'mountain')).toBe(false)
  expect(errors).toEqual([])
})
