import { expect, readDisk, saved, test } from './fixtures'
import { clickCell, diskGrid, dragThrough, expectedFill, openMap, pickTerrain, row, status, TEMPLATE_MAPS, tool, cellPoint, type TemplateName } from './mapkit'

// A terrain other than the first one, per template, and its display name.
const PAINT: Record<TemplateName, { id: string; name: string }> = {
  'strategy-rpg': { id: 'mountain', name: 'Mountain' },
  'economy-tactics': { id: 'road', name: 'Road' },
  blank: { id: 'wall', name: 'Wall' },
}

for (const template of Object.keys(TEMPLATE_MAPS) as TemplateName[]) {
  const mapId = TEMPLATE_MAPS[template]
  const terrain = PAINT[template]

  test(`@smoke ${template}: a drag paints a row, fill fills, each is one undo step`, async ({ page, game }) => {
    const id = await game.create(template, `Paint ${template}`)
    const before = diskGrid(id, mapId)
    const w = before[0].length
    await openMap(page, id, mapId)
    await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()

    // 1. one drag across row 0
    await pickTerrain(page, terrain.name).click()
    await dragThrough(page, row(0, 0, w - 1))
    await saved(page)
    const afterDrag = before.map((r) => r.slice())
    afterDrag[0] = afterDrag[0].map(() => terrain.id)
    expect(diskGrid(id, mapId)).toEqual(afterDrag)
    // The legend is rebuilt but stays valid and keeps the file's own characters.
    const file = readDisk(id, `maps/${mapId}.json`)
    expect(file.tiles).toHaveLength(before.length)
    expect(file.tiles.every((r: string) => r.length === w)).toBe(true)
    expect(Object.values(file.legend)).toContain(terrain.id)

    // 2. flood fill from the bottom-left square with the first terrain (changes only that connected area)
    await page.keyboard.press('f')
    await pickTerrain(page, terrain.name).click() // keep the same terrain; fill toggle stays on
    const h = before.length
    await clickCell(page, 0, h - 1)
    await saved(page)
    const afterFill = expectedFill(afterDrag, 0, h - 1, terrain.id)
    expect(diskGrid(id, mapId)).toEqual(afterFill)

    // 3. undo: the fill goes, the whole drag stays; undo again: the whole drag goes (one step per stroke)
    await page.getByRole('button', { name: 'Undo' }).click()
    await saved(page)
    expect(diskGrid(id, mapId)).toEqual(afterDrag)
    await page.getByRole('button', { name: 'Undo' }).click()
    await saved(page)
    expect(diskGrid(id, mapId)).toEqual(before)
    await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()
    // 4. redo brings the drag back in one step
    await page.getByRole('button', { name: 'Redo' }).click()
    await saved(page)
    expect(diskGrid(id, mapId)).toEqual(afterDrag)
    expect(await game.problems(id)).toEqual([])
  })
}

test('two separate strokes are two undo steps, even when they come quickly', async ({ page, game }) => {
  const id = await game.create('blank', 'Strokes')
  await openMap(page, id, 'start')
  await pickTerrain(page, 'Wall').click()
  await dragThrough(page, row(0, 0, 3))
  await dragThrough(page, row(5, 0, 3))
  await saved(page)
  const both = diskGrid(id, 'start')
  expect(both[0].slice(0, 4)).toEqual(Array(4).fill('wall'))
  expect(both[5].slice(0, 4)).toEqual(Array(4).fill('wall'))
  await page.keyboard.press('Control+z')
  await saved(page)
  const one = diskGrid(id, 'start')
  expect(one[0].slice(0, 4)).toEqual(Array(4).fill('wall'))
  expect(one[5].slice(0, 4)).toEqual(Array(4).fill('plain'))
})

test('a stroke that pauses mid-drag is still one undo step', async ({ page, game }) => {
  const id = await game.create('blank', 'Pause')
  await openMap(page, id, 'start')
  await pickTerrain(page, 'Wall').click()
  const a = await cellPoint(page, 0, 0)
  const b = await cellPoint(page, 1, 0)
  const c = await cellPoint(page, 2, 0)
  await page.mouse.move(a.x, a.y)
  await page.mouse.down()
  await page.mouse.move(b.x, b.y, { steps: 2 })
  await page.waitForTimeout(1800) // longer than the typing-coalescing window
  await page.mouse.move(c.x, c.y, { steps: 2 })
  await page.mouse.up()
  await saved(page)
  expect(diskGrid(id, 'start')[0].slice(0, 3)).toEqual(['wall', 'wall', 'wall'])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(diskGrid(id, 'start')[0].slice(0, 3)).toEqual(['plain', 'plain', 'plain'])
})

