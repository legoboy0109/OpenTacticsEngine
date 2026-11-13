import type { Page } from '@playwright/test'
import { expect, readDisk, saved, test } from './fixtures'
import { cellPoint, clickCell, dragThrough, openMap, pickSide, pickTerrain, pickUnitType, tool } from './mapkit'

const notice = (page: Page) => page.getByTestId('map-notice')
const diskUnits = (id: string, file: string) => readDisk(id, `maps/${file}.json`).units

test('@smoke place units for both sides, move one by dragging, leader, name, level, stat override', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Units')
  const original = diskUnits(id, 'ford')
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()

  await pickSide(page, 'Blue').click()
  await pickUnitType(page, 'Archer').click()
  await clickCell(page, 3, 1)
  await expect(page.getByTestId('unit-props')).toContainText('at 4, 2')
  await pickSide(page, 'Red').click()
  await pickUnitType(page, 'Soldier').click()
  await clickCell(page, 6, 3)

  // The new red soldier is selected: give it a name, a level, make it a leader, own Strength.
  const props = page.getByTestId('unit-props')
  await props.getByLabel('Name', { exact: true }).fill('Zed')
  await props.getByLabel('Level').fill('3')
  await props.getByRole('checkbox', { name: /Leader/ }).check()
  await props.getByLabel('Strength for this unit').fill('9')
  await saved(page)
  // Drag the blue archer one square down (one undo step, a real move).
  const from = await cellPoint(page, 3, 1)
  const to = await cellPoint(page, 3, 2)
  await page.mouse.move(from.x, from.y)
  await page.mouse.down()
  await page.mouse.move(to.x, to.y, { steps: 4 })
  await page.mouse.up()
  await saved(page)

  expect(diskUnits(id, 'ford')).toEqual([
    ...original,
    { type: 'archer', side: 'blue', x: 3, y: 2 },
    { type: 'soldier', side: 'red', x: 6, y: 3, name: 'Zed', level: 3, leader: true, stats: { str: 9 } },
  ])
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByRole('button', { name: 'No problems' })).toBeVisible()

  // Typing the unit type's own value stores nothing; clearing removes the key too.
  await clickCell(page, 6, 3)
  await props.getByLabel('Strength for this unit').fill('5') // a soldier's Strength is 5
  await saved(page)
  expect(diskUnits(id, 'ford').at(-1)).not.toHaveProperty('stats')
  await props.getByLabel('Strength for this unit').fill('11')
  await props.getByLabel('Strength for this unit').fill('')
  await saved(page)
  expect(diskUnits(id, 'ford').at(-1)).not.toHaveProperty('stats')

  // Undo the whole sequence step by step: the drag is one step.
  await page.keyboard.press('Control+z')
  await saved(page)
})

test('a unit cannot be placed or dragged onto terrain its movement cannot enter', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Impassable')
  const original = diskUnits(id, 'ford')
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await pickSide(page, 'Blue').click()
  await pickUnitType(page, 'Lord').click()
  await clickCell(page, 5, 0) // river
  await expect(notice(page)).toContainText('cannot stand on River')
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()

  await dragThrough(page, [[1, 4], [3, 4], [5, 5]]) // drag Kell onto the river
  await expect(notice(page)).toContainText('cannot stand on River')
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()

  await clickCell(page, 1, 3) // an occupied square selects the unit there instead of replacing it
  await expect(page.getByTestId('unit-props').getByLabel('Name', { exact: true })).toHaveValue('Eirik')
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()
  await page.waitForTimeout(400)
  expect(diskUnits(id, 'ford')).toEqual(original)
})

test('painting impassable terrain under a unit is flagged, not deleted; the helper reports it too', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Flag')
  await openMap(page, id, 'ford')
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  await pickTerrain(page, 'River').click()
  await clickCell(page, 1, 3) // Eirik (a lord on foot) stands here
  await expect(page.getByTestId('map-warnings')).toContainText('Lord at 2,4 stands on River')
  await saved(page)
  expect(diskUnits(id, 'ford')).toHaveLength(4) // not deleted
  await expect(page.getByRole('button', { name: '1 problem' })).toBeVisible()
  expect((await game.problems(id)).some((p) => /impassable/.test(p.message))).toBe(true)

  // Clicking the warning finds the square and shows what is there.
  await page.getByRole('button', { name: /Lord at 2,4 stands on River/ }).click()
  await expect(tool(page, 'Select')).toHaveAttribute('aria-pressed', 'true')
  await expect(page.getByTestId('unit-props')).toContainText('at 2, 4')

  await tool(page, 'Terrain').click()
  await pickTerrain(page, 'Plain').click()
  await clickCell(page, 1, 3)
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  await saved(page)
  await expect(page.getByRole('button', { name: 'No problems' })).toBeVisible()
})

