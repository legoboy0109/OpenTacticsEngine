// Map editor and Play screenshots for design review (npm run screenshots; skipped otherwise).
import type { Page } from '@playwright/test'
import { expect, saved, test } from './fixtures'
import { canvas, cellPoint, clickCell, openMap, pickSide, pickTerrain, pickUnitType, tab, tool } from './mapkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string) {
  await page.waitForTimeout(250)
  await page.screenshot({ path: `${OUT}/${name}.png` })
}
const hover = async (page: Page, x: number, y: number) => { const p = await cellPoint(page, x, y); await page.mouse.move(p.x, p.y) }
async function play(page: Page) {
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await page.waitForTimeout(900)
}

test.describe('map screenshots', () => {
  test('desktop light', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('strategy-rpg', 'Ford Skirmish')
    await openMap(page, id, 'ford')
    await pickTerrain(page, 'Forest').click()
    await hover(page, 6, 3)
    await shot(page, '17-map-terrain-light')

    await tool(page, 'Units').click()
    await pickSide(page, 'Red Warband').click()
    await pickUnitType(page, 'Archer').click()
    await clickCell(page, 1, 3) // Eirik: selects him
    await hover(page, 4, 4)
    await shot(page, '18-map-units-light')

    await tool(page, 'Select').click()
    await clickCell(page, 8, 4)
    await shot(page, '20-map-select-light')

    await page.setViewportSize({ width: 1180, height: 1040 })
    await shot(page, '21-map-unit-properties')
    await page.setViewportSize({ width: 1180, height: 760 })

    await tab(page, 'Map').click()
    await page.getByRole('button', { name: 'Resize…' }).click()
    const dlg = page.getByRole('dialog', { name: 'Resize the map' })
    await dlg.getByLabel('Width').fill('6')
    await dlg.getByLabel('Height').fill('4')
    await dlg.getByRole('radio', { name: 'top left' }).click()
    await shot(page, '22-map-resize-dialog')
    await dlg.getByRole('button', { name: 'Cancel' }).click()

    await tab(page, 'Tool').click()
    await tool(page, 'Terrain').click()
    await pickTerrain(page, 'River').click()
    await clickCell(page, 1, 4) // a cavalier stands here: flagged
    await hover(page, 1, 4)
    await shot(page, '24-map-warnings')
  })

  test('economy: owners, sides, dark', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('economy-tactics', 'Night Ops')
    await openMap(page, id, 'crossroads')
    await tool(page, 'Owners').click()
    await pickSide(page, 'Red').click()
    await hover(page, 5, 0)
    await shot(page, '19-map-owners-light')

    await tab(page, 'Sides').click()
    await page.getByRole('button', { name: 'Add a side' }).click()
    await shot(page, '23-map-sides-panel')

    await page.emulateMedia({ colorScheme: 'dark' })
    await tab(page, 'Tool').click()
    await tool(page, 'Terrain').click()
    await pickTerrain(page, 'Forest').click()
    await hover(page, 3, 5)
    await shot(page, '25-map-terrain-dark')
    await tool(page, 'Units').click()
    await pickSide(page, 'Blue').click()
    await pickUnitType(page, 'Tank').click()
    await clickCell(page, 5, 5)
    await shot(page, '26-map-units-dark')
  })

  test('play', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('economy-tactics', 'Play Room')
    await openMap(page, id, 'crossroads')
    await play(page)
    await shot(page, '27-play-running')
    await page.getByRole('button', { name: 'Close' }).click()

    // A unit standing on terrain it cannot enter is a problem the game cannot load with.
    const bad = await game.create('strategy-rpg', 'Blocked Ford')
    await openMap(page, bad, 'ford')
    await pickTerrain(page, 'River').click()
    await clickCell(page, 1, 3)
    await saved(page)
    await page.getByRole('button', { name: 'Play', exact: true }).click()
    await expect(page.getByRole('region', { name: 'Problems' })).toBeVisible()
    await shot(page, '28-play-blocked-problems')
  })

  test('mobile', async ({ page, game }) => {
    await page.setViewportSize({ width: 360, height: 740 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('strategy-rpg', 'Pocket Map')
    await openMap(page, id, 'ford')
    await shot(page, '29-mobile-map-editor')
    await tool(page, 'Units').click()
    await page.evaluate(() => window.scrollTo(0, 420))
    await shot(page, '30-mobile-map-options')
    await page.evaluate(() => window.scrollTo(0, 0))
    await page.getByRole('button', { name: 'Play', exact: true }).click()
    await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
    await page.waitForTimeout(900)
    await shot(page, '31-mobile-play')
    await expect(canvas(page)).toBeAttached()
  })
})
