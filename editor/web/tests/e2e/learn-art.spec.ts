// The lesson "Art" played through the real editor by a script that does what each step says. Each step is current (not ticked)
// before its action and ticked after it; near misses must not tick. Ends with the game loading in the real engine.
import { clickPiece, picker, pickerGrid } from './artkit'
import { clickCell, dragThrough, pickSide, pickTerrain, pickUnitType, row, tool } from './mapkit'
import { gotoSection, requirePlayer, saved } from './fixtures'
import { expect, expectCurrent, expectRingOn, expectTicked, panel, readGame, test } from './learnkit'
import { finish, playAndClose, practiceOf, shot, startLesson } from './learntopic'

test.use({ viewport: process.env.LEARN_PHONE ? { width: 390, height: 844 } : { width: 1920, height: 1080 } })
const { id: PRACTICE, name: PRACTICE_NAME } = practiceOf('art', 'Art')
const useIt = (page: import('@playwright/test').Page) => picker(page).getByRole('button', { name: 'Use this picture' })
const kind = (page: import('@playwright/test').Page, name: string) => picker(page).getByRole('radio', { name })

test('@smoke Art: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, 'art', 'Add a road')
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/terrain$`))
  await expect(panel(page).getByText('Step 1 of 8')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)

  // 1. A new terrain.
  await expectRingOn(page, 'new-item')
  await shot(page, 'art-1')
  await page.getByRole('button', { name: 'New terrain' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Road')
  await expectCurrent(page, 'Add a road')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Add a road', 'Open the picture chooser')

  // 2. Open the chooser. Trap: visiting the Art screen is not opening the chooser.
  await expectRingOn(page, 'picture-choose')
  await shot(page, 'art-2')
  await gotoSection(page, 'Art')
  await expectCurrent(page, 'Open the picture chooser')
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/terrain\/road$/)
  await expectRingOn(page, 'picture-choose')
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await expect(picker(page)).toBeVisible()
  await expectTicked(page, 'Open the picture chooser', 'Pick pieces that join up')

  // 3. Joining pieces. Trap: "One piece" is a picture, but not one that joins up, so the step stays.
  await expectRingOn(page, 'picker-grid')
  await shot(page, 'art-3')
  await clickPiece(page, pickerGrid(page), 0, 2)
  await useIt(page).click()
  await expect(picker(page)).toBeHidden()
  await expectCurrent(page, 'Pick pieces that join up')
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await kind(page, 'Joins with neighbours').click()
  await expectCurrent(page, 'Pick pieces that join up') // choosing the kind is not choosing the pieces
  await picker(page).getByRole('button', { name: 'Clear' }).click() // the earlier piece stays chosen when the kind changes
  await clickPiece(page, pickerGrid(page), 0, 2)
  await expect(page.getByTestId('chosen-summary')).toHaveText('0 across, 2 down'.replace('0 across', '1 across').replace('2 down', '3 down'))
  await shot(page, 'art-3b')
  await useIt(page).click()
  await expectTicked(page, 'Pick pieces that join up', 'Paint a road')

  // 4. Paint. Trap: four squares are not enough.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/start$/)
  await expectRingOn(page, 'terrain-picker')
  await shot(page, 'art-4')
  await pickTerrain(page, 'Road').click()
  await expectCurrent(page, 'Paint a road')
  await dragThrough(page, row(2, 0, 3))
  await expectCurrent(page, 'Paint a road')
  await dragThrough(page, row(2, 4, 5))
  await expectTicked(page, 'Paint a road', 'Make a unit')

  // 5. A new unit.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/units$/)
  await expectRingOn(page, 'new-item')
  await shot(page, 'art-5')
  await page.getByRole('button', { name: 'New unit' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Archer')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a unit', 'Give the unit a picture')

  // 6. A picture for the unit. Trap: opening the chooser and cancelling changes nothing.
  await expectRingOn(page, 'picture-choose')
  await shot(page, 'art-6')
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await picker(page).getByRole('button', { name: 'Cancel' }).click()
  await expectCurrent(page, 'Give the unit a picture')
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await picker(page).getByLabel('Image').selectOption({ label: 'units' })
  await shot(page, 'art-6b')
  await clickPiece(page, pickerGrid(page), 1, 0)
  await expectCurrent(page, 'Give the unit a picture') // a piece is chosen, but not used yet
  await useIt(page).click()
  await expectTicked(page, 'Give the unit a picture', 'Put it on the map')

  // 7. On the map for Blue.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/start$/)
  await expectRingOn(page, 'tool-units')
  await shot(page, 'art-7')
  await tool(page, 'Units').click()
  await pickSide(page, 'Blue').click()
  await pickUnitType(page, 'Archer').click()
  await expectCurrent(page, 'Put it on the map')
  await clickCell(page, 0, 4)
  await expectTicked(page, 'Put it on the map', 'Play your game')

  // 8. Play.
  await saved(page)
  await expectRingOn(page, 'play')
  await shot(page, 'art-8')
  await expectCurrent(page, 'Play your game')
  await playAndClose(page, () => expect(page.getByTestId('learn-end')).toBeVisible())

  await finish(page, lh, 'art', 'A game that has no problems and that you played.', PRACTICE)
  // What was made, on disk.
  const graphics = readGame(lh, PRACTICE, 'data/graphics.json')
  expect(graphics.terrain.road).toMatchObject({ sheet: 'tiles', join: [0, 2] })
  expect(graphics.units.archer).toMatchObject({ sheet: 'units', cell: [1, 0] })
  const map = readGame(lh, PRACTICE, 'maps/start.json')
  expect(map.tiles[2].split('').filter((c: string) => map.legend[c] === 'road')).toHaveLength(6)
})