test('Delete key and button remove the selected unit', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Remove')
  await openMap(page, id, 'ford')
  await tool(page, 'Select').click()
  await clickCell(page, 1, 4) // Kell
  await page.keyboard.press('Delete')
  await saved(page)
  expect(diskUnits(id, 'ford').map((u: { name: string }) => u.name)).toEqual(['Eirik', 'Gorm', 'Vex'])
  await clickCell(page, 8, 3) // Vex
  await page.getByRole('button', { name: 'Remove unit' }).click()
  await saved(page)
  expect(diskUnits(id, 'ford').map((u: { name: string }) => u.name)).toEqual(['Eirik', 'Gorm'])
  await page.keyboard.press('Control+z')
  await saved(page)
  expect(diskUnits(id, 'ford')).toHaveLength(3)
})

test('owners: set, drag-set, clear, and the soft warning on terrain where ownership means nothing', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Owners')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Owners').click()
  await expect(page.getByText(/Ownership matters for terrain that can be captured/)).toBeVisible()
  await pickSide(page, 'Red').click()
  await dragThrough(page, [[5, 0], [6, 0]]) // two cities
  await saved(page)
  const owners = () => readDisk(id, 'maps/crossroads.json').owners as Array<{ x: number; y: number; side: string }>
  expect(owners()).toEqual(expect.arrayContaining([{ x: 5, y: 0, side: 'red' }, { x: 6, y: 0, side: 'red' }]))
  expect(owners()).toHaveLength(8)
  await expect(notice(page)).toHaveText('')
  await clickCell(page, 2, 0) // forest: no use for an owner
  await expect(notice(page)).toContainText('has no use for an owner')
  await saved(page)
  expect(owners()).toHaveLength(9) // set anyway
  await pickSide(page, 'No owner').click()
  await dragThrough(page, [[5, 0], [6, 0], [2, 0]])
  await saved(page)
  expect(owners()).toHaveLength(6)
  expect(owners().some((o) => o.y === 0)).toBe(false)
  expect(await game.problems(id)).toEqual([])
})

test('warnings: no leader / no headquarters appear and clear', async ({ page, game }) => {
  const rpg = await game.create('strategy-rpg', 'Leader warn')
  await openMap(page, rpg, 'ford')
  await tool(page, 'Select').click()
  await clickCell(page, 1, 3) // Eirik, the blue leader (the game is lost when the leader falls)
  await page.getByRole('checkbox', { name: /Leader/ }).uncheck()
  await expect(page.getByTestId('map-warnings')).toContainText('has no leader, but')
  await page.getByRole('checkbox', { name: /Leader/ }).check()
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)

  const eco = await game.create('economy-tactics', 'HQ warn')
  await openMap(page, eco, 'crossroads')
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  await tool(page, 'Owners').click()
  await pickSide(page, 'No owner').click()
  await clickCell(page, 1, 2) // blue's headquarters
  await expect(page.getByTestId('map-warnings')).toContainText('owns no headquarters, but')
  await pickSide(page, 'Blue').click()
  await clickCell(page, 1, 2)
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
})

test('the level box shows only when Experience is on; the chosen tool is remembered across a reload', async ({ page, game }) => {
  const eco = await game.create('economy-tactics', 'NoLevels')
  await openMap(page, eco, 'crossroads')
  await tool(page, 'Select').click()
  await clickCell(page, 2, 2) // an infantry unit
  await expect(page.getByTestId('unit-props')).toBeVisible()
  await expect(page.getByTestId('unit-props').getByLabel('Level')).toHaveCount(0)
  await expect(page.getByTestId('unit-props').getByLabel('HP for this unit')).toBeVisible()
  await page.reload()
  await expect(tool(page, 'Select')).toHaveAttribute('aria-pressed', 'true')

  const rpg = await game.create('strategy-rpg', 'Levels')
  await openMap(page, rpg, 'ford')
  await tool(page, 'Select').click()
  await clickCell(page, 1, 3)
  await expect(page.getByTestId('unit-props').getByLabel('Level')).toBeVisible()
})
