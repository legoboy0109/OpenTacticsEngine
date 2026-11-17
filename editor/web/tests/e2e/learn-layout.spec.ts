// Where the lesson panel sits and how the pointing ring behaves: docked at the side on wide windows, a bottom sheet that
// folds into a slim bar below that (phones included), room made for it, and a ring that follows, ignores clicks and
// respects reduced motion.
import { gotoSection, expectNoHScroll } from './fixtures'
import { expectRingOnOneOf } from './learnstart'
import { canvas, openMap } from './mapkit'
import {
  expect, expectRingOn, learnButton, learnList, panel, PRACTICE, ring, startFromOffer, stepTitle, test,
} from './learnkit'

const box = async (loc: import('@playwright/test').Locator) => (await loc.boundingBox())!

test.describe('phone (390 x 844)', () => {
  test.use({ viewport: { width: 390, height: 844 }, hasTouch: true })

  test('@smoke the lesson is a bottom sheet that folds into a slim bar, and stays across screens and reloads', async ({ page }) => {
    await startFromOffer(page)
    const p = panel(page)
    let b = await box(p)
    expect(b.x).toBe(0)
    expect(b.width).toBe(390)
    expect(Math.round(b.y + b.height)).toBe(844) // sits on the bottom edge
    expect(b.height).toBeLessThan(844 * 0.6)
    await expectNoHScroll(page)
    await expect(p.getByText('Step 1 of 10')).toBeVisible()
    // The screen's content is not covered: the page keeps room under the sheet.
    const room = await page.evaluate(() => parseFloat(getComputedStyle(document.getElementById('root')!).paddingBottom))
    expect(Math.abs(room - b.height)).toBeLessThanOrEqual(1)

    // Folded: one slim bar with the step's title.
    await p.getByRole('button', { name: 'Hide lesson' }).click()
    const bar = p.getByRole('button', { name: /Step 1 of 10: Name your game/ })
    await expect(bar).toBeVisible()
    b = await box(p)
    expect(b.height).toBeLessThan(64)
    expect(Math.round(b.y + b.height)).toBe(844)
    await expectNoHScroll(page)
    const folded = await page.evaluate(() => parseFloat(getComputedStyle(document.getElementById('root')!).paddingBottom))
    expect(Math.abs(folded - b.height)).toBeLessThanOrEqual(1)

    // It stays across screens and reloads, still folded (the choice is remembered), and opens again.
    await page.goto(`/p/${PRACTICE}/maps/start`)
    await expect(canvas(page)).toBeVisible()
    await expect(bar).toBeVisible()
    await page.reload()
    await expect(bar).toBeVisible()
    await bar.click()
    await expect(stepTitle(page)).toHaveText('Name your game')
    await expect(p.getByRole('button', { name: 'Hide lesson' })).toBeVisible()
  })

  test('the ring points at the sidebar link of the screen to open, and the sheet does not hide the Learn list', async ({ page }) => {
    await startFromOffer(page)
    await expectRingOn(page, 'game-name')
    await page.getByRole('button', { name: 'Skip this step' }).click()
    // On a phone the section links sit in the closed menu: the ring (still aimed at the Terrain link) rests on the menu button.
    await expectRingOnOneOf(page, 'nav-terrain', 'nav-menu')
    await learnButton(page).click()
    await expect(learnList(page)).toBeVisible()
    const l = await box(learnList(page))
    expect(l.x).toBeGreaterThanOrEqual(0)
    expect(l.x + l.width).toBeLessThanOrEqual(390)
  })
})

test.describe('laptop (1280 x 720): a bottom sheet', () => {
  test.use({ viewport: { width: 1280, height: 720 } })

  test('a sheet across the bottom with room made for it; the map editor keeps its canvas', async ({ page }) => {
    await startFromOffer(page)
    const p = panel(page)
    const b = await box(p)
    expect(b.x).toBe(0)
    expect(b.width).toBe(1280)
    expect(Math.round(b.y + b.height)).toBe(720)
    await openMap(page, PRACTICE, 'start')
    // Same canvas width as without a lesson: the sheet takes height, not width.
    const withLesson = await box(canvas(page))
    expect(withLesson.width).toBeGreaterThan(400)
    await p.getByRole('button', { name: 'Hide lesson' }).click()
    await expect(p.getByRole('button', { name: /Step 1 of 10/ })).toBeVisible()
    const folded = await box(canvas(page))
    expect(Math.abs(folded.width - withLesson.width)).toBeLessThanOrEqual(1)
    // The page can be scrolled far enough to see everything above the bar.
    await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight))
    const last = await box(page.locator('.map-view-note'))
    const bar = await box(p)
    expect(last.y + last.height).toBeLessThanOrEqual(bar.y + 1)
  })
})

