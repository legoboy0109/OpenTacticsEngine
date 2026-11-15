// The lesson "Economy" played through the real editor by a script that does what each step says. Each step is current (not ticked)
// before its action and ticked after it; near misses must not tick. Ends with the game loading in the real engine.
import { clickCell, pickSide, pickStructure, tab, tool } from './mapkit'
import { requirePlayer, saved } from './fixtures'
import { expect, expectCurrent, expectRingOn, expectTicked, panel, readGame, test } from './learnkit'
import { finish, playAndClose, practiceOf, shot, startLesson } from './learntopic'

test.use({ viewport: process.env.LEARN_PHONE ? { width: 390, height: 844 } : { width: 1920, height: 1080 } })
const { id: PRACTICE, name: PRACTICE_NAME } = practiceOf('economy', 'Economy')

test('@smoke Economy: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, 'economy', 'Make a structure to build units in')
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/structures$`))
  await expect(panel(page).getByText('Step 1 of 8')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)

  // 1. A new structure.
  await expectRingOn(page, 'new-item')
  await shot(page, 'economy-1')
  await page.getByRole('button', { name: 'New structure' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Barracks')
  await expectCurrent(page, 'Make a structure to build units in')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a structure to build units in', 'Choose what it builds')

  // 2. What it builds. Trap: the City is not the new structure; ticking Infantry there does nothing for the step.
  await expectRingOn(page, 'structure-produces')
  await shot(page, 'economy-2')
  const produces = page.locator('[data-learn="structure-produces"]')
  await page.getByRole('button', { name: /^City\b/ }).click()
  await produces.getByRole('checkbox', { name: 'Infantry' }).check()
  await expectCurrent(page, 'Choose what it builds')
  await page.getByRole('button', { name: /^Barracks\b/ }).click()
  await expect(produces.getByRole('checkbox', { name: 'Infantry' })).not.toBeChecked()
  await produces.getByRole('checkbox', { name: 'Infantry' }).check()
  await expectTicked(page, 'Choose what it builds', 'Give Blue a Barracks')

  // 3. On the map for Blue. Trap: a Barracks nobody owns does not tick.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/crossroads$/)
  await expectRingOn(page, 'tool-structures')
  await shot(page, 'economy-3')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Barracks').click()
  await clickCell(page, 4, 7)
  await expectCurrent(page, 'Give Blue a Barracks')
  await pickSide(page, 'Blue Army').click()
  await clickCell(page, 1, 7)
  await expectTicked(page, 'Give Blue a Barracks', 'Let a City be upgraded')

  // 4. Upgrade the City. Trap: an upgrade with no cost is not enough; the cost is typed and ticks after a pause.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/structures\/city$/)
  await expectRingOn(page, 'structure-upgrades')
  await shot(page, 'economy-4')
  await page.getByRole('button', { name: 'Add an upgrade' }).click()
  await page.getByLabel('Becomes').selectOption({ label: 'Barracks' })
  await expectCurrent(page, 'Let a City be upgraded')
  const cost = page.getByLabel('Funds Cost')
  await cost.fill('5')
  await expectCurrent(page, 'Let a City be upgraded')
  await cost.fill('500')
  await expectTicked(page, 'Let a City be upgraded', 'Add a technology')

  // 5. A technology.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/research$/)
  await expectRingOn(page, 'new-item')
  await shot(page, 'economy-5')
  await page.getByRole('button', { name: 'New technology' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Heavy Armor')
  await expectCurrent(page, 'Add a technology')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Add a technology', 'Make it unlock a unit')

  // 6. What it unlocks.
  await expectRingOn(page, 'tech-unlocks')
  await shot(page, 'economy-6')
  await expectCurrent(page, 'Make it unlock a unit')
  await page.locator('[data-learn="tech-unlocks"]').getByRole('checkbox', { name: 'Tank' }).check()
  await expectTicked(page, 'Make it unlock a unit', 'Turn on fog of war')

  // 7. Fog of war: the switch is on the Map tab. Trap: opening the tab is not switching fog on.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/crossroads$/)
  await tab(page, 'Map').click()
  await expectRingOn(page, 'fog-setting')
  await shot(page, 'economy-7')
  await expectCurrent(page, 'Turn on fog of war')
  await page.getByRole('checkbox', { name: 'Fog of war' }).check()
  await expectTicked(page, 'Turn on fog of war', 'Play your game')

  // 8. Play.
  await saved(page)
  await expectRingOn(page, 'play')
  await shot(page, 'economy-8')
  await expectCurrent(page, 'Play your game')
  await playAndClose(page, () => expect(page.getByTestId('learn-end')).toBeVisible())

  await finish(page, lh, 'economy', 'A game that has no problems and that you played.', PRACTICE)
  // What was made, on disk.
  const kinds = readGame(lh, PRACTICE, 'data/structures.json').structures
  expect(kinds.find((s: any) => s.id === 'barracks')).toMatchObject({ name: 'Barracks', produces: ['infantry'] })
  expect(kinds.find((s: any) => s.id === 'city').upgrades).toEqual([{ to: 'barracks', cost: { funds: 500 } }])
  expect(readGame(lh, PRACTICE, 'data/research.json').technologies).toMatchObject([{ id: 'heavy_armor', unlocks: { units: ['tank'] } }])
  const map = readGame(lh, PRACTICE, 'maps/crossroads.json')
  expect(map.fog).toBe(true)
  expect(map.structures.filter((s: any) => s.type === 'barracks').map((s: any) => `${s.x},${s.y},${s.side ?? ''}`).sort()).toEqual(['1,7,blue', '4,7,'])
})
