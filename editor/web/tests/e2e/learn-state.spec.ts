// The Learn feature around the lesson: the first-run offer, resume after a reload, stop and continue, skipping, a deleted
// practice game, numbering of practice games, and an editor session without a lesson.
import { rmSync } from 'node:fs'
import path from 'node:path'
import type { Page } from '@playwright/test'
import type { Helper } from './helper'
import {
  answered, expect, expectCurrent, expectTicked, learnButton, learnList, learnState, LESSON, offer, panel, PRACTICE, PRACTICE_NAME, readGame, ring,
  setLearnState, startFromOffer, stepTitle, test,
} from './learnkit'

/** A practice game made like the editor makes it, and a lesson state pointing at it. */
async function seed(lh: Helper, progress: object, id = PRACTICE, name = PRACTICE_NAME): Promise<void> {
  const res = await fetch(`${lh.url}/api/projects`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id, name, template: 'blank' }) })
  expect(res.status).toBe(201)
  await setLearnState(lh, { version: 1, offerAnswered: true, lessons: { [LESSON]: { skipped: [], signals: [], game: id, ...progress } } })
}

const nameBox = (page: Page) => page.locator('[data-learn="game-name"]')

test.describe('first-run offer', () => {
  test('@smoke "Not now" is remembered: no card after a reload, no lesson, and the lesson is still in the Learn list', async ({ page, lh }) => {
    await page.goto('/')
    await expect(offer(page)).toBeVisible()
    await expect(offer(page)).toContainText('New here? Make your first game with a short lesson.')
    await offer(page).getByRole('button', { name: 'Not now' }).click()
    await expect(offer(page)).toHaveCount(0)
    await expect(panel(page)).toHaveCount(0)
    await expect.poll(() => learnState(lh)).toMatchObject({ offerAnswered: true, lessons: {} })
    await page.reload()
    await expect(page.getByRole('heading', { name: 'Your games' })).toBeVisible()
    await expect(offer(page)).toHaveCount(0)
    await expect(panel(page)).toHaveCount(0)
    // No practice game was made.
    await expect(page.getByRole('link', { name: /Practice/ })).toHaveCount(0)

    await learnButton(page).click()
    const row = learnList(page).locator('[data-lesson="first-game"]')
    await expect(row).toContainText('Your first game')
    await expect(row).toContainText('About 10 minutes')
    await expect(row).toContainText('Not started')
    await expect(learnList(page).getByRole('listitem')).toHaveCount(1) // only lessons that exist: no "coming soon" entries
    await row.getByRole('button', { name: 'Start', exact: true }).click()
    await expect(stepTitle(page)).toHaveText('Name your game')
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/game$`))
  })

  test('"Start" makes the practice game, and the card never comes back by itself', async ({ page, lh }) => {
    await startFromOffer(page)
    await expect.poll(() => learnState(lh)).toMatchObject({ offerAnswered: true, lessons: { [LESSON]: { state: 'running', step: 0 } } })
    await page.goto('/')
    await expect(page.getByRole('link', { name: new RegExp(PRACTICE_NAME) })).toBeVisible() // an ordinary game in the list
    await expect(offer(page)).toHaveCount(0)
    await page.reload()
    await expect(offer(page)).toHaveCount(0)
  })

  test('a state the helper cannot read means no offer, and nothing breaks', async ({ page }) => {
    await page.route('**/api/learn', (route) => route.fulfill({ status: 500, contentType: 'application/json', body: '{"error":"nope"}' }))
    await page.goto('/')
    await expect(page.getByRole('heading', { name: 'Your games' })).toBeVisible()
    await expect(offer(page)).toHaveCount(0)
    await expect(learnButton(page)).toBeVisible()
  })
})

test.describe('remembered progress', () => {
  test('@smoke a lesson in progress survives a reload, stays across screens and games, and "Take me there" finds the way back', async ({ page, lh }) => {
    await startFromOffer(page)
    await nameBox(page).fill('Moonlit Valley')
    await expectTicked(page, 'Name your game', 'Add a new kind of ground')
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { step: 1 } } })

    await page.reload()
    await expect(panel(page)).toBeVisible()
    await expect(panel(page).getByText('Step 2 of 10')).toBeVisible()
    await expectCurrent(page, 'Add a new kind of ground')
    await expect(page.getByTestId('learn-done')).toContainText('Name your game')

    // The panel stays on the game list and says where the lesson lives.
    await page.getByRole('link', { name: /Back to all games/ }).click()
    await expect(page.getByRole('heading', { name: 'Your games' })).toBeVisible()
    await expect(panel(page)).toBeVisible()
    await expect(panel(page)).toContainText('This lesson happens in your practice game.')
    await expect(ring(page)).toHaveCount(0)
    await panel(page).getByRole('button', { name: 'Take me there' }).click()
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/terrain$`))
    await expect(panel(page)).not.toContainText('This lesson happens in your practice game.')
    await expect(panel(page).getByRole('button', { name: 'Take me there' })).toHaveCount(0) // already there
  })

  test('stop the lesson, then continue it from the Learn list', async ({ page, lh }) => {
    await startFromOffer(page)
    await nameBox(page).fill('Moonlit Valley')
    await expectTicked(page, 'Name your game', 'Add a new kind of ground')
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { step: 1 } } })

    await panel(page).getByRole('button', { name: 'Stop lesson' }).click()
    await expect(panel(page)).toHaveCount(0)
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { state: 'stopped', step: 1 } } })
    // The editor works as usual meanwhile.
    await expect(page.locator('html')).not.toHaveAttribute('data-learn-layout', /.*/)
    await page.reload()
    await expect(panel(page)).toHaveCount(0)

    await learnButton(page).click()
    const row = learnList(page).locator('[data-lesson="first-game"]')
    await expect(row).toContainText('In progress: step 2 of 10')
    await row.getByRole('button', { name: 'Continue' }).click()
    await expect(learnList(page)).toHaveCount(0)
    await expect(panel(page).getByText('Step 2 of 10')).toBeVisible()
    await expectCurrent(page, 'Add a new kind of ground')
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/terrain$`))
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { state: 'running', step: 1 } } })
  })

  test('"Start again" from the list makes a second, numbered practice game and leaves the first alone', async ({ page, lh }) => {
    await startFromOffer(page)
    await nameBox(page).fill('Moonlit Valley')
    await expectTicked(page, 'Name your game', 'Add a new kind of ground')
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { step: 1 } } })
    await learnButton(page).click()
    await learnList(page).getByRole('button', { name: 'Start again' }).click()
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}-2/game$`))
    await expect(panel(page).getByText('Step 1 of 10')).toBeVisible()
    await expect(nameBox(page)).toHaveValue('Practice: Your first game 2')
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { state: 'running', step: 0, skipped: [], signals: [], game: `${PRACTICE}-2` } } })
    expect(readGame(lh, PRACTICE, 'game.json').name).toBe('Moonlit Valley') // the first practice game is untouched
  })

  test('skipping a step moves on and is remembered; a lesson finished with skipped steps counts as finished', async ({ page, lh }) => {
    await seed(lh, { state: 'running', step: 8, skipped: ['name', 'paint'], signals: ['played'] })
    await page.goto(`/p/${PRACTICE}/game`)
    await expectCurrent(page, 'Check for problems')
    await page.getByTestId('learn-done').locator('summary').click()
    await expect(page.getByTestId('learn-done')).toContainText('Name your game (skipped)')
    await expect(page.getByTestId('learn-done')).toContainText('Play your game')
    await expect(page.getByTestId('learn-done')).not.toContainText('Play your game (skipped)')
    await page.getByRole('button', { name: 'No problems' }).click()
    await expect(page.getByTestId('learn-end')).toBeVisible()
    await page.getByTestId('learn-end').getByRole('button', { name: 'Done' }).click()
    await expect(panel(page)).toHaveCount(0)
    await expect.poll(async () => (await learnState(lh)).lessons[LESSON]).toMatchObject({ state: 'finished', skipped: ['name', 'paint'] })
    await learnButton(page).click()
    await expect(learnList(page).locator('[data-lesson="first-game"]')).toContainText('Finished')
  })

  test('"Skip this step" on the first step', async ({ page, lh }) => {
    await startFromOffer(page)
    await panel(page).getByRole('button', { name: 'Skip this step' }).click()
    await expectCurrent(page, 'Add a new kind of ground')
    await expect(panel(page).getByText('Step 2 of 10')).toBeVisible()
    await expect.poll(() => learnState(lh)).toMatchObject({ lessons: { [LESSON]: { step: 1, skipped: ['name'] } } })
  })

  test('a step that is already true when it is reached ticks at once', async ({ page, lh }) => {
    // The maker already renamed the game and made a terrain before the panel got to those steps.
    await seed(lh, { state: 'running', step: 0 })
    const game = await (await fetch(`${lh.url}/api/projects/${PRACTICE}`)).json()
    game.game.name = 'Already named'
    game.terrain.push({ id: 'water', name: 'Water', color: '#3366cc', glyph: '~' })
    const put = await fetch(`${lh.url}/api/projects/${PRACTICE}`, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ baseRevision: game.revision, game: game.game, terrain: game.terrain }) })
    expect(put.ok).toBe(true)
    await page.goto(`/p/${PRACTICE}/game`)
    await expectCurrent(page, 'Paint it on the map') // steps 1 and 2 ticked by themselves (after the name's short pause)
  })
})

