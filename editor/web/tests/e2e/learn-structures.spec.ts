// The lesson "Structures" played through the real editor by a script that does what each step says. Each step is current (not ticked)
// before its action and ticked after it; near misses must not tick. Ends with the game loading in the real engine.
import { clickCell, pickSide, pickStructure, tool } from './mapkit'
import { requirePlayer, saved } from './fixtures'
import { expect, expectCurrent, expectRingOn, expectTicked, learnState, panel, readGame, test } from './learnkit'
import { finish, playAndClose, practiceOf, shot, startLesson } from './learntopic'

test.use({ viewport: process.env.LEARN_PHONE ? { width: 390, height: 844 } : { width: 1920, height: 1080 } })
const { id: PRACTICE, name: PRACTICE_NAME } = practiceOf('structures', 'Structures')

test('@smoke Structures: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, 'structures', 'Make a structure')
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/structures$`))
  await expect(panel(page).getByText('Step 1 of 6')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)
  expect(readGame(lh, PRACTICE, 'data/structures.json').structures.map((s: any) => s.id)).toEqual(['city', 'factory', 'hq'])

  // 1. A new structure. The dialog being open is not the work.
  await expectRingOn(page, 'new-item')
  await shot(page, 'structures-1')
  await page.getByRole('button', { name: 'New structure' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Mine')
  await expectCurrent(page, 'Make a structure')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a structure', 'Let it be captured')

  // 2. Can be captured. Trap: the City already can be captured; opening it ticks nothing.
  await expectRingOn(page, 'structure-capture')
  await shot(page, 'structures-2')
  await page.getByRole('button', { name: /^City\b/ }).click()
  await expectCurrent(page, 'Let it be captured')
  await page.getByRole('button', { name: /^Mine\b/ }).click()
  await expectRingOn(page, 'structure-capture')
  await shot(page, 'structures-3')
  await expect(page.getByRole('checkbox', { name: 'Can be captured' })).not.toBeChecked()
  await page.getByRole('checkbox', { name: 'Can be captured' }).check()
  await expectTicked(page, 'Let it be captured', 'Make it earn funds')

  // 3. Income. A zero is not income; typing waits for a pause so "5" of "500" does not tick it.
  await expectRingOn(page, 'structure-income')
  await shot(page, 'structures-4')
  const funds = page.getByLabel('Funds Per turn')
  await funds.fill('0')
  await expectCurrent(page, 'Make it earn funds')
  await funds.fill('500')
  await expectCurrent(page, 'Make it earn funds')
  await expectTicked(page, 'Make it earn funds', 'Put it on the map')

  // 4. Place it. Trap: a City placed with the same tool is not the new structure.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/crossroads$/)
  await expectRingOn(page, 'tool-structures')
  await shot(page, 'structures-5')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'City').click()
  await clickCell(page, 4, 7)
  await expectCurrent(page, 'Put it on the map')
  await pickStructure(page, 'Mine').click()
  await expectCurrent(page, 'Put it on the map') // picking is not placing
  await clickCell(page, 0, 7)
  await expectTicked(page, 'Put it on the map', 'Give one to Blue')

  // 5. Give one to Blue. Trap: an owner chosen but nothing placed with it, and a Red one, do not tick.
  await expectRingOn(page, 'owner-picker')
  await shot(page, 'structures-6')
  await pickSide(page, 'Red Army').click()
  await clickCell(page, 2, 7)
  await expectCurrent(page, 'Give one to Blue')
  await pickSide(page, 'Blue Army').click()
  await expectCurrent(page, 'Give one to Blue')
  await clickCell(page, 1, 7)
  await expectTicked(page, 'Give one to Blue', 'Play your game')

  // 6. Play.
  await saved(page)
  await expectRingOn(page, 'play')
  await shot(page, 'structures-7')
  await expectCurrent(page, 'Play your game')
  await playAndClose(page, () => expect(page.getByTestId('learn-end')).toBeVisible()) // the last step ticked: the end card is up

  await finish(page, lh, 'structures', 'A game that has no problems and that you played.', PRACTICE)
  // What was made, on disk.
  const kinds = readGame(lh, PRACTICE, 'data/structures.json').structures
  expect(kinds.find((s: any) => s.id === 'mine')).toMatchObject({ name: 'Mine', capturePoints: 20, income: { funds: 500 } })
  const placed = readGame(lh, PRACTICE, 'maps/crossroads.json').structures.filter((s: any) => s.type === 'mine')
  expect(placed.map((s: any) => `${s.x},${s.y},${s.side ?? ''}`).sort()).toEqual(['0,7,', '1,7,blue', '2,7,red'])
  expect((await learnState(lh)).lessons.structures).toMatchObject({ state: 'finished' })
})
