// The Play DIALOG is sized from the browser window (src/play/wholeFit.ts, docs/EDITOR_UI.md "Play"): its frame is exactly the largest whole
// multiple (1x, 2x, 3x...) of the game's view that fits the window minus the dialog's chrome and a margin, in device pixels; the dialog is as
// wide as the frame and as tall as header + frame + footer (no black area around the picture), it stays inside the window, the frame's canvas
// backing store is k x view (one canvas pixel = one device pixel) and the picture grows in steps when the window is resized. Judged on layout
// numbers, not on pixels of the picture. Needs the web player build (requirePlayer); the panel is muted by the unit test of the address only
// (the browser is muted by playwright.config.ts).
import type { Page } from '@playwright/test'
import { copyDemo } from './campaignkit'
import { openMap } from './mapkit'
import { expect, test } from './fixtures'
import { requirePlayer } from './fixtures'
import { OUTER_MARGIN } from '../../src/play/wholeFit'

test.beforeEach(() => requirePlayer())

const panel = (page: Page) => page.getByRole('dialog', { name: 'Play', exact: true })

interface Measure {
  dpr: number
  frame: { x: number; y: number; w: number; h: number }  // the iframe, in CSS pixels
  stage: { x: number; y: number; w: number; h: number }
  dialog: { x: number; y: number; w: number; h: number }
  win: [number, number]
  chrome: number                                           // header + footer + border heights, CSS px
  canvas: [number, number] | null                          // the canvas backing store inside the frame
}

async function measure(page: Page): Promise<Measure> {
  return page.evaluate(() => {
    const f = document.querySelector('.play-frame') as HTMLIFrameElement
    const st = document.querySelector('.play-stage') as HTMLElement
    const fr = f.getBoundingClientRect(), sr = st.getBoundingClientRect()
    const dl = document.querySelector('.play-dialog') as HTMLElement, dr = dl.getBoundingClientRect()
    const chrome = dr.height - sr.height
    const c = f.contentDocument?.getElementById('canvas') as HTMLCanvasElement | null
    return {
      dpr: window.devicePixelRatio,
      frame: { x: fr.x, y: fr.y, w: fr.width, h: fr.height }, stage: { x: sr.x, y: sr.y, w: sr.width, h: sr.height },
      dialog: { x: dr.x, y: dr.y, w: dr.width, h: dr.height }, win: [document.documentElement.clientWidth, document.documentElement.clientHeight] as [number, number], chrome,
      canvas: c ? [c.width, c.height] as [number, number] : null,
    }
  })
}

async function openPlay(page: Page, id: string): Promise<void> {
  await openMap(page, id, 'north_road')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(panel(page)).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
}

/** Waits until the layout has caught up with a resize: the frame is whole, centred and its canvas matches (the panel measures on the next frame). */
async function settles(page: Page, view: { width: number; height: number }): Promise<void> {
  let why = 'never measured'
  for (let i = 0; i < 60; i++) {
    const m = await measure(page)
    try { expectWhole(m, view); return } catch (e) { why = `${String((e as Error).message).split('\n').slice(0, 3).join(' | ')} ${JSON.stringify(m)}` }
    await page.waitForTimeout(150)
  }
  throw new Error(`the frame never settled: ${why}`)
}