test.describe('the practice game is gone', () => {
  test('@smoke a running lesson says so, and "Start again" makes a new one', async ({ page, lh }) => {
    await startFromOffer(page)
    rmSync(path.join(lh.games, PRACTICE), { recursive: true, force: true })
    await page.goto('/')
    await expect(page.getByTestId('learn-gone')).toBeVisible()
    await expect(page.getByTestId('learn-gone')).toContainText('Your practice game is gone')
    await expect(ring(page)).toHaveCount(0)
    await page.getByTestId('learn-gone').getByRole('button', { name: 'Start again' }).click()
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/game$`))
    await expect(page.getByTestId('learn-gone')).toHaveCount(0)
    await expectCurrent(page, 'Name your game')
    await expect(panel(page).getByText('Step 1 of 10')).toBeVisible()
  })

  test('opening the deleted game directly says so too', async ({ page, lh }) => {
    await startFromOffer(page)
    rmSync(path.join(lh.games, PRACTICE), { recursive: true, force: true })
    await page.goto(`/p/${PRACTICE}/game`)
    await expect(page.getByRole('heading', { name: 'Could not open this game' })).toBeVisible()
    await expect(page.getByTestId('learn-gone')).toBeVisible()
  })

  test('Continue on a stopped lesson whose game was deleted offers Start again instead', async ({ page, lh }) => {
    await seed(lh, { state: 'stopped', step: 3 })
    rmSync(path.join(lh.games, PRACTICE), { recursive: true, force: true })
    await page.goto('/')
    await learnButton(page).click()
    const row = learnList(page).locator('[data-lesson="first-game"]')
    await row.getByRole('button', { name: 'Continue' }).click()
    await expect(row).toContainText('The practice game for “Your first game” is gone')
    await expect(row.getByRole('button', { name: 'Continue' })).toHaveCount(0)
    await row.getByRole('button', { name: 'Start again' }).click()
    await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/game$`))
    await expect(panel(page).getByText('Step 1 of 10')).toBeVisible()
  })
})

