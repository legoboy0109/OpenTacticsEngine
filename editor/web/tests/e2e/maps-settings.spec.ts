import { expect, readDisk, saved, test } from './fixtures'
import { clickCell, diskGrid, openMap, pickSide, pickUnitType, tab, tool } from './mapkit'

const mapFile = (id: string, f: string) => readDisk(id, `maps/${f}.json`)

test('@smoke resize bigger with an anchor: terrain and units move together, new squares are the first terrain', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Grow')
  const before = diskGrid(id, 'ford')
  const unitsBefore = mapFile(id, 'ford').units
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await expect(page.getByTestId('map-size')).toHaveText('10 × 8 squares')
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill('14')
  await dlg.getByLabel('Height').fill('10')
  await dlg.getByRole('radio', { name: 'bottom right' }).click()
  await expect(dlg.getByText('Nothing on the map is lost.')).toBeVisible()
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await saved(page)

  const after = diskGrid(id, 'ford')
  expect(after).toHaveLength(10)
  expect(after.every((r) => r.length === 14)).toBe(true)
  // The old map sits at the bottom right (offset 4, 2); everything else is plain.
  for (let y = 0; y < 10; y++) for (let x = 0; x < 14; x++) {
    const inOld = x >= 4 && y >= 2
    expect(after[y][x], `${x},${y}`).toBe(inOld ? before[y - 2][x - 4] : 'plain')
  }
  expect(mapFile(id, 'ford').units).toEqual(unitsBefore.map((u: { x: number; y: number }) => ({ ...u, x: u.x + 4, y: u.y + 2 })))
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('map-size')).toHaveText('14 × 10 squares')
})

test('resize smaller lists what is lost, needs a confirmation, removes it, and undoes', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Shrink')
  const before = mapFile(id, 'ford')
  await openMap(page, id, 'ford')
  await tab(page, 'Map').click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill('6')
  await dlg.getByLabel('Height').fill('4')
  const lost = dlg.getByRole('alert')
  await expect(lost).toContainText('This will remove')
  await expect(lost).toContainText('Gorm (Soldier) at 9, 5')
  await expect(lost).toContainText('Vex (Archer) at 9, 4')
  await expect(lost).toContainText('Kell (Cavalier) at 2, 5') // y = 4 is outside a 4-high map
  await expect(lost).not.toContainText('Eirik')
  await dlg.getByRole('button', { name: 'Cancel' }).click() // cancel changes nothing
  await expect(page.getByTestId('map-size')).toHaveText('10 × 8 squares')

  await page.getByRole('button', { name: 'Resize…' }).click()
  await dlg.getByLabel('Width').fill('6')
  await dlg.getByLabel('Height').fill('4')
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await saved(page)
  const after = mapFile(id, 'ford')
  expect(after.grid).toMatchObject({ width: 6, height: 4 })
  expect(after.tiles).toEqual(before.tiles.slice(0, 4).map((r: string) => r.slice(0, 6)))
  expect(after.units.map((u: { name: string }) => u.name)).toEqual(['Eirik'])
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(mapFile(id, 'ford')).toEqual(before)
})

test('resize dialog enforces 1..256 and warns about big maps', async ({ page, game }) => {
  const id = await game.create('blank', 'Limits')
  await openMap(page, id, 'start')
  await tab(page, 'Map').click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill('300')
  await expect(dlg.getByText('At most 256.')).toBeVisible()
  await expect(dlg.getByRole('button', { name: 'Resize', exact: true })).toBeDisabled()
  await dlg.getByLabel('Width').fill('0')
  await expect(dlg.getByText('At least 1.')).toBeVisible()
  await dlg.getByLabel('Width').fill('70')
  await expect(dlg.getByText(/big map/)).toBeVisible()
  await dlg.getByLabel('Width').fill('1')
  await dlg.getByLabel('Height').fill('1')
  await dlg.getByRole('radio', { name: 'middle' }).click()
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await saved(page)
  expect(mapFile(id, 'start').tiles).toHaveLength(1)
  expect(mapFile(id, 'start').tiles[0]).toHaveLength(1)
  expect(await game.problems(id)).toEqual([])
  // A 1 x 1 map still draws and can be painted.
  await tab(page, 'Tool').click()
  await clickCell(page, 0, 0)
})

