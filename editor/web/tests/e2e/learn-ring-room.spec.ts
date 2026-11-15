// Two things the topic lessons needed from the pointing ring and the lesson sheet (src/learn/Highlight.tsx, styles/learn.css):
// 1. a ring around a control inside the picture chooser (a modal dialog) is drawn above the dialog, not hidden behind it;
// 2. on a phone the long map screen can be scrolled far enough to bring its options above the lesson sheet, and the ring never
//    points at a control that is hidden under the sheet.
import { picker } from './artkit'
import { expect, expectRingOn, learnState, panel, ring, setLearnState, test } from './learnkit'
import type { Helper } from './helper'

/** A practice game of the Art lesson, made like the editor makes it, with the lesson running at step `step`. */
async function seedArt(lh: Helper, step: number): Promise<string> {
  const id = 'practice-art'
  const res = await fetch(`${lh.url}/api/projects`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id, name: 'Practice: Art', template: 'blank' }) })
  expect(res.status).toBe(201)
  await setLearnState(lh, { version: 1, offerAnswered: true, lessons: { art: { state: 'running', step, skipped: [], signals: [], game: id } } })
  return id
}

test.describe('wide window', () => {
  test.use({ viewport: { width: 1920, height: 1080 } })
  test('the ring around the piece grid of the picture chooser is in the top layer while the dialog is open, and a plain ring after', async ({ page, lh }) => {
    const id = await seedArt(lh, 1) // "Open the picture chooser"
    await page.goto(`/p/${id}/terrain/plain`)
    await expectRingOn(page, 'picture-choose')
    expect(await ring(page).evaluate((el) => el.matches(':popover-open'))).toBe(false) // no dialog: an ordinary ring under the panel
    await page.getByRole('button', { name: 'Change picture…' }).click()
    await expect(picker(page)).toBeVisible()
    await expect(panel(page).getByText('Pick pieces that join up')).toBeVisible()
    await expectRingOn(page, 'picker-grid')
    expect(await ring(page).evaluate((el) => el.matches(':popover-open'))).toBe(true) // above the modal dialog
    const [r, d] = await Promise.all([ring(page).boundingBox(), picker(page).boundingBox()])
    expect(r!.x).toBeGreaterThanOrEqual(d!.x - 1)
    expect(r!.y + r!.height).toBeLessThanOrEqual(d!.y + d!.height + 1)
    await picker(page).getByRole('button', { name: 'Cancel' }).click()
    await expect(picker(page)).toHaveCount(0)
    await expect.poll(() => ring(page).evaluate((el) => el.matches(':popover-open'))).toBe(false)
  })
})

test.describe('phone', () => {
  test.use({ viewport: { width: 390, height: 844 } })
  test('the map options can be scrolled above the lesson sheet, and the ring is on them there, not under the sheet', async ({ page, lh }) => {
    const id = await seedArt(lh, 3) // "Paint a road"
    await page.goto(`/p/${id}/maps/start`)
    await expect(panel(page)).toBeVisible()
    await expect(ring(page)).toHaveAttribute('data-target', 'terrain-picker')
    await expect.poll(async () => {
      const [r, p] = await Promise.all([ring(page).boundingBox(), panel(page).boundingBox()])
      return r && p ? r.y + r.height <= p.y + 1 : false
    }, { timeout: 8000 }).toBe(true)
    // The end of the page: the last control of the map options ends above the sheet.
    await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight))
    const bottom = await page.evaluate(() => document.querySelector('.map-panel')!.getBoundingClientRect().bottom)
    const sheetTop = (await panel(page).boundingBox())!.y
    expect(bottom).toBeLessThanOrEqual(sheetTop + 1)
    expect((await learnState(lh)).lessons.art).toMatchObject({ state: 'running', step: 3 })
  })
})
