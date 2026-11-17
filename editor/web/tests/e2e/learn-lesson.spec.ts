// "Your first game" played through the real editor by a script that does what each step asks. For every step: before the
// action the step is the current one (not ticked); after it, it is ticked and the next one is current. Ends with the practice
// game on disk loading in the real engine.
import { clickCell, dragThrough, pickSide, pickTerrain, pickUnitType, row, tool } from './mapkit'
import { gotoSection, requirePlayer, saved } from './fixtures'
import {
  expect, expectCurrent, expectLoadsInEngine, expectRingOn, expectTicked, learnButton, learnList, learnState, LESSON, panel, PRACTICE, PRACTICE_NAME, readGame,
  ring, startFromOffer, stepTitle, test,
} from './learnkit'

// A wide window: the panel docks at the side (below 1800 px it is a bottom sheet, tested in learn-layout.spec.ts).
test.use({ viewport: { width: 1920, height: 1080 } })

test('@smoke the whole lesson: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startFromOffer(page)
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/game$`))
  await expect(panel(page)).toBeVisible()
  await expect(panel(page).getByText('Step 1 of 10')).toBeVisible()
  expect(await learnState(lh)).toMatchObject({ offerAnswered: true, lessons: { [LESSON]: { state: 'running', step: 0, game: PRACTICE } } })
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)

  // 1. Name your game. The tick waits for a short pause, so typing is not cut off at the first letter.
  await expectRingOn(page, 'game-name')
  await page.locator('[data-learn="game-name"]').fill('Moon')
  await expectCurrent(page, 'Name your game')
  await page.locator('[data-learn="game-name"]').fill('Moonlit Valley')
  await expectCurrent(page, 'Name your game')
  await expectTicked(page, 'Name your game', 'Add a new kind of ground')

  // 2. New terrain: the ring first points at the sidebar link, then at "New terrain" once the screen is open.
  await expectRingOn(page, 'nav-terrain')
  await gotoSection(page, 'Terrain')
  await expectRingOn(page, 'new-item')
  await page.getByRole('button', { name: 'New terrain' }).click()
  await expectCurrent(page, 'Add a new kind of ground') // the dialog is open, nothing made yet
  await page.getByRole('dialog').getByRole('textbox').fill('Water')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Add a new kind of ground', 'Paint it on the map')

  // 3. Paint it: "Take me there" opens the first map; picking the terrain is not painting.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/start$/)
  await expectRingOn(page, 'terrain-picker')
  await pickTerrain(page, 'Water').click()
  await expectCurrent(page, 'Paint it on the map')
  await dragThrough(page, row(0, 0, 2))
  await expectCurrent(page, 'Paint it on the map') // three squares are not enough
  await dragThrough(page, row(1, 0, 1))
  await expectTicked(page, 'Paint it on the map', 'Make a new unit')

  // 4. A new unit type.
  await gotoSection(page, 'Units')
  await page.getByRole('button', { name: 'New unit' }).click()
  await expectCurrent(page, 'Make a new unit')
  await page.getByRole('dialog').getByRole('textbox').fill('Archer')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a new unit', 'Make a weapon')

  // 5. A new weapon.
  await gotoSection(page, 'Weapons')
  await page.getByRole('button', { name: 'New weapon' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Bow')
  await expectCurrent(page, 'Make a weapon')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a weapon', 'Give your unit the weapon')

  // 6. Give the weapon: "Take me there" opens the new unit (not the soldier); the ring is on the Weapons list.
  // Trap: with the Soldier open the ring is on the new unit in the list, not on the Soldier's weapons, and a weapon ticked
  // for the Soldier does not count.
  await gotoSection(page, 'Units')
  await page.getByRole('button', { name: /^Soldier/ }).click()
  await expect(page).toHaveURL(/\/units\/soldier$/)
  await expect(ring(page)).toHaveAttribute('data-entry', 'archer')
  await expect.poll(async () => {
    const [r, t] = await Promise.all([ring(page).boundingBox(), page.locator('[data-learn-item="archer"]').boundingBox()])
    return !!r && !!t && Math.abs(r.x - (t.x - 4)) <= 1.5 && Math.abs(r.y - (t.y - 4)) <= 1.5
  }).toBe(true)
  await page.locator('[data-learn="unit-weapons"]').getByRole('checkbox', { name: 'Bow' }).check()
  await expectCurrent(page, 'Give your unit the weapon')
  await page.locator('[data-learn="unit-weapons"]').getByRole('checkbox', { name: 'Bow' }).uncheck()
  // The same with no unit in the address: the screen opens the first one (the Soldier) by itself.
  await gotoSection(page, 'Game')
  await gotoSection(page, 'Units')
  await expect(page).toHaveURL(/\/units$/)
  await expect.poll(async () => {
    const [r, t] = await Promise.all([ring(page).boundingBox(), page.locator('[data-learn-item="archer"]').boundingBox()])
    return !!r && !!t && Math.abs(r.x - (t.x - 4)) <= 1.5 && Math.abs(r.y - (t.y - 4)) <= 1.5
  }).toBe(true)
  await page.locator('[data-learn-item="archer"]').click()
  await expect(page).toHaveURL(/\/units\/archer$/)
  await expectRingOn(page, 'unit-weapons')
  await expectCurrent(page, 'Give your unit the weapon')
  await page.locator('[data-learn="unit-weapons"]').getByRole('checkbox', { name: 'Bow' }).check()
  await expectTicked(page, 'Give your unit the weapon', 'Put your unit on the map for Blue')

  // 7. Blue: a soldier is not the new unit and does not tick the step; the new unit does.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/start$/)
  await expectRingOn(page, 'tool-units')
  await tool(page, 'Units').click()
  await pickSide(page, 'Blue').click()
  await pickUnitType(page, 'Soldier').click()
  await clickCell(page, 0, 3)
  await expectCurrent(page, 'Put your unit on the map for Blue')
  await pickUnitType(page, 'Archer').click()
  await clickCell(page, 0, 4)
  await expectTicked(page, 'Put your unit on the map for Blue', 'Put one on the map for Red too')

  // 8. Red: choosing the side alone does not tick.
  await pickSide(page, 'Red').click()
  await expectCurrent(page, 'Put one on the map for Red too')
  await clickCell(page, 7, 5)
  await expectTicked(page, 'Put one on the map for Red too', 'Play your game')

  // 9. Play: the step ticks when the player says it is ready.
  await saved(page)
  await expectRingOn(page, 'play')
  await expectCurrent(page, 'Play your game')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expectTicked(page, 'Play your game', 'Check for problems')
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toHaveCount(0)

  // 10. Problems: opening the list is the action.
  await expectRingOn(page, 'problems')
  await expectCurrent(page, 'Check for problems')
  await page.getByRole('button', { name: 'No problems' }).click()
  await expect(page.getByTestId('learn-end')).toBeVisible()
  await expect(stepTitle(page)).toHaveCount(0)
  await expect(page.getByTestId('learn-end')).toContainText('You finished the lesson')
  await expect(page.getByTestId('learn-end')).toContainText('A game that has no problems and that you played.')
  await expect(ring(page)).toHaveCount(0)
  await expect.poll(async () => (await learnState(lh)).lessons[LESSON]).toMatchObject({ state: 'running', step: 10, skipped: [], signals: expect.arrayContaining(['played', 'problemsChecked']) })

  // The end: Play again from the panel, then Done. The lesson counts as finished.
  await page.getByRole('button', { name: 'Close problems' }).click()
  await page.getByTestId('learn-end').getByRole('button', { name: 'Play your game' }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await page.getByRole('dialog', { name: 'Play', exact: true }).getByRole('button', { name: 'Close' }).click()
  await page.getByTestId('learn-end').getByRole('button', { name: 'Done' }).click()
  await expect(panel(page)).toHaveCount(0)
  await expect.poll(async () => (await learnState(lh)).lessons[LESSON].state).toBe('finished')
  await learnButton(page).click()
  await expect(learnList(page).locator('[data-lesson="first-game"]')).toContainText('Finished')
  await expect(learnList(page).getByRole('button', { name: 'Start again' })).toBeVisible()

  // What was made, on disk, loaded by the real engine.
  await saved(page)
  const game = readGame(lh, PRACTICE, 'game.json')
  expect(game.name).toBe('Moonlit Valley')
  expect(readGame(lh, PRACTICE, 'data/terrain.json').terrain.map((t: any) => t.id)).toEqual(['plain', 'wall', 'water'])
  expect(readGame(lh, PRACTICE, 'data/units.json').units.find((u: any) => u.id === 'archer').weapons).toEqual(['bow'])
  const map = readGame(lh, PRACTICE, 'maps/start.json')
  expect(map.units.filter((u: any) => u.type === 'archer').map((u: any) => u.side).sort()).toEqual(['blue', 'red'])
  expect(map.tiles.join('').split('').filter((c: string) => map.legend[c] === 'water').length).toBe(5)
  await expectLoadsInEngine(lh, PRACTICE)
})
