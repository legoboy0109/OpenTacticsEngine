// Structures on a phone (360 x 740, touch): the screens fit, and the map tool works by touch, including a 2 x 2 structure.
import type { CDPSession, Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, openSectionMenu, readDisk, saved, test } from './fixtures'
import { canvas, cellPoint, diskStructures, openMap, pickSide, pickStructure, tool } from './mapkit'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

type Pt = { x: number; y: number }
const touch = (page: Page): Promise<CDPSession> => page.context().newCDPSession(page)
const send = (c: CDPSession, type: 'touchStart' | 'touchMove' | 'touchEnd', pts: Pt[]) => c.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map((p, i) => ({ x: p.x, y: p.y, id: i })) })

test('the Structures screen at 360 px: list, form, size boxes, movement table and picture card fit; no horizontal scroll', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Phone Structures')
  await page.goto(editorUrl(id, 'structures'))
  await expectNoHScroll(page)
  await openSectionMenu(page)
  const links = page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name: 'Structures', exact: true })
  await expect(links).toBeVisible()
  expect(await links.evaluate((el) => el.scrollWidth > el.clientWidth + 1), 'the "Structures" label is whole').toBe(false)
  await page.keyboard.press('Escape')
  await page.getByRole('list', { name: 'Structures' }).getByRole('button', { name: /Headquarters/ }).click()
  await expect(page.getByLabel('Width (squares)')).toBeVisible()
  await expectNoHScroll(page)
  for (const control of [page.getByLabel('Width (squares)'), page.getByLabel('Foot: rule'), page.getByRole('button', { name: /^(Choose|Change) picture…$/ })]) {
    await control.scrollIntoViewIfNeeded()
    await expectNoHScroll(page)
    expect((await control.boundingBox())!.height, 'touch target height').toBeGreaterThanOrEqual(34)
  }
})

test('the map tool by touch: tap places a 2 x 2 structure, dragging it moves it, a tap that does not fit says why', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Phone Place')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await expectNoHScroll(page)
  await pickSide(page, 'Red Army').click()
  await pickStructure(page, 'Headquarters').click()
  await canvas(page).scrollIntoViewIfNeeded()
  const c = await touch(page)
  const before = diskStructures(id, 'crossroads')

  // A tap at the bottom-right corner of the map would stick out: refused, with a sentence under the map.
  const edge = await cellPoint(page, 11, 7)
  await send(c, 'touchStart', [edge]); await send(c, 'touchEnd', [])
  await expect(page.getByTestId('map-notice')).toContainText('needs 2 × 2 squares and would stick out of the map')
  await page.waitForTimeout(900)
  expect(diskStructures(id, 'crossroads')).toEqual(before)

  // A tap on a free square places it with its top-left corner there.
  const spot = await cellPoint(page, 3, 5)
  await send(c, 'touchStart', [spot]); await send(c, 'touchEnd', [])
  await expect.poll(() => diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'hq', x: 3, y: 5, side: 'red' })
  await expectNoHScroll(page)

  // Drag it by its lower-right square two rows up: (4,6) -> (4,4).
  const from = await cellPoint(page, 4, 6)
  const to = await cellPoint(page, 4, 4)
  await send(c, 'touchStart', [from])
  for (let i = 1; i <= 4; i++) await send(c, 'touchMove', [{ x: from.x + ((to.x - from.x) * i) / 4, y: from.y + ((to.y - from.y) * i) / 4 }])
  await send(c, 'touchEnd', [])
  await expect.poll(() => diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'hq', x: 3, y: 3, side: 'red' })
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').structures).toHaveLength(before.length + 1)
  expect((await (await page.request.get(`/api/projects/${id}/problems`)).json()).problems).toEqual([])
  await expectNoHScroll(page)
})

test('the structure buttons are big enough to tap, and the page does not scroll sideways on any tool', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Phone Buttons')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  for (const r of await page.getByRole('radiogroup', { name: 'Structure' }).getByRole('radio').all()) {
    expect((await r.boundingBox())!.height).toBeGreaterThanOrEqual(36)
  }
  await tool(page, 'Select').click()
  await expectNoHScroll(page)
})