test('@smoke alt-click picks the terrain under the pointer; the status line names the square', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Pick')
  await openMap(page, id, 'ford')
  await expect(pickTerrain(page, 'Plain')).toHaveAttribute('aria-checked', 'true')
  await clickCell(page, 5, 0, { modifiers: ['Alt'] }) // water in the template
  await expect(pickTerrain(page, 'River')).toHaveAttribute('aria-checked', 'true')
  const p = await cellPoint(page, 5, 0)
  await page.mouse.move(p.x, p.y)
  await expect(status(page)).toContainText('6, 1')
  await expect(status(page)).toContainText('River')
  expect(await page.getByRole('button', { name: 'Undo' }).isDisabled()).toBe(true) // picking edits nothing
})

test('a game with a single terrain still opens the editor and explains', async ({ page, game, request }) => {
  const id = await game.create('blank', 'One terrain')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  // Remove the wall terrain through the UI's own rule: replace via API (valid single-terrain game).
  doc.terrain = [doc.terrain[0]]
  doc.maps.start.legend = { '.': 'plain' }
  doc.maps.start.tiles = doc.maps.start.tiles.map((r: string) => r.replace(/#/g, '.'))
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, terrain: doc.terrain, maps: doc.maps } })
  expect(put.ok()).toBeTruthy()
  await openMap(page, id, 'start')
  await expect(page.getByText('only one kind of terrain')).toBeVisible()
  await pickTerrain(page, 'Plain').click()
  await dragThrough(page, row(1, 0, 3))
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled() // painting plain on plain changes nothing
})

test('tool shortcuts and visible hints', async ({ page, game }) => {
  const id = await game.create('blank', 'Keys')
  await openMap(page, id, 'start')
  await expect(page.getByText(/T U O H D S pick a tool/)).toBeVisible()
  for (const [key, name] of [['u', 'Units'], ['o', 'Structures'], ['h', 'Heights'], ['s', 'Select'], ['t', 'Terrain']] as const) {
    await page.keyboard.press(key)
    await expect(tool(page, name)).toHaveAttribute('aria-pressed', 'true')
  }
  await page.keyboard.press('f')
  await expect(page.getByRole('switch', { name: /Fill/ })).toBeChecked()
  await page.keyboard.press('f')
  await expect(page.getByRole('switch', { name: /Fill/ })).not.toBeChecked()
})

test('keyboard only: the canvas has a cursor; Enter paints', async ({ page, game }) => {
  const id = await game.create('blank', 'Cursor')
  await openMap(page, id, 'start')
  await pickTerrain(page, 'Wall').click()
  await page.getByTestId('map-canvas').focus()
  await page.keyboard.press('ArrowRight')
  await page.keyboard.press('ArrowRight')
  await page.keyboard.press('ArrowDown')
  await expect(status(page)).toContainText('3, 2')
  await page.keyboard.press('Enter')
  await saved(page)
  expect(diskGrid(id, 'start')[1][2]).toBe('wall')
  await page.keyboard.press('Delete') // nothing selected there: harmless
})

test('zoom buttons and Fit change the view; wheel zooms', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Zoom')
  await openMap(page, id, 'ford')
  const scale = async () => Number((await page.getByTestId('map-canvas').getAttribute('data-view'))!.split(',')[0])
  const s0 = await scale()
  await page.getByRole('button', { name: 'Zoom in' }).click()
  await expect.poll(scale).toBeGreaterThan(s0)
  await page.getByRole('button', { name: 'Fit' }).click()
  await expect.poll(scale).toBeCloseTo(s0, 1)
  const p = await cellPoint(page, 3, 3)
  await page.mouse.move(p.x, p.y)
  await page.mouse.wheel(0, -300)
  await expect.poll(scale).toBeGreaterThan(s0)
})

test('panning: middle-button drag and Space + drag move the map without painting', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Pan')
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'Mountain').click()
  const origin = async () => (await page.getByTestId('map-canvas').getAttribute('data-view'))!.split(',').slice(1).map(Number)
  const [x0, y0] = await origin()
  const a = await cellPoint(page, 4, 3)
  await page.mouse.move(a.x, a.y)
  await page.mouse.down({ button: 'middle' })
  await page.mouse.move(a.x + 40, a.y + 25, { steps: 4 })
  await page.mouse.up({ button: 'middle' })
  await expect.poll(async () => (await origin())[0]).toBeGreaterThan(x0 + 30)
  const [x1, y1] = await origin()
  expect(y1).toBeGreaterThan(y0 + 15)
  await page.keyboard.down(' ')
  await page.mouse.move(a.x + 40, a.y + 25)
  await page.mouse.down()
  await page.mouse.move(a.x, a.y, { steps: 4 })
  await page.mouse.up()
  await page.keyboard.up(' ')
  await expect.poll(async () => (await origin())[0]).toBeLessThan(x1 - 30)
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled() // neither drag painted anything
})
