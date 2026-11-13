// Art on a phone (360 x 740, touch): the screens fit, the picker works with taps.
import { addImages, openArt, png, picker, pickerGrid, piecePoint, seedArt, sheetPng, tilesGraphics } from './artkit'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

test('the section switcher still fits with Art in it: every label is whole', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Art Nav')
  await page.goto(editorUrl(id, 'game'))
  await expectNoHScroll(page)
  const links = page.getByRole('navigation', { name: 'Sections' }).getByRole('link')
  await expect(links.first()).toBeVisible()
  expect(await links.count()).toBe(7)
  for (let i = 0; i < 7; i++) {
    const clipped = await links.nth(i).evaluate((el) => el.scrollWidth > el.clientWidth + 1)
    expect(clipped, await links.nth(i).innerText()).toBe(false)
  }
})

test('Art screen at 360 px: list, details and unused files fit; adding works', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Art')
  await seedArt(request, id, { spare: sheetPng(1, 1) }, { sheets: [] })
  await openArt(page, id)
  await expectNoHScroll(page)
  await expect(page.getByRole('region', { name: 'Unused files' })).toBeVisible()
  await addImages(page, [png('tiles.png', sheetPng(4, 3))]) // the new image opens straight away (the list is hidden on a phone)
  await expectNoHScroll(page)
  await expect(page.getByTestId('grid-summary')).toHaveText('4 across, 3 down')
  const grid = page.getByTestId('piece-grid')
  expect((await grid.boundingBox())!.width).toBeGreaterThan(150)
})

test('the picker at 360 px: fits the screen, big enough buttons, a tap on a piece chooses it', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Picker')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics())
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  const dlg = picker(page)
  await expect(dlg).toBeVisible()
  const box = (await dlg.boundingBox())!
  expect(box.width).toBeLessThanOrEqual(360)
  expect(box.x).toBeGreaterThanOrEqual(0)
  await expectNoHScroll(page)
  expect(await dlg.evaluate((el) => el.scrollWidth <= el.clientWidth + 1)).toBe(true)
  // Touch targets: kinds, zoom buttons and the footer buttons are at least 40 px tall.
  for (const loc of [dlg.getByRole('radio', { name: 'Random mix' }), dlg.getByRole('button', { name: 'Zoom in' }), dlg.getByRole('button', { name: 'Use this picture' })]) {
    expect((await loc.boundingBox())!.height, 'touch target height').toBeGreaterThanOrEqual(36)
  }
  const p = await piecePoint(pickerGrid(page), 2, 0)
  await page.touchscreen.tap(p.x, p.y)
  await expect(page.getByTestId('chosen-summary')).toHaveText('1 piece')
  await dlg.getByRole('button', { name: 'Use this picture' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.forest.cell).toEqual([2, 0])
  await expectNoHScroll(page)
})

test('the join picker and the unit sides list fit at 360 px', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Join')
  await seedArt(request, id, { tiles: sheetPng(4, 4) }, tilesGraphics({ units: { lord: { sheet: 'tiles', cell: [0, 0] } } }))
  await page.goto(editorUrl(id, 'terrain', 'mountain'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await picker(page).getByRole('radio', { name: 'Joins with neighbours' }).click()
  await expect(picker(page).getByText('Which piece goes where')).toBeVisible()
  await expectNoHScroll(page)
  expect(await picker(page).evaluate((el) => el.scrollWidth <= el.clientWidth + 1)).toBe(true)
  await page.keyboard.press('Escape')
  await page.goto(editorUrl(id, 'units', 'lord'))
  await expect(page.getByRole('region', { name: 'A different picture for a side' })).toBeVisible()
  await expectNoHScroll(page)
})
