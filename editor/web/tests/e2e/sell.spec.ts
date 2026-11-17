// Selling items back through the real editor (docs/EVENTS.md "Preparation", docs/EDITOR_UI.md): the preparation shop's "The shop
// also buys items" switch, an item's "Can be sold" and the Rules screen's "Shops pay back". The proof that matters: the exact
// JSON in the files and that the real game loader accepts it (/problems is empty). Desktop and a 360 px phone.
import { cpSync, rmSync, readdirSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import { diskPath, editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { openMap, tab } from './mapkit'

const repoFixture = path.resolve(import.meta.dirname, '../../../../tests/fixtures/srpg-prep')

/** A new game whose files are replaced by the engine's preparation fixture (its orchard has a shop that sells potion and tonic). */
async function prepGame(game: { create: (t: string, n?: string) => Promise<string> }): Promise<string> {
  const id = await game.create('blank', 'Muster')
  for (const dir of ['maps', 'data']) for (const f of readdirSync(diskPath(id, dir))) rmSync(diskPath(id, `${dir}/${f}`), { recursive: true, force: true })
  cpSync(repoFixture, diskPath(id, '.'), { recursive: true })
  rmSync(diskPath(id, 'README.md'), { force: true })
  return id
}

const problems = async (page: Page, id: string) => (await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems
const buysSwitch = (page: Page) => page.getByRole('switch', { name: /^The shop also buys items/ })
const sellableSwitch = (page: Page) => page.getByRole('switch', { name: /^Can be sold/ })

async function run(page: Page, id: string): Promise<void> {
  // --- The shop card: the switch is on (the default); turning it off writes buys: false, on again removes the key.
  await openMap(page, id, 'orchard')
  await tab(page, 'Preparation').click()
  await expect(buysSwitch(page)).toBeChecked()
  expect(readDisk(id, 'maps/orchard.json').prep.shop).toEqual({ sells: ['potion', 'tonic'] })
  await buysSwitch(page).uncheck()
  await saved(page)
  expect(readDisk(id, 'maps/orchard.json').prep.shop).toEqual({ sells: ['potion', 'tonic'], buys: false })
  expect(await problems(page, id)).toEqual([])
  await page.getByLabel('Add an item…').selectOption({ label: 'Ring' })  // changing the list keeps the switch
  await saved(page)
  expect(readDisk(id, 'maps/orchard.json').prep.shop).toEqual({ sells: ['potion', 'tonic', 'ring'], buys: false })
  await buysSwitch(page).check()
  await saved(page)
  expect(readDisk(id, 'maps/orchard.json').prep.shop).toEqual({ sells: ['potion', 'tonic', 'ring'] })
  await expect(page.getByTestId('prep-checks')).toContainText('Nothing to check')

  // --- The item form: "Can be sold" shows for an item with a price while a shop buys; off writes sellable: false.
  await page.goto(editorUrl(id, 'items', 'potion'))
  await expect(sellableSwitch(page)).toBeChecked()
  await sellableSwitch(page).uncheck()
  await saved(page)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'potion')).toMatchObject({ sellable: false })
  expect(await problems(page, id)).toEqual([])
  await sellableSwitch(page).check()
  await saved(page)
  expect(readDisk(id, 'data/items.json').items.find((i: any) => i.id === 'potion')).not.toHaveProperty('sellable')
  await page.goto(editorUrl(id, 'items', 'token'))  // no price: nothing to sell, no switch
  await expect(page.getByRole('heading', { name: 'Token', level: 2 })).toBeVisible()
  await expect(sellableSwitch(page)).toHaveCount(0)

  // --- Rules: the share a shop pays; the default is not written, a change is, the loader accepts it.
  await page.goto(editorUrl(id, 'rules'))
  const share = page.getByLabel('Shops pay back')
  await expect(share).toHaveValue('50')
  await share.fill('30')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.items).toEqual({ inventorySize: 3, storage: 'anywhere', weapons: 'fixed', sellPercent: 30 })
  expect(await problems(page, id)).toEqual([])
  await share.fill('50')
  await saved(page)
  expect(readDisk(id, 'game.json').rules.items).not.toHaveProperty('sellPercent')
  await expectNoHScroll(page)
}

test('@smoke selling: the shop buys switch, an item\'s Can be sold and the share on the Rules screen: the files are exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(120_000)
  const id = await prepGame(game)
  await run(page, id)
})

test.describe('on a phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the selling switches work at 360 px without sideways scrolling', async ({ page, game }) => {
    test.setTimeout(120_000)
    const id = await prepGame(game)
    await page.goto(editorUrl(id, 'maps', 'orchard'))
    await tab(page, 'Preparation').click()
    await expect(buysSwitch(page)).toBeVisible()
    await expectNoHScroll(page)
    await page.goto(editorUrl(id, 'items', 'potion'))
    await expect(sellableSwitch(page)).toBeVisible()
    await expectNoHScroll(page)
    await page.goto(editorUrl(id, 'rules'))
    await expect(page.getByLabel('Shops pay back')).toBeVisible()
    await expectNoHScroll(page)
  })
})
