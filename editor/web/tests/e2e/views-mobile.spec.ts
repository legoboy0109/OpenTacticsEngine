// The Heights tool and the styles editor on a phone (360 px, touch). Chromium only.
import type { CDPSession, Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { canvas, cellPoint, openMap, tool } from './mapkit'

test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })
test.skip(({ browserName }) => browserName !== 'chromium', 'touch emulation is Chromium-only')

type Pt = { x: number; y: number }
const send = (c: CDPSession, type: 'touchStart' | 'touchMove' | 'touchEnd', pts: Pt[]) => c.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map((p, i) => ({ x: p.x, y: p.y, id: i })) })
async function touchDrag(page: Page, cells: Array<[number, number]>): Promise<void> {
  const c = await page.context().newCDPSession(page)
  const pts = []
  for (const [x, y] of cells) pts.push(await cellPoint(page, x, y))
  await send(c, 'touchStart', [pts[0]])
  for (const p of pts.slice(1)) await send(c, 'touchMove', [p])
  await send(c, 'touchEnd', [])
}

test('Heights by touch: a tap raises one square, a drag several; the tool fits the phone', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'PhoneHeights')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Heights').click()
  await expect(page.getByTestId('heights-tool')).toBeVisible()
  await expectNoHScroll(page)
  await canvas(page).scrollIntoViewIfNeeded()
  const c = await page.context().newCDPSession(page)
  const p = await cellPoint(page, 2, 2)
  await send(c, 'touchStart', [p]); await send(c, 'touchEnd', [])
  await expect.poll(() => readDisk(id, 'maps/crossroads.json').heights?.[2]?.[2]).toBe(1)
  await touchDrag(page, [[4, 4], [5, 4], [6, 4]])
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').heights[4].slice(3, 8)).toEqual([0, 1, 1, 1, 0])
  await expectNoHScroll(page)
})

test('the kind of view of a style on a phone: no sideways scrolling', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'PhoneStyles')
  await page.goto(editorUrl(id, 'game'))
  await page.getByLabel('Isometric: Kind of view').scrollIntoViewIfNeeded()
  await page.locator('.subcard').nth(1).getByText('Advanced').click()
  await expect(page.getByLabel('Isometric: Square width')).toBeVisible()
  await expectNoHScroll(page)
})
