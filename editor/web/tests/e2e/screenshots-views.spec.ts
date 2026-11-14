// Kinds of view and heights screenshots for design review (npm run screenshots; skipped otherwise). Numbering continues after the structures set.
import type { Page } from '@playwright/test'
import { editorUrl, test } from './fixtures'
import { clickCell, dragThrough, openMap, row, tool } from './mapkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string) {
  await page.waitForTimeout(350)
  await page.screenshot({ path: `${OUT}/${name}.png` })
}

/** A few hills: a drag raises a ridge, a click on top makes it higher, a click elsewhere digs a hollow. */
async function sculpt(page: Page): Promise<void> {
  await tool(page, 'Heights').click()
  await dragThrough(page, row(2, 3, 7))
  await dragThrough(page, row(3, 4, 6))
  await clickCell(page, 5, 3)
  await page.getByRole('radio', { name: 'Lower' }).click()
  await clickCell(page, 1, 4)
}

test.describe('kinds of view and heights screenshots', () => {
  test('styles editor with an Isometric style', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 900 })
    const id = await game.create('strategy-rpg', 'Ford Skirmish')
    await page.goto(editorUrl(id, 'game'))
    await page.getByLabel('Isometric: Kind of view').scrollIntoViewIfNeeded()
    await page.locator('.subcard').nth(1).getByText('Advanced').click()
    await page.getByLabel('Isometric: Square width').fill('56')
    await page.locator('.subcard').nth(1).scrollIntoViewIfNeeded()
    await shot(page, '73-styles-isometric')
  })

  test('heights tool on the desktop', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    const id = await game.create('strategy-rpg', 'Ford Skirmish')
    await openMap(page, id, 'ford')
    await sculpt(page)
    await page.getByRole('radio', { name: 'Raise' }).click()
    await shot(page, '74-map-heights')
  })

  test.describe('on a phone', () => {
    test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
    test('heights tool at phone width', async ({ page, game }) => {
      const id = await game.create('strategy-rpg', 'Ford Skirmish')
      await openMap(page, id, 'ford')
      await sculpt(page)
      await shot(page, '75-mobile-map-heights')
    })
  })
})
