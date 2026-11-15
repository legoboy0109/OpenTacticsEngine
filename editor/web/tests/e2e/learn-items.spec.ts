// The lesson "Items" played through the real editor by a script that does what each step asks. For every step: before the action
// the step is the current one (not ticked); after it, it is ticked and the next one is current. Ends with the practice game on
// disk loading in the real engine.
import { requirePlayer, saved } from './fixtures'
import {
  expect, expectCurrent, expectLoadsInEngine, expectRingOn, expectTicked, learnState, panel, readGame, ring, sectionLink, stepTitle, test,
} from './learnkit'
import { shot, startLesson } from './learnstart'

const PRACTICE = 'practice-items'
test.use({ viewport: { width: 1920, height: 1080 } })

test('@smoke the whole lesson: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, lh, 'items', 'Make an item', PRACTICE)
  await expect(panel(page).getByText('Step 1 of 6')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe('Practice: Items')
  expect(await learnState(lh)).toMatchObject({ lessons: { items: { state: 'running', step: 0, game: PRACTICE } } })

  // 1. Make an item: starting the lesson already opened the Items screen, so the ring points at "New item"
  // (and at the sidebar link once the maker is on another screen).
  await expectRingOn(page, 'new-item')
  await shot(page, 'items-1')
  await sectionLink(page, 'Game').click()
  await expectRingOn(page, 'nav-items')
  await sectionLink(page, 'Items').click()
  await expectRingOn(page, 'new-item')
  await page.getByRole('button', { name: 'New item' }).click()
  await expectCurrent(page, 'Make an item') // the dialog is open, nothing made yet
  await page.getByRole('dialog').getByLabel('Name of the item').fill('Potion')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make an item', 'Choose how much it heals')

  // 2. Heal: changing something else (the uses) is not the work; a different amount is, once typing has paused.
  await expectRingOn(page, 'item-heal')
  await shot(page, 'items-2')
  await page.getByLabel('Number of uses').fill('5')
  await page.waitForTimeout(1600)
  await expectCurrent(page, 'Choose how much it heals')
  await page.getByLabel('Amount of health restored').fill('1')
  await page.getByLabel('Amount of health restored').fill('15')
  await expectCurrent(page, 'Choose how much it heals') // the tick waits for a pause in typing
  await expectTicked(page, 'Choose how much it heals', 'Give it to your soldiers')

  // 3. Give: "Take me there" opens the Soldier; looking at it is not giving.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/units\/soldier$/)
  await expectRingOn(page, 'unit-start-items')
  await shot(page, 'items-3')
  await expectCurrent(page, 'Give it to your soldiers')
  await page.getByLabel('Starting items: Add an item…').selectOption({ label: 'Potion' })
  await expectTicked(page, 'Give it to your soldiers', 'Let it drop')

  // 4. Drop: the item is on the unit, which is not yet a drop.
  await expectRingOn(page, 'unit-start-items')
  await expectCurrent(page, 'Let it drop')
  await shot(page, 'items-4')
  await page.getByRole('checkbox', { name: 'Drops when defeated' }).check()
  await expectTicked(page, 'Let it drop', 'Play it')

  // 5. Play: the step ticks when the player says it is ready.
  await saved(page)
  await expectRingOn(page, 'play')
  await expectCurrent(page, 'Play it')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expectTicked(page, 'Play it', 'Check for problems')
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toHaveCount(0)

  // 6. Problems: opening the list is the action.
  await expectRingOn(page, 'problems')
  await expectCurrent(page, 'Check for problems')
  await page.getByRole('button', { name: 'No problems' }).click()
  await expect(page.getByTestId('learn-end')).toBeVisible()
  await expect(stepTitle(page)).toHaveCount(0)
  await expect(page.getByTestId('learn-end')).toContainText('You finished the lesson')
  await expect(page.getByTestId('learn-end')).toContainText('A drop: the one who defeats a soldier gets its item.')
  await expect(ring(page)).toHaveCount(0)
  await shot(page, 'items-end')

  // What was made, on disk, loaded by the real engine.
  await saved(page)
  expect(readGame(lh, PRACTICE, 'data/items.json').items).toEqual([{ id: 'potion', name: 'Potion', kind: 'usable', uses: 5, heal: 15 }])
  expect(readGame(lh, PRACTICE, 'data/units.json').units[0].items).toEqual([{ item: 'potion', drop: true }])
  await expectLoadsInEngine(lh, PRACTICE)
})
