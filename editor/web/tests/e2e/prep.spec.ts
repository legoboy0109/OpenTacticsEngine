// The preparation screen in the map editor (docs/EVENTS.md "Preparation", docs/EDITOR_UI.md): switch it on for a map, paint
// deployment slots with the Slots tool, set the limit, a required character (with a slot of its own), a forbidden one and the
// shop, and check what is written. The proof that matters: the exact JSON in the map file, and that the real game loader
// accepts it (/problems is empty). Desktop and a 360 px phone.
import { cpSync, rmSync, readdirSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { canvas, clickCell, openMap, tab, tool } from './mapkit'
import { shot } from './storykit'

const repoFixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-prep')

/** A new game (so the test has its own folder) whose files are replaced by the engine's preparation fixture: a three-chapter
 *  campaign, a meadow with four named units and no preparation, an orchard and a hilltop that have one. */
async function prepGame(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Muster')
  for (const dir of ['maps', 'data']) for (const f of readdirSync(diskPath(id, dir))) rmSync(diskPath(id, `${dir}/${f}`), { recursive: true, force: true })
  cpSync(repoFixture, diskPath(id, '.'), { recursive: true })
  rmSync(diskPath(id, 'README.md'), { force: true })
  return id
}

const slots = (page: Page) => canvas(page).getAttribute('data-slots')
const prepOn = (page: Page) => page.getByRole('switch', { name: /^This map has a preparation screen/ })

async function run(page: Page, id: string): Promise<void> {
  // --- The meadow has no preparation: the Slots tool says so, the tab has the switch.
  await openMap(page, id, 'meadow')
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)
  await tool(page, 'Slots').click()
  await expect(page.getByTestId('slots-tool')).toContainText('This map has no preparation screen yet')
  await tab(page, 'Preparation').click()
  await expect(prepOn(page)).not.toBeChecked()
  await expect(canvas(page)).toHaveAttribute('data-slots', '')

  // --- Switch on: the squares of the four named units become slots 1 to 4 (the map's own placement stops mattering).
  await prepOn(page).check()
  await saved(page)
  await expect(page.getByTestId('prep-slot-count')).toContainText('4 deployment slots')
  expect(await slots(page)).toBe('0,1;0,2;0,3;1,2')
  expect(readDisk(id, 'maps/meadow.json').prep).toEqual({ slots: [{ x: 0, y: 1 }, { x: 0, y: 2 }, { x: 0, y: 3 }, { x: 1, y: 2 }], limit: 4 })

  // --- The Slots tool: a click on an empty square adds the next slot, a click on a slot takes it away.
  await tool(page, 'Slots').click()
  await clickCell(page, 4, 1)
  await expect(canvas(page)).toHaveAttribute('data-slots', '0,1;0,2;0,3;1,2;4,1')
  await clickCell(page, 4, 1)
  await expect(canvas(page)).toHaveAttribute('data-slots', '0,1;0,2;0,3;1,2')
  await clickCell(page, 4, 2)  // a slot on the pond's neighbour: fine
  await saved(page)
  expect(readDisk(id, 'maps/meadow.json').prep.slots).toHaveLength(5)
  await clickCell(page, 7, 1)  // a raider stands there and stays where it is: refused, with the reason
  await expect(page.getByTestId('map-notice')).toContainText('stays where it is placed')
  await expect(canvas(page)).toHaveAttribute('data-slots', '0,1;0,2;0,3;1,2;4,2')

  // --- The Preparation tab: limit, required (with a slot), forbidden, the shop.
  await tab(page, 'Preparation').click()
  await page.getByLabel('Most characters deployed').fill('3')
  await page.getByLabel('Require a character…').selectOption({ label: 'Sera' })
  await page.getByTestId('prep-required-sera').getByRole('combobox').selectOption({ label: 'Slot 2' })
  await page.getByLabel('Forbid a character…').selectOption({ label: 'Guard' })
  await page.getByRole('switch', { name: /^Sell things before the battle/ }).check()
  await page.getByLabel('Add an item…').selectOption({ label: 'Potion' })
  await saved(page)
  expect(readDisk(id, 'maps/meadow.json').prep).toEqual({
    slots: [{ x: 0, y: 1 }, { x: 0, y: 2 }, { x: 0, y: 3 }, { x: 1, y: 2 }, { x: 4, y: 2 }], limit: 3,
    required: [{ character: 'sera', slot: 2 }], forbidden: ['guard'], shop: { sells: ['potion'] },
  })
  await expect(page.getByTestId('prep-checks')).toContainText('Nothing to check')
  await shot(page, 'prep-editor-desktop')
  expect(await (await page.request.get(`/api/projects/${id}/problems`)).json()).toEqual({ problems: [] })  // the real loader accepts it

  // --- Taking slot 2 away: Sera is no longer fixed (any slot), the numbers close up.
  await tool(page, 'Slots').click()
  await clickCell(page, 0, 2)
  await saved(page)
  expect(readDisk(id, 'maps/meadow.json').prep.required).toEqual(['sera'])
  expect(readDisk(id, 'maps/meadow.json').prep.slots).toHaveLength(4)

  // --- A mistake is flagged above the map: no slots left.
  for (const [x, y] of [[0, 1], [0, 3], [1, 2], [4, 2]]) await clickCell(page, x, y)
  await expect(page.getByTestId('map-warnings')).toContainText('no deployment squares')
  await page.getByRole('button', { name: 'Undo' }).click()
  await expect(canvas(page)).toHaveAttribute('data-slots', '4,2')
  await expect(page.getByTestId('map-warnings')).toHaveCount(0)

  // --- Switch off: the key goes away.
  await tab(page, 'Preparation').click()
  await prepOn(page).uncheck()
  await saved(page)
  expect('prep' in readDisk(id, 'maps/meadow.json')).toBe(false)
  await expect(canvas(page)).toHaveAttribute('data-slots', '')
}

test('@smoke preparation: switch on, paint slots, limit, required with a slot, forbidden, shop: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await prepGame(game)
  await run(page, id)
})

test.describe('on a phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('preparation tab and slots tool work at 360 px without sideways scrolling', async ({ page, game }) => {
    test.setTimeout(120_000)
    const id = await prepGame(game)
    await page.goto(editorUrl(id, 'maps', 'orchard'))
    await expect(canvas(page)).toHaveAttribute('data-view', /.+/)
    await expectNoHScroll(page)
    await expect(canvas(page)).toHaveAttribute('data-slots', '0,1;0,2;0,3;1,0')
    await tab(page, 'Preparation').click()
    await expect(page.getByTestId('prep-panel')).toBeVisible()
    await expect(page.getByLabel('Most characters deployed')).toHaveValue('3')
    await expect(page.getByTestId('prep-required-sera')).toBeVisible()
    await expect(page.getByTestId('prep-checks')).toContainText('Nothing to check')
    await expectNoHScroll(page)
    await shot(page, 'prep-editor-phone')
    // A tap with the Slots tool adds a slot (the map is a touch surface).
    await tool(page, 'Slots').tap()
    await page.getByRole('tab', { name: 'Tool', exact: true }).tap()
    const box = (await canvas(page).boundingBox())!
    const [scale, vx, vy] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
    await page.touchscreen.tap(box.x + vx + 5.5 * scale, box.y + vy + 2.5 * scale)
    await saved(page)
    expect(readDisk(id, 'maps/orchard.json').prep.slots).toHaveLength(5)
    await expectNoHScroll(page)
  })
})
