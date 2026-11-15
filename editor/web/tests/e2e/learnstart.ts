// Helpers for the topic lessons' specs (items, story, sound): start a lesson from the Learn list like a maker who already
// answered the first-run offer, point checks for steps that name alternatives ("add-event|map-tab-events"), and screenshots.
// Everything else comes from learnkit.ts. Each test has its own helper process and temp tutorial state (learnkit's `test`).
import type { Page } from '@playwright/test'
import type { Helper } from './helper'
import { answered, expect, learnButton, learnList, panel, ring, stepTitle } from './learnkit'

/** Opens the Learn list, starts the lesson and waits for its first step. Returns the practice game's id. */
export async function startLesson(page: Page, lh: Helper, lessonId: string, firstTitle: string, practiceId: string): Promise<string> {
  await answered(lh)
  await page.goto('/')
  await learnButton(page).click()
  await learnList(page).locator(`[data-lesson="${lessonId}"]`).getByRole('button', { name: 'Start', exact: true }).click()
  await expect(page).toHaveURL(new RegExp(`/p/${practiceId}/`))
  await expect(panel(page)).toBeVisible()
  await expect(stepTitle(page)).toHaveText(firstTitle)
  return practiceId
}

/** The ring is up with these alternatives (`names` as the lesson wrote them, "a|b") and sits on `actual`, the one that is on screen. */
export async function expectRingOnOneOf(page: Page, names: string, actual: string): Promise<void> {
  await expect(ring(page)).toHaveAttribute('data-target', names)
  await expect.poll(async () => {
    const [r, t] = await Promise.all([ring(page).boundingBox(), page.locator(`[data-learn="${actual}"]:visible`).first().boundingBox()])
    if (!r || !t) return 'missing'
    const off = [r.x - (t.x - 4), r.y - (t.y - 4), r.width - (t.width + 8), r.height - (t.height + 8)].map((n) => Math.abs(n))
    return Math.max(...off) <= 1.5 ? 'on it' : `off by ${off.join(',')}`
  }, { timeout: 7000 }).toBe('on it')
}

/** A screenshot into $OTE_SHOT_DIR when it is set (for looking at the steps by eye); nothing otherwise. With OTE_SHOT_PHONE=1 the
 *  window is made phone-sized (390 x 844) for a second picture, "phone-<name>", and put back. */
export async function shot(page: Page, name: string): Promise<void> {
  const dir = process.env.OTE_SHOT_DIR
  if (!dir) return
  await page.screenshot({ path: `${dir}/${name}.png` })
  if (!process.env.OTE_SHOT_PHONE) return
  const wide = page.viewportSize()!
  await page.setViewportSize({ width: 390, height: 844 })
  await page.waitForTimeout(900) // the layout settles and the ring scrolls its control into view
  await page.screenshot({ path: `${dir}/phone-${name}.png` })
  await page.setViewportSize(wide)
  await page.waitForTimeout(300)
}
