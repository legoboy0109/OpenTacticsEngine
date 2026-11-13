// Art screenshots for design review (npm run screenshots; skipped otherwise). They use the starter art that the
// templates carry; when a template has none yet, simple flat pieces are seeded so the layout can still be judged.
import type { APIRequestContext, Page } from '@playwright/test'
import { clickPiece, openArt, picker, pickerGrid, seedArt, sheetPng } from './artkit'
import { editorUrl, expect, saved, test } from './fixtures'
import { openMap, pickTerrain, tool } from './mapkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string, full = false) {
  await page.waitForTimeout(350)
  if (!full) { await page.screenshot({ path: `${OUT}/${name}.png` }); return }
  const height = await page.evaluate(() => document.documentElement.scrollHeight)
  const width = page.viewportSize()!.width
  await page.screenshot({ path: `${OUT}/${name}.png`, fullPage: true, clip: { x: 0, y: 0, width, height: Math.min(height, 1900) } })
}

/** A game from the template with its art; seeds plain pieces if the template has none. Returns the first image's id. */
async function artGame(request: APIRequestContext, create: () => Promise<string>): Promise<{ id: string; sheet: string }> {
  const id = await create()
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  if (doc.graphics?.sheets?.length) return { id, sheet: doc.graphics.sheets[0].id }
  await seedArt(request, id, { tiles: sheetPng(8, 6), men: sheetPng(4, 3) }, {
    sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 24, cellHeight: 24, name: 'Terrain' }, { id: 'men', file: 'art/men.png', cellWidth: 24, cellHeight: 24, name: 'Soldiers' }],
    terrain: { plain: { sheet: 'tiles', variants: [[0, 0], [1, 0], [2, 0]] }, forest: { sheet: 'tiles', cell: [3, 0] }, mountain: { sheet: 'tiles', join: [0, 2] }, water: { sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 500 } },
    units: { lord: { sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', cell: [0, 2] } } }, cavalier: { sheet: 'men', cell: [1, 0] }, archer: { sheet: 'men', frames: [[2, 0], [3, 0]] }, soldier: { sheet: 'men', cell: [0, 1] } },
  })
  return { id, sheet: 'tiles' }
}

async function addStyles(request: APIRequestContext, id: string) {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.styles = [{ id: 'classic', name: 'Classic' }, { id: 'parchment', name: 'Parchment', background: '#2b2118', showGlyphs: false, terrainColors: { plain: '#d9c79a', forest: '#9aa66a' } }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game } })).ok()).toBeTruthy()
}

test.describe('art screenshots', () => {
  test('desktop light', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const { id, sheet } = await artGame(request, () => game.create('strategy-rpg', 'Ford Skirmish'))
    await addStyles(request, id)
    await openArt(page, id, sheet)
    await expect(page.getByTestId('grid-summary')).toBeVisible()
    await shot(page, '32-art-light', true)

    await page.goto(editorUrl(id, 'terrain', 'forest')); await saved(page)
    await shot(page, '33-terrain-picture-light', true)
    await page.getByRole('button', { name: /^(Choose|Change) picture…$/ }).click()
    await expect(picker(page)).toBeVisible()
    await clickPiece(page, pickerGrid(page), 3, 0)
    await shot(page, '34-picker-one-light')
    await picker(page).getByRole('radio', { name: 'Random mix' }).click()
    for (const [c, r] of [[0, 0], [1, 0], [2, 0]]) await clickPiece(page, pickerGrid(page), c, r)
    await shot(page, '35-picker-mix-light')
    await picker(page).getByRole('radio', { name: 'Animation' }).click()
    await picker(page).getByRole('button', { name: 'Clear' }).click()
    for (const [c, r] of [[0, 1], [1, 1], [0, 1]]) await clickPiece(page, pickerGrid(page), c, r)
    await shot(page, '36-picker-animation-light')
    await picker(page).getByRole('radio', { name: 'Joins with neighbours' }).click()
    await clickPiece(page, pickerGrid(page), 0, 2)
    await shot(page, '37-picker-join-light')
    await page.keyboard.press('Escape')

    await page.goto(editorUrl(id, 'units', 'lord')); await saved(page)
    await shot(page, '38-unit-picture-light', true)
    await page.getByRole('button', { name: /^(Choose|Change) picture…$/ }).first().click()
    await shot(page, '39-picker-unit-light')
    await page.keyboard.press('Escape')

    await page.goto(editorUrl(id, 'terrain', 'forest')); await saved(page)
    await page.getByLabel('For look:').selectOption('parchment')
    await shot(page, '40-picture-looks-light')

    await openMap(page, id, 'ford')
    await pickTerrain(page, 'Forest').click()
    await shot(page, '41-map-art-light')
    await page.getByLabel('Look to preview').selectOption('parchment')
    await shot(page, '42-map-art-look-light')
    await tool(page, 'Units').click()
    await page.getByLabel('Look to preview').selectOption('classic')
    await shot(page, '43-map-art-units-light')
  })

  test('desktop dark', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 1180, height: 760 })
    await page.emulateMedia({ colorScheme: 'dark', reducedMotion: 'reduce' })
    const { id, sheet } = await artGame(request, () => game.create('economy-tactics', 'Night Ops'))
    await openArt(page, id, sheet)
    await expect(page.getByTestId('grid-summary')).toBeVisible()
    await shot(page, '44-art-dark', true)
    await page.goto(editorUrl(id, 'terrain', 'city')); await saved(page)
    await page.getByRole('button', { name: /^(Choose|Change) picture…$/ }).click()
    await clickPiece(page, pickerGrid(page), 1, 0)
    await shot(page, '45-picker-dark')
    await page.keyboard.press('Escape')
    const maps = (await (await request.get(`/api/projects/${id}`)).json()).game.maps
    await openMap(page, id, maps[0].id)
    await shot(page, '46-map-art-dark')
  })

  test('mobile', async ({ page, request, game }) => {
    await page.setViewportSize({ width: 360, height: 740 })
    await page.emulateMedia({ colorScheme: 'light', reducedMotion: 'reduce' })
    const { id, sheet } = await artGame(request, () => game.create('strategy-rpg', 'Pocket Game'))
    await page.goto(editorUrl(id, 'art')); await saved(page)
    await shot(page, '47-mobile-art-list', true)
    await page.goto(editorUrl(id, 'art', sheet)); await saved(page)
    await expect(page.getByTestId('grid-summary')).toBeVisible()
    await shot(page, '48-mobile-art-details', true)
    await page.goto(editorUrl(id, 'terrain', 'forest')); await saved(page)
    await shot(page, '49-mobile-terrain-picture', true)
    await page.getByRole('button', { name: /^(Choose|Change) picture…$/ }).click()
    await expect(picker(page)).toBeVisible()
    await shot(page, '50-mobile-picker')
    await picker(page).getByRole('radio', { name: 'Joins with neighbours' }).click()
    await shot(page, '51-mobile-picker-join')
    await page.keyboard.press('Escape')
    await openMap(page, id, 'ford')
    await shot(page, '52-mobile-map-art')
  })
})