/** The facts the panel promises, for one measurement. Returns k. */
function expectWhole(m: Measure, view: { width: number; height: number }): number {
  const dw = m.frame.w * m.dpr, dh = m.frame.h * m.dpr  // device pixels
  const k = Math.round(dw / view.width)
  expect(k, 'at least 1x').toBeGreaterThanOrEqual(1)
  // (a frame is laid out in whole css pixels: k x view device pixels once rounded, never short, and a pixel over at worst)
  expect(Math.round(dw) - k * view.width, 'frame width is k x view in device pixels').toBeGreaterThanOrEqual(0)
  expect(Math.round(dw) - k * view.width, 'frame width is k x view in device pixels').toBeLessThan(2)
  expect(Math.round(dh) - k * view.height, 'frame height is k x view in device pixels').toBeGreaterThanOrEqual(0)
  expect(Math.round(dh) - k * view.height, 'frame height is k x view in device pixels').toBeLessThan(2)
  // the largest that fits: one more would not fit the window minus the chrome and the 16 px margins
  const aw = Math.floor((m.win[0] - 2 * OUTER_MARGIN - (m.dialog.w - m.stage.w)) * m.dpr + 1e-6), ah = Math.floor((m.win[1] - 2 * OUTER_MARGIN - m.chrome) * m.dpr + 1e-6)
  expect((k + 1) * view.width > aw || (k + 1) * view.height > ah, `${k + 1}x would also fit ${aw}x${ah}`).toBe(true)
  expect(k * view.width <= aw && k * view.height <= ah, `${k}x fits ${aw}x${ah}`).toBe(true)
  // the dialog stays inside the window, centred
  expect(m.dialog.x, 'dialog inside the window (left)').toBeGreaterThanOrEqual(0)
  expect(m.dialog.y, 'dialog inside the window (top)').toBeGreaterThanOrEqual(0)
  expect(m.dialog.x + m.dialog.w, 'dialog inside the window (right)').toBeLessThanOrEqual(m.win[0] + 0.5)
  expect(m.dialog.y + m.dialog.h, 'dialog inside the window (bottom)').toBeLessThanOrEqual(m.win[1] + 0.5)
  // no black area: the stage is the frame (the dialog is wider than the frame only for a frame narrower than its header needs)
  expect(Math.abs(m.stage.h - m.frame.h), 'stage as tall as the frame').toBeLessThanOrEqual(1)
  if (m.frame.w >= 600) expect(Math.abs(m.stage.w - m.frame.w), 'stage as wide as the frame').toBeLessThanOrEqual(1)
  expect(Math.abs(m.frame.x + m.frame.w / 2 - (m.stage.x + m.stage.w / 2)), 'frame centred in the stage').toBeLessThanOrEqual(1)
  // one canvas pixel is one device pixel: no browser scaling inside the frame
  expect(m.canvas, 'the player drew a canvas').not.toBeNull()
  // (exactly k x view where a whole css size gives it; else a pixel or two over, never short: then the picture is still k x with that thin black edge)
  expect(m.canvas![0] - k * view.width, 'canvas backing store width = k x view').toBeGreaterThanOrEqual(0)
  expect(m.canvas![0] - k * view.width, 'canvas backing store width = k x view').toBeLessThanOrEqual(2)
  expect(m.canvas![1] - k * view.height, 'canvas backing store height = k x view').toBeGreaterThanOrEqual(0)
  expect(m.canvas![1] - k * view.height, 'canvas backing store height = k x view').toBeLessThanOrEqual(2)
  if (m.dpr !== 1.75) expect(m.canvas, 'canvas backing store = k x view exactly').toEqual([k * view.width, k * view.height])
  return k
}

// [window w, h, devicePixelRatio, the multiple the whole-number fit must give (0 = not asserted)]
for (const [w, h, dpr, expected] of [[1280, 900, 1, 2], [2560, 1401, 1, 4], [1920, 1080, 1, 3], [1500, 800, 1.25, 0], [1700, 900, 1.1, 0], [1300, 800, 1.75, 0]] as Array<[number, number, number, number]>) {
  test(`Play: the frame is a whole multiple of the view in device pixels, at ${w}x${h} @${dpr} and after a resize`, async ({ browser, request }) => {
    test.setTimeout(120_000)
    const context = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: dpr })
    const page = await context.newPage()
    try {
      const id = copyDemo()
      const doc = await (await request.get(`/api/projects/${id}`)).json()
      const view = { width: doc.game.view?.width ?? 480, height: doc.game.view?.height ?? 270 }
      await openPlay(page, id)
      const m0 = await measure(page)
      if (expected === 4 || expected === 3) expect([Math.round(m0.frame.w), Math.round(m0.frame.h)], 'frame size').toEqual([expected * 480, expected * 270])
      const k0 = expectWhole(m0, view)
      if (expected) expect(k0, `${w}x${h} @${dpr}: ${expected}x`).toBe(expected)

      // A smaller window: the frame shrinks in steps and stays whole (the player is restarted by the panel only for its own choices,
      // so the frame keeps running and its canvas follows).
      await page.setViewportSize({ width: Math.round(w * 0.7), height: Math.round(h * 0.75) })
      await settles(page, view)
      const k1 = expectWhole(await measure(page), view)
      expect(k1, 'the picture did not grow in a smaller window').toBeLessThanOrEqual(k0)

      // Back to the first size: the same frame again.
      await page.setViewportSize({ width: w, height: h })
      await settles(page, view)
      expect(expectWhole(await measure(page), view)).toBe(k0)
    } finally {
      await context.close()
    }
  })
}

test('Play: when even 1x does not fit, the frame fills the space (the player fits the picture to it)', async ({ browser }) => {
  test.setTimeout(120_000)
  const context = await browser.newContext({ viewport: { width: 420, height: 700 }, deviceScaleFactor: 1 })
  const page = await context.newPage()
  try {
    const id = copyDemo()
    await openPlay(page, id)
    const m = await measure(page)
    expect(m.frame.w).toBeCloseTo(m.stage.w, 1)
    expect(m.frame.h).toBeCloseTo(m.stage.h, 1)
  } finally {
    await context.close()
  }
})