test.describe('an editor session without a lesson', () => {
  test('looks and behaves as before: only the Learn button is new', async ({ page, lh }) => {
    await answered(lh)
    await page.request.post('/api/projects', { data: { id: 'plain-one', name: 'Plain One', template: 'blank' } })
    await page.goto('/p/plain-one/maps/start')
    await expect(page.getByTestId('map-canvas')).toBeVisible()
    await expect(panel(page)).toHaveCount(0)
    await expect(ring(page)).toHaveCount(0)
    await expect(page.locator('html')).not.toHaveAttribute('data-learn-layout', /.*/)
    const root = await page.evaluate(() => { const s = getComputedStyle(document.getElementById('root')!); return [s.marginRight, s.paddingBottom] })
    expect(root).toEqual(['0px', '0px'])
    await expect(learnButton(page)).toBeVisible()
    await expect(offer(page)).toHaveCount(0)
    // Nothing was written to the tutorial state by just using the editor.
    expect(await learnState(lh)).toEqual({ version: 1, offerAnswered: true, lessons: {} })
    // Play, problems, undo keep working as usual (their buttons carry only the new pointing names).
    await expect(page.getByRole('button', { name: 'Play', exact: true })).toBeEnabled()
    await expect(page.getByRole('button', { name: 'No problems' })).toBeVisible()
  })

  test('the Learn button is on every screen of a game and on the game list', async ({ page, lh }) => {
    await answered(lh)
    await page.request.post('/api/projects', { data: { id: 'plain-two', name: 'Plain Two', template: 'blank' } })
    await page.goto('/')
    await expect(learnButton(page)).toBeVisible()
    for (const section of ['game', 'rules', 'units', 'weapons', 'items', 'terrain', 'structures', 'research', 'art', 'sound', 'maps', 'campaign']) {
      await page.goto(`/p/plain-two/${section}`)
      await expect(learnButton(page), section).toBeVisible()
    }
  })
})
