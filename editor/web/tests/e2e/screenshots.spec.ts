// Regenerates editor/web/screenshots/*.png for design review.
// Run with: npm run screenshots   (skipped otherwise, so "npm run e2e" stays fast)
import type { APIRequestContext, Page } from '@playwright/test'
import { editorUrl, expect, saved, test } from './fixtures'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function busy(request: APIRequestContext, id: string, fn: (doc: any) => void) {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  fn(doc)
  const res = await request.put(`/api/projects/${id}`, {
    data: { baseRevision: doc.revision, game: doc.game, units: doc.units, weapons: doc.weapons, terrain: doc.terrain, maps: doc.maps },
  })
  expect(res.ok()).toBeTruthy()
}

async function shot(page: Page, name: string, full = false) {
  await page.waitForTimeout(150)
  await page.screenshot({ path: `${OUT}/${name}.png`, fullPage: full })
}

const NAMES = ['Knight', 'Archer', 'Mage of the Eastern Marches and Beyond', 'Cavalier', 'Fighter', 'Priest', 'Thief', 'Paladin', 'Sniper', 'Berserker']

test.describe('screenshots', () => {
  test('desktop light', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('strategy-rpg', 'Emblem Skirmish')
    await busy(request, id, (d) => {
      for (let i = 0; i < 30; i++) {
        const base = d.units[i % d.units.length]
        d.units.push({ ...base, id: `extra_${i}`, name: `${NAMES[i % NAMES.length]} ${i + 1}` })
      }
    })
    await page.goto('/')
    await expect(page.getByText('Emblem Skirmish')).toBeVisible()
    await shot(page, '01-picker-light')
    await page.getByRole('button', { name: 'New game' }).click()
    await expect(page.getByRole('radio').first()).toBeVisible()
    await shot(page, '02-new-game-dialog')
    await page.keyboard.press('Escape')

    await page.goto(editorUrl(id, 'game')); await saved(page)
    await shot(page, '03-game-light', true)
    await page.goto(editorUrl(id, 'rules')); await saved(page)
    await shot(page, '04-rules-light', true)
    await page.goto(editorUrl(id, 'units', 'cavalier')); await saved(page)
    await shot(page, '05-units-light-30-units')
    await page.goto(editorUrl(id, 'weapons', 'iron_bow')); await saved(page)
    await shot(page, '06-weapons-light', true)
    await page.goto(editorUrl(id, 'terrain', 'forest')); await saved(page)
    await shot(page, '07-terrain-light', true)
    await page.goto(editorUrl(id, 'maps')); await saved(page)
    await shot(page, '08-maps-light')

    // Problems panel + delete confirmation
    await page.goto(editorUrl(id, 'weapons', 'iron_lance')); await saved(page)
    await page.getByRole('button', { name: 'Delete' }).click()
    await shot(page, '09-delete-confirm-light')
    await page.keyboard.press('Escape')
    await page.goto(editorUrl(id, 'rules')); await saved(page)
    await page.getByRole('button', { name: 'Delete Skill' }).click()
    await shot(page, '10-delete-stat-confirm')
  })

  test('desktop dark', async ({ page, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'dark' })
    const id = await game.create('economy-tactics', 'Night Ops')
    await page.goto('/')
    await shot(page, '11-picker-dark')
    await page.goto(editorUrl(id, 'rules')); await saved(page)
    await shot(page, '12-rules-dark', true)
    await page.goto(editorUrl(id, 'terrain', 'city')); await saved(page)
    await shot(page, '13-terrain-dark')
  })

  test('mobile', async ({ page, game }) => {
    await page.setViewportSize({ width: 360, height: 740 })
    await page.emulateMedia({ colorScheme: 'light' })
    const id = await game.create('strategy-rpg', 'Pocket Game')
    await page.goto(editorUrl(id, 'units')); await saved(page)
    await shot(page, '14-mobile-units-list')
    await page.goto(editorUrl(id, 'units', 'cavalier')); await saved(page)
    await shot(page, '15-mobile-unit-detail', true)
    await page.goto(editorUrl(id, 'rules')); await saved(page)
    await shot(page, '16-mobile-rules', true)
  })
})