test.describe('wide window (1920 x 1080): docked at the side', () => {
  test.use({ viewport: { width: 1920, height: 1080 } })

  test('@smoke the panel is a column at the right edge, the screen gives up that room, and the map canvas never goes under it', async ({ page }) => {
    await startFromOffer(page)
    const p = panel(page)
    const b = await box(p)
    expect(b.width).toBe(340)
    expect(Math.round(b.x + b.width)).toBe(1920)
    expect(b.y).toBe(0)
    expect(Math.round(b.height)).toBe(1080)
    // The content ends where the panel begins: the top bar's right end (Play) is not under the panel.
    const play = await box(page.getByRole('button', { name: 'Play', exact: true }))
    expect(play.x + play.width).toBeLessThanOrEqual(b.x)

    await openMap(page, PRACTICE, 'start')
    const mapPanel = page.locator('.map-panel')
    const c1 = await box(canvas(page))
    expect(c1.x + c1.width).toBeLessThanOrEqual((await box(mapPanel)).x)
    expect((await box(mapPanel)).x + (await box(mapPanel)).width).toBeLessThanOrEqual(b.x)
    // Fold and open again while the map is shown: the canvas follows the room it has (no overlap, no stuck size).
    await p.getByRole('button', { name: 'Hide lesson' }).click()
    await expect(p.getByRole('button', { name: /Step 1 of 10/ })).toBeVisible()
    const c2 = await box(canvas(page))
    expect(c2.width).toBeGreaterThan(c1.width) // folded: the bar takes only height
    await p.getByRole('button', { name: /Step 1 of 10/ }).click()
    await expect(p.getByRole('button', { name: 'Hide lesson' })).toBeVisible()
    await expect.poll(async () => { const c = await box(canvas(page)); return c.x + c.width <= (await box(mapPanel)).x }).toBe(true)
    const c3 = await box(canvas(page))
    expect(Math.abs(c3.width - c1.width)).toBeLessThanOrEqual(2)
  })

  test('the canvas still takes input beside the docked panel', async ({ page }) => {
    await startFromOffer(page)
    await openMap(page, PRACTICE, 'start')
    await page.getByRole('button', { name: 'Skip this step' }).click() // name
    await expect(stepTitle(page)).toHaveText('Add a new kind of ground')
    const c = await box(canvas(page))
    await page.getByRole('radiogroup', { name: 'Terrain' }).getByRole('radio', { name: 'Wall' }).click()
    await page.mouse.click(c.x + 40, c.y + c.height / 2) // a left-hand square; the data change shows in the status line
    await expect(page.getByTestId('map-status')).toBeVisible()
  })
})

test.describe('the pointing ring', () => {
  test.use({ viewport: { width: 1280, height: 520 } })

  test('is on the named control, follows it when the page scrolls, ignores clicks, and is not there without a target', async ({ page }) => {
    await startFromOffer(page)
    await expectRingOn(page, 'game-name')
    await expect(ring(page)).toHaveCSS('pointer-events', 'none')
    // Scrolling moves the control; the ring stays on it.
    await page.mouse.wheel(0, 60)
    await expect.poll(async () => (await page.evaluate(() => window.scrollY)) > 0).toBe(true)
    await expectRingOn(page, 'game-name')
    // A click on the ring's area reaches the control underneath.
    const field = await box(page.locator('[data-learn="game-name"]'))
    await page.mouse.click(field.x + field.width / 2, field.y + field.height / 2)
    await expect(page.locator('[data-learn="game-name"]')).toBeFocused()
    // The ring can point at a control that is not shown yet: nothing breaks and no ring hangs in the air.
    await page.getByRole('button', { name: 'Skip this step' }).click() // step 2: terrain, ring on the sidebar link
    await gotoSection(page, 'Terrain')
    await expectRingOn(page, 'new-item')
    await page.getByRole('button', { name: 'Skip this step' }).click() // step 3: maps; its terrain list is not on the Terrain screen
    await expect(stepTitle(page)).toHaveText('Paint it on the map')
    await expectRingOn(page, 'nav-maps')
  })

  test('pulses only when motion is welcome', async ({ page }) => {
    await startFromOffer(page)
    await expectRingOn(page, 'game-name')
    await expect(ring(page)).toHaveCSS('animation-name', 'learn-pulse')
    await page.emulateMedia({ reducedMotion: 'reduce' })
    await expect(ring(page)).toHaveCSS('animation-name', 'none')
  })

  test('stays while the panel is folded (on a phone that is how the maker gets at the controls) and is not shown on the game list', async ({ page }) => {
    await startFromOffer(page)
    await expectRingOn(page, 'game-name')
    await panel(page).getByRole('button', { name: 'Hide lesson' }).click()
    await expectRingOn(page, 'game-name')
    await page.getByRole('button', { name: /Step 1 of 10/ }).click()
    await expectRingOn(page, 'game-name')
    await page.getByRole('link', { name: /Back to all games/ }).click()
    await expect(page.getByRole('heading', { name: 'Your games' })).toBeVisible()
    await expect(ring(page)).toHaveCount(0)
  })
})