test('@smoke sides: add a third, set everything, give it a unit and a building, remove it with confirmation', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Sides')
  const before = mapFile(id, 'crossroads')
  await openMap(page, id, 'crossroads')
  await tab(page, 'Sides').click()
  const removeButtons = page.getByRole('button', { name: /^Remove / })
  await expect(removeButtons.first()).toBeDisabled() // two sides: the minimum
  await page.getByRole('button', { name: 'Add a side' }).click()
  const card = page.getByTestId('side-green')
  await expect(card).toBeVisible()
  await card.getByLabel('Name', { exact: true }).fill('Gold')
  await card.getByLabel('Played by').selectOption('human')
  await card.getByLabel('Team', { exact: true }).fill('allies')
  await card.getByLabel('Gold starting Funds').fill('1234')
  await saved(page)
  let sides = mapFile(id, 'crossroads').sides
  expect(sides).toHaveLength(3)
  expect(sides[2]).toMatchObject({ id: 'green', name: 'Gold', controller: 'human', team: 'allies', resources: { funds: 1234 } })
  expect(sides[2].color).toMatch(/^#[0-9a-f]{6}$/)
  // Its new colour reaches the map, and with no unit it is flagged.
  await expect(page.getByTestId('map-warnings')).toContainText('Gold has no units')

  await tab(page, 'Tool').click()
  await tool(page, 'Units').click()
  await pickSide(page, 'Gold').click()
  await pickUnitType(page, 'Infantry').click()
  await clickCell(page, 6, 7)
  await tool(page, 'Owners').click()
  await pickSide(page, 'Gold').click()
  await clickCell(page, 5, 6) // a city
  await saved(page)
  expect(mapFile(id, 'crossroads').units.at(-1)).toEqual({ type: 'infantry', side: 'green', x: 6, y: 7 })
  expect(mapFile(id, 'crossroads').owners.at(-1)).toEqual({ x: 5, y: 6, side: 'green' })
  expect(await game.problems(id)).toEqual([])

  await tab(page, 'Sides').click()
  await page.getByRole('button', { name: 'Remove Gold' }).click()
  const dlg = page.getByRole('dialog', { name: /Remove the side/ })
  await expect(dlg).toContainText('1 unit and 1 owned square')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  expect(mapFile(id, 'crossroads').sides).toHaveLength(3)
  await page.getByRole('button', { name: 'Remove Gold' }).click()
  await page.getByRole('dialog', { name: /Remove the side/ }).getByRole('button', { name: 'Remove', exact: true }).click()
  await saved(page)
  expect(mapFile(id, 'crossroads')).toEqual(before)
  expect(await game.problems(id)).toEqual([])
})

test('a map file whose rows do not match shows a clear repair message and a Repair button', async ({ page, game, request }) => {
  const id = await game.create('blank', 'Broken')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const m = doc.maps.start
  m.tiles = ['....?...', '...##...', '......', '........', '...##...', '........', '........'] // one short row, one unknown symbol, one extra row
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { start: m } } })
  expect(put.ok()).toBeTruthy()
  await page.goto(`/p/${id}/maps/start`)
  await expect(page.getByText('This map’s squares are stored inconsistently.')).toBeVisible()
  await expect(page.getByText(/should have 6 rows but has 7/)).toBeVisible()
  await expect(page.getByText(/Row 3 is not 8 squares wide/)).toBeVisible()
  await expect(page.getByTestId('map-canvas')).toHaveCount(0)
  await page.getByRole('button', { name: 'Repair this map' }).click()
  await expect(page.getByTestId('map-canvas')).toBeVisible()
  await saved(page)
  const fixed = mapFile(id, 'start')
  expect(fixed.tiles).toHaveLength(6)
  expect(fixed.tiles.every((r: string) => r.length === 8)).toBe(true)
  expect(fixed.tiles[0]).toBe('........') // the unknown "?" became the first terrain
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Undo' }).click()
  await expect(page.getByText('This map’s squares are stored inconsistently.')).toBeVisible()
})
