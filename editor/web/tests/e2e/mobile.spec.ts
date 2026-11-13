// Phone: 360 x 740 with touch. Chromium only (Firefox has no mobile emulation); touch is driven through the
// browser's own touch input, so the editor sees real touch pointer events.
import type { CDPSession, Page } from '@playwright/test'
import { addSecondMap, expect, expectNoHScroll, readDisk, requirePlayer, saved, test } from './fixtures'
import { canvas, cellPoint, diskGrid, openMap, pickSide, pickTerrain, pickUnitType, tab, tool } from './mapkit'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

type Pt = { x: number; y: number }
async function touch(page: Page): Promise<CDPSession> { return page.context().newCDPSession(page) }
const send = (c: CDPSession, type: 'touchStart' | 'touchMove' | 'touchEnd', pts: Pt[]) => c.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map((p, i) => ({ x: p.x, y: p.y, id: i })) })

async function touchDrag(page: Page, cells: Array<[number, number]>): Promise<void> {
  const c = await touch(page)
  const pts = []
  for (const [x, y] of cells) pts.push(await cellPoint(page, x, y))
  await send(c, 'touchStart', [pts[0]])
  for (const p of pts.slice(1)) await send(c, 'touchMove', [p])
  await send(c, 'touchEnd', [])
}

test('map editor at 360 px: one tidy top bar, canvas on top, no horizontal scroll on any tab', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Phone')
  await openMap(page, id, 'ford')
  await expectNoHScroll(page)
  const bar = await page.locator('.topbar').boundingBox()
  expect(bar!.height, 'the top bar fits on one row').toBeLessThanOrEqual(64)
  const box = (await canvas(page).boundingBox())!
  expect(box.width).toBeGreaterThan(320)
  expect(box.height).toBeGreaterThan(240)
  for (const t of ['Map', 'Sides', 'Tool'] as const) {
    await tab(page, t).click()
    await expectNoHScroll(page)
  }
  // The options sheet can be folded away to leave more room.
  await page.getByRole('button', { name: 'Hide options' }).click()
  await expect(page.getByRole('tabpanel')).toBeHidden()
  await page.getByRole('button', { name: 'Show options' }).click()
  await expect(page.getByRole('tabpanel')).toBeVisible()
  await tab(page, 'Map').click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  await expectNoHScroll(page)
  await page.keyboard.press('Escape')
})

test('painting by touch drag, a tap, and a long press that picks terrain', async ({ page, game }) => {
  const id = await game.create('blank', 'TouchPaint')
  await openMap(page, id, 'start')
  await pickTerrain(page, 'Wall').click()
  await canvas(page).scrollIntoViewIfNeeded()
  const before = diskGrid(id, 'start')
  const scrolled = await page.evaluate(() => window.scrollY)
  await touchDrag(page, [[0, 0], [1, 0], [2, 0], [3, 0]])
  await saved(page)
  expect(diskGrid(id, 'start')[0].slice(0, 5)).toEqual(['wall', 'wall', 'wall', 'wall', 'plain'])
  expect(diskGrid(id, 'start').slice(1)).toEqual(before.slice(1))
  // The page did not scroll while drawing.
  expect(await page.evaluate(() => window.scrollY)).toBe(scrolled)

  // A single tap paints one square.
  const c = await touch(page)
  const p = await cellPoint(page, 6, 5)
  await send(c, 'touchStart', [p]); await send(c, 'touchEnd', [])
  // (not saved(page): it can still say "Saved" for an instant before the tap's edit arrives)
  await expect.poll(() => diskGrid(id, 'start')[5][6]).toBe('wall')

  // A long press picks the terrain under the finger (and paints nothing).
  await pickTerrain(page, 'Wall').click()
  await canvas(page).scrollIntoViewIfNeeded()
  const plain = await cellPoint(page, 0, 3)
  await send(c, 'touchStart', [plain])
  await page.waitForTimeout(700)
  await send(c, 'touchEnd', [])
  await expect(pickTerrain(page, 'Plain')).toHaveAttribute('aria-checked', 'true')
  expect(diskGrid(id, 'start')[3][0]).toBe('plain')
})

test('place a unit with a tap; pinch zooms the map', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'TouchUnit')
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await pickSide(page, 'Red Warband').click()
  await pickUnitType(page, 'Archer').click()
  await canvas(page).scrollIntoViewIfNeeded()
  const c = await touch(page)
  const p = await cellPoint(page, 6, 3)
  await send(c, 'touchStart', [p]); await send(c, 'touchEnd', [])
  await expect.poll(() => readDisk(id, 'maps/ford.json').units.at(-1)).toEqual({ type: 'archer', side: 'red', x: 6, y: 3 })
  await expectNoHScroll(page)

  const scale = async () => Number((await canvas(page).getAttribute('data-view'))!.split(',')[0])
  const s0 = await scale()
  const box = (await canvas(page).boundingBox())!
  const mid = { x: box.x + box.width / 2, y: box.y + box.height / 2 }
  await send(c, 'touchStart', [{ x: mid.x - 20, y: mid.y }, { x: mid.x + 20, y: mid.y }])
  for (let i = 1; i <= 6; i++) await send(c, 'touchMove', [{ x: mid.x - 20 - i * 12, y: mid.y }, { x: mid.x + 20 + i * 12, y: mid.y }])
  await send(c, 'touchEnd', [])
  await expect.poll(scale).toBeGreaterThan(s0 * 1.5)
  expect(diskGrid(id, 'ford')).toBeTruthy() // the pinch painted nothing
})

test('Play at 360 px fills the screen, runs, and closes', async ({ page, game, request }) => {
  requirePlayer()
  const id = await game.create('strategy-rpg', 'PhonePlay')
  await addSecondMap(request, id)
  await openMap(page, id, 'ford')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(panel).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  const b = (await panel.boundingBox())!
  expect(b.width).toBeGreaterThanOrEqual(359)
  await expectNoHScroll(page)
  await panel.getByRole('button', { name: 'Close' }).click()
  await expect(panel).toBeHidden()
  await expectNoHScroll(page)
})

test('warnings and the problems panel do not break the phone layout', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'PhoneWarn')
  await openMap(page, id, 'ford')
  await pickTerrain(page, 'River').click()
  await canvas(page).scrollIntoViewIfNeeded()
  const c = await touch(page)
  const p = await cellPoint(page, 1, 3)
  await send(c, 'touchStart', [p]); await send(c, 'touchEnd', [])
  await expect(page.getByTestId('map-warnings')).toBeVisible()
  await saved(page)
  await page.getByRole('button', { name: '1 problem' }).click()
  await expectNoHScroll(page)
  const bar = await page.locator('.topbar').boundingBox()
  expect(bar!.height).toBeLessThanOrEqual(64)
  const wide = await page.evaluate(() => document.scrollingElement!.scrollWidth)
  expect(wide).toBeLessThanOrEqual(360)
})
