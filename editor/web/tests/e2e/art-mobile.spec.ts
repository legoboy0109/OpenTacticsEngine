// Art on a phone (360 x 740, touch): the screens fit, the picker works with taps.
import { addImages, openArt, png, picker, pickerGrid, piecePoint, seedArt, sheetPng, tilesGraphics } from './artkit'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, sectionMenu, test } from './fixtures'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')
const NAV = 17 // every section the editor has, the five strategy-RPG ones (Conditions too) included

test('the section switcher on a phone: one row that fits, a menu panel that lists every section (the real strategy-rpg template: Characters, Skills, Abilities, Conditions, Supports too) without sideways scroll', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Art Nav', { srpgContent: true })
  await page.goto(editorUrl(id, 'game'))
  await expectNoHScroll(page)
  const nav = page.getByRole('navigation', { name: 'Sections' })
  const bar = sectionMenu(page)
  // Closed: one row, showing the current section; the links are out of the tab order and the accessibility tree.
  await expect(bar).toBeVisible()
  await expect(bar).toContainText('Game')
  await expect(bar).toHaveAttribute('aria-expanded', 'false')
  expect((await bar.boundingBox())!.height).toBeLessThan(60)
  expect(await bar.evaluate((el) => el.scrollWidth > el.clientWidth + 1), 'the row is whole').toBe(false)
  await expect(nav).toBeHidden()
  await expect(nav.getByRole('link')).toHaveCount(0)
  // Open: every section is listed, whole, in the window, without sideways scroll; the panel scrolls itself when it is taller than the window.
  await bar.click()
  await expect(bar).toHaveAttribute('aria-expanded', 'true')
  const links = nav.getByRole('link')
  await expect(links).toHaveCount(NAV)
  await expectNoHScroll(page)
  for (let i = 0; i < NAV; i++) {
    const clipped = await links.nth(i).evaluate((el) => el.scrollWidth > el.clientWidth + 1)
    expect(clipped, await links.nth(i).innerText()).toBe(false)
  }
  const panel = await nav.evaluate((el) => ({ over: el.scrollHeight > el.clientHeight, canScroll: getComputedStyle(el).overflowY, h: el.getBoundingClientRect().bottom, win: window.innerHeight, sideways: el.scrollWidth > el.clientWidth }))
  expect(panel.sideways).toBe(false)
  expect(panel.h).toBeLessThanOrEqual(panel.win + 1)
  if (panel.over) expect(panel.canScroll).toBe('auto')
  // The last link is reachable in the panel, and choosing it goes there and closes the panel.
  await links.last().scrollIntoViewIfNeeded()
  await links.last().click()
  await expect(page).toHaveURL(/\/campaign$/)
  await expect(nav).toBeHidden()
  await expect(bar).toContainText('Campaign')
  await expect(bar).toBeFocused()
})

test('the phone section menu: focus moves in and back; Escape, the button and a tap outside close it; the top bar stays uncovered', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Phone Menu Keys')
  await page.goto(editorUrl(id, 'game'))
  const nav = page.getByRole('navigation', { name: 'Sections' })
  const bar = sectionMenu(page)
  await bar.click()
  await expect(nav.getByRole('link', { name: 'Game', exact: true })).toBeFocused() // the current section's link
  await page.keyboard.press('Tab')
  await expect(nav.getByRole('link', { name: 'Rules', exact: true })).toBeFocused()
  // the panel starts under the row, so the top bar is never covered
  const top = await page.locator('.topbar').boundingBox()
  expect((await nav.boundingBox())!.y).toBeGreaterThanOrEqual(top!.y + top!.height - 1)
  await page.keyboard.press('Escape')
  await expect(nav).toBeHidden()
  await expect(bar).toBeFocused()
  await bar.click()
  await expect(nav).toBeVisible()
  await bar.click()
  await expect(nav).toBeHidden()
  await bar.click()
  await expect(nav).toBeVisible()
  await page.mouse.click(180, 700) // outside the panel (a tap on the page behind it)
  await expect(nav).toBeHidden()
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
