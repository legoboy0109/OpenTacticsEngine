// Structures screenshots for design review (npm run screenshots; skipped otherwise). Numbering continues after the art set.
import type { APIRequestContext, Page } from '@playwright/test'
import { clickPiece, picker, pickerGrid, seedArt, sheetPng } from './artkit'
import { editorUrl, expect, saved, test } from './fixtures'
import { cellPoint, clickCell, hoverCell, openMap, pickSide, pickStructure, tool } from './mapkit'
import { seedOldGame } from './oldkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string, full = false) {
  await page.waitForTimeout(350)
  if (!full) { await page.screenshot({ path: `${OUT}/${name}.png` }); return }
  const height = await page.evaluate(() => document.documentElement.scrollHeight)
  const width = page.viewportSize()!.width
  await page.screenshot({ path: `${OUT}/${name}.png`, fullPage: true, clip: { x: 0, y: 0, width, height: Math.min(height, 1900) } })
}

/** The economy template with a gate (movement and number overrides) and plain pieces for pictures. */
async function ecoGame(request: APIRequestContext, create: () => Promise<string>): Promise<string> {
  const id = await create()
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const gate = { id: 'gate', name: 'Gate', color: '#8a6a4a', glyph: 'G', width: 2, height: 1, moveCost: { foot: 1, tread: 0 }, props: { defense: 5, cover: 7 } }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, structures: [...doc.structures, gate] } })).ok()).toBeTruthy()
  return id
}

test.describe('structures screenshots', () => {
  test('desktop light', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 900 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const id = await ecoGame(request, () => game.create('economy-tactics', 'Capital Wars'))
    await page.goto(editorUrl(id, 'structures', 'hq')); await saved(page)
    await shot(page, '53-structures-light', true)
    await page.goto(editorUrl(id, 'structures', 'gate')); await saved(page)
    await page.getByLabel('Foot: rule').scrollIntoViewIfNeeded()
    await shot(page, '54-structure-moving-numbers')
    await page.goto(editorUrl(id, 'terrain', 'city')); await saved(page)
    await shot(page, '55-terrain-points-to-structures')

    // Pictures for structures (plain pieces seeded: a layout check, not an art review).
    await seedArt(request, id, { blds: sheetPng(4, 3) }, { sheets: [{ id: 'blds', file: 'art/blds.png', cellWidth: 24, cellHeight: 24, name: 'Buildings' }] })
    await page.goto(editorUrl(id, 'structures', 'hq')); await saved(page)
    await page.getByRole('button', { name: 'Choose picture…' }).click()
    await clickPiece(page, pickerGrid(page), 2, 1)
    await shot(page, '56-picker-structure')
    await picker(page).getByRole('button', { name: 'Use this picture' }).click()
    await saved(page)
    const card = page.locator('[id="picture:structures:hq"]')
    await card.scrollIntoViewIfNeeded()
    await shot(page, '57-structure-picture-card')
    await page.goto(editorUrl(id, 'game')); await saved(page)
  })

  test('map tool', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 800 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const id = await ecoGame(request, () => game.create('economy-tactics', 'Capital Wars'))
    await openMap(page, id, 'crossroads')
    await tool(page, 'Structures').click()
    await pickSide(page, 'Blue Army').click()
    await pickStructure(page, 'Headquarters').click()
    await hoverCell(page, 3, 5)
    await shot(page, '58-map-structure-ghost-valid')
    await hoverCell(page, 11, 7)
    await shot(page, '59-map-structure-ghost-invalid')
    await clickCell(page, 3, 5)
    await page.mouse.move((await cellPoint(page, 8, 7)).x, (await cellPoint(page, 8, 7)).y)
    await shot(page, '60-map-structure-selected')

    await pickStructure(page, 'Gate').click()
    await clickCell(page, 2, 0)
    await tool(page, 'Select').click()
    await clickCell(page, 3, 0)
    await shot(page, '61-map-select-effective')

    await page.getByRole('tab', { name: 'Map', exact: true }).click()
    await page.getByRole('button', { name: 'Resize…' }).click()
    const dlg = page.getByRole('dialog', { name: 'Resize the map' })
    await dlg.getByLabel('Width').fill('11')
    await dlg.getByRole('radio', { name: 'top left' }).click()
    await shot(page, '62-map-resize-structures')
    await dlg.getByRole('button', { name: 'Cancel' }).click()

    await page.goto(editorUrl(id, 'structures', 'city')); await saved(page)
    await page.getByRole('button', { name: 'Delete', exact: true }).click()
    await shot(page, '63-structure-delete-confirm')
  })

  test('old project: the offer', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 800 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const id = await game.create('economy-tactics', 'Old Capital Wars')
    await seedOldGame(request, id)
    await page.goto(editorUrl(id, 'maps', 'crossroads'))
    await expect(page.getByText('This game was made with an older version of the editor.')).toBeVisible()
    await shot(page, '64-convert-offer')
    await page.getByRole('button', { name: 'See what will change…' }).click()
    await shot(page, '65-convert-dialog')
  })

  test('desktop dark', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 800 })
    await page.emulateMedia({ colorScheme: 'dark', reducedMotion: 'reduce' })
    const id = await ecoGame(request, () => game.create('economy-tactics', 'Night Capital'))
    await page.goto(editorUrl(id, 'structures', 'hq')); await saved(page)
    await shot(page, '66-structures-dark')
    await openMap(page, id, 'crossroads')
    await tool(page, 'Structures').click()
    await pickSide(page, 'Red Army').click()
    await pickStructure(page, 'Headquarters').click()
    await hoverCell(page, 3, 5)
    await shot(page, '67-map-structures-dark')
  })

  test('mobile', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 360, height: 740 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const id = await ecoGame(request, () => game.create('economy-tactics', 'Pocket Capital'))
    await page.goto(editorUrl(id, 'structures')); await saved(page)
    await shot(page, '68-mobile-structures-list')
    await page.goto(editorUrl(id, 'structures', 'hq')); await saved(page)
    await shot(page, '69-mobile-structure-form', true)
    await openMap(page, id, 'crossroads')
    await tool(page, 'Structures').click()
    await pickSide(page, 'Blue Army').click()
    await pickStructure(page, 'Headquarters').click()
    await clickCell(page, 3, 5)
    await shot(page, '70-mobile-map-structures')
    await page.evaluate(() => window.scrollTo(0, 420))
    await shot(page, '71-mobile-map-structure-panel')
    const old = await game.create('economy-tactics', 'Old Pocket')
    await seedOldGame(request, old)
    await page.goto(editorUrl(old, 'game'))
    await page.getByRole('button', { name: 'See what will change…' }).click()
    await shot(page, '72-mobile-convert-dialog')
  })
})
