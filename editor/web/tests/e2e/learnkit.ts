// Helpers for the Learn specs. Every Learn test gets its OWN helper process (own games folder, own tutorial state file in a
// temp folder, starting EMPTY = a first run), so tests never share state and the real user data folder is never touched.
import { test as base, expect, type Locator, type Page } from '@playwright/test'
import { execFileSync } from 'node:child_process'
import { existsSync, readFileSync } from 'node:fs'
import path from 'node:path'
import { playerDir, startHelper, type Helper } from './helper'
import { playerBin } from './soundkit'

export { expect }

export const test = base.extend<{ lh: Helper }>({
  lh: async ({}, use) => {
    const h = await startHelper(playerDir(), 'empty')
    await use(h)
    await h.stop()
  },
  baseURL: async ({ lh }, use) => { await use(lh.url) },
})

export const LESSON = 'first-game'
export const PRACTICE = 'practice-your-first-game'
export const PRACTICE_NAME = 'Practice: Your first game'

// ---- the state the helper keeps ----

export async function learnState(lh: Helper): Promise<any> { return (await fetch(`${lh.url}/api/learn`)).json() }
export async function setLearnState(lh: Helper, state: object): Promise<void> {
  const res = await fetch(`${lh.url}/api/learn`, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(state) })
  expect(res.ok).toBe(true)
}
/** The offer was answered, so the game list shows no card (a returning maker). */
export const answered = (lh: Helper) => setLearnState(lh, { version: 1, offerAnswered: true, lessons: {} })

// ---- the page ----

export const panel = (page: Page): Locator => page.getByRole('complementary', { name: 'Lesson' })
export const offer = (page: Page): Locator => page.getByTestId('learn-offer')
export const stepTitle = (page: Page): Locator => page.getByTestId('learn-step-title')
export const ring = (page: Page): Locator => page.getByTestId('learn-ring')
export const learnButton = (page: Page): Locator => page.getByRole('button', { name: 'Learn', exact: true })
export const learnList = (page: Page): Locator => page.getByRole('dialog', { name: 'Learn' })

/** Starts the lesson from the first-run card and waits for the first step. */
export async function startFromOffer(page: Page): Promise<void> {
  await page.goto('/')
  await offer(page).getByRole('button', { name: 'Start', exact: true }).click()
  await expect(stepTitle(page)).toHaveText('Name your game')
}

/** The step is the current one (not ticked yet) ... */
export async function expectCurrent(page: Page, title: string): Promise<void> { await expect(stepTitle(page)).toHaveText(title) }
/** ... and later this one is ticked: it is in the "Done so far" list (which is folded) and the current step is another. */
export async function expectTicked(page: Page, title: string, next: string): Promise<void> {
  await expect(stepTitle(page)).toHaveText(next)
  await expect(page.getByTestId('learn-done')).toContainText(title)
}

/** The ring is on the control with this `data-learn` name: the same box, plus the ring's small margin. */
export async function expectRingOn(page: Page, name: string): Promise<void> {
  await expect(ring(page)).toHaveAttribute('data-target', name)
  await expect.poll(async () => {
    const [r, t] = await Promise.all([ring(page).boundingBox(), page.locator(`[data-learn="${name}"]:visible`).first().boundingBox()])
    if (!r || !t) return 'missing'
    const off = [r.x - (t.x - 4), r.y - (t.y - 4), r.width - (t.width + 8), r.height - (t.height + 8)].map((n) => Math.abs(n))
    return Math.max(...off) <= 1.5 ? 'on it' : `off by ${off.join(',')}`
  }, { timeout: 7000 }).toBe('on it')
}

export const sectionLink = (page: Page, name: string): Locator => page.getByRole('navigation', { name: 'Sections' }).getByRole('link', { name, exact: true })

// ---- the finished game ----

/** The real engine's check of the practice game on disk: the loader (via the helper) finds no problem and the native player starts it. */
export async function expectLoadsInEngine(lh: Helper, id: string): Promise<void> {
  const problems = (await (await fetch(`${lh.url}/api/projects/${id}/problems`)).json()).problems
  expect(problems).toEqual([])
  const bin = playerBin()
  expect(existsSync(bin), `ote-player not found at ${bin}: build it (cmake --build build) or set OTE_PLAYER_BIN`).toBe(true)
  let output = ''
  try {
    output = execFileSync(bin, ['--headless', '--no-settings', '--no-saving', '--games', lh.games, '--game', id, '--frames', '5'],
      { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], timeout: 60_000 })
  } catch (e) {
    const err = e as { stdout?: string; stderr?: string }
    throw new Error(`the native player did not start ${id}:\n${err.stdout ?? ''}${err.stderr ?? ''}`)
  }
  expect(output.split('\n').filter((l) => /\[ERROR\]|Cannot load/.test(l))).toEqual([])
}

export const readGame = (lh: Helper, id: string, rel: string): any => JSON.parse(readFileSync(path.join(lh.games, id, rel), 'utf8'))
