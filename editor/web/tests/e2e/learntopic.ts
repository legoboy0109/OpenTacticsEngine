// Helpers for the topic lesson specs (learn-structures, learn-economy, learn-art, learn-hexagons): starting a lesson from the Learn
// list, the finish, and the checks at the end. The generic ones (panel, ring, expectCurrent, expectTicked...) are in learnkit.ts.
import type { Page } from '@playwright/test'
import type { Helper } from './helper'
import { expect, expectLoadsInEngine, learnButton, learnList, learnState, panel, readGame, ring, stepTitle } from './learnkit'

/** The id and name of the practice game of a lesson (the editor makes "Practice: <title>"). */
export const practiceOf = (lesson: string, title: string) => ({ id: `practice-${lesson}`, name: `Practice: ${title}` })

/** Starts a lesson from the Learn list on the game list and waits for its first step. */
export async function startLesson(page: Page, lesson: string, firstStep: string): Promise<void> {
  await page.goto('/')
  await learnButton(page).click()
  await learnList(page).locator(`[data-lesson="${lesson}"]`).getByRole('button', { name: 'Start', exact: true }).click()
  await expect(stepTitle(page)).toHaveText(firstStep)
  await expect(panel(page)).toBeVisible()
}

/** The Play step done properly: the player opens, loads, the step ticks (the caller checks that), and Play is closed again. */
export async function playAndClose(page: Page, ticked: () => Promise<void>): Promise<void> {
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await ticked()
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toHaveCount(0)
}

/** After the last step: the end card shows with its summary, Play starts from it, "Done" finishes the lesson, and the real engine
 *  loads the practice game without a single problem. */
export async function finish(page: Page, lh: Helper, lesson: string, summaryLine: string, practiceId: string): Promise<void> {
  const end = page.getByTestId('learn-end')
  await expect(end).toBeVisible()
  await expect(stepTitle(page)).toHaveCount(0)
  await expect(end).toContainText('You finished the lesson')
  await expect(end).toContainText(summaryLine)
  await expect(ring(page)).toHaveCount(0)
  await end.getByRole('button', { name: 'Play your game' }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()
  await end.getByRole('button', { name: 'Done' }).click()
  await expect(panel(page)).toHaveCount(0)
  await expect.poll(async () => (await learnState(lh)).lessons[lesson].state).toBe('finished')
  await expectLoadsInEngine(lh, practiceId)
}

/** Debug aid: with LEARN_SHOTS=<folder> a screenshot of the whole window is saved under that name (nothing otherwise). */
export async function shot(page: Page, name: string): Promise<void> {
  const dir = process.env.LEARN_SHOTS
  if (dir) await page.waitForTimeout(900) // a ring that scrolled its control into view settles first
  if (dir) await page.screenshot({ path: `${dir}/${name}.png` })
}

export { readGame }
