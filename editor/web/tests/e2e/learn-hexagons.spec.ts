// The lesson "Hexagons" played through the real editor by a script that does what each step says. Each step is current (not ticked)
// before its action and ticked after it; near misses must not tick. Ends with the game loading in the real engine.
import { clickHex, dragHexes } from './hexkit'
import { pickSide, pickStructure, pickTerrain, pickUnitType, tool } from './mapkit'
import { requirePlayer, saved } from './fixtures'
import { expect, expectCurrent, expectRingOn, expectTicked, panel, readGame, test } from './learnkit'
import { finish, playAndClose, practiceOf, shot, startLesson } from './learntopic'

test.use({ viewport: process.env.LEARN_PHONE ? { width: 390, height: 844 } : { width: 1920, height: 1080 } })
const { id: PRACTICE, name: PRACTICE_NAME } = practiceOf('hexagons', 'Hexagons')

test('@smoke Hexagons: every step ticks only after its action, and the practice game loads in the real engine', async ({ page, lh }) => {
  requirePlayer()
  await startLesson(page, 'hexagons', 'Make a big structure')
  await expect(page).toHaveURL(new RegExp(`/p/${PRACTICE}/structures$`))
  await expect(panel(page).getByText('Step 1 of 6')).toBeVisible()
  expect(readGame(lh, PRACTICE, 'game.json').name).toBe(PRACTICE_NAME)

  // 1. A new structure.
  await expectRingOn(page, 'new-item')
  await shot(page, 'hexagons-2')
  await page.getByRole('button', { name: 'New structure' }).click()
  await page.getByRole('dialog').getByRole('textbox').fill('Castle')
  await expectCurrent(page, 'Make a big structure')
  await page.getByRole('dialog').getByRole('button', { name: 'Create' }).click()
  await expectTicked(page, 'Make a big structure', 'Make it cover seven hexagons')

  // 2. Seven hexagons. Trap: the Fortress already covers seven; looking at it ticks nothing. One hexagon is not it either.
  await expectRingOn(page, 'structure-covers')
  await shot(page, 'hexagons-3')
  await page.getByRole('button', { name: /^Fortress\b/ }).click()
  await expectCurrent(page, 'Make it cover seven hexagons')
  await page.getByRole('button', { name: /^Castle\b/ }).click()
  await page.getByRole('radio', { name: '1 hexagon' }).click()
  await expectCurrent(page, 'Make it cover seven hexagons')
  await page.getByRole('radio', { name: /^7 hexagons/ }).click()
  await expectTicked(page, 'Make it cover seven hexagons', 'Put it on the map for Blue')

  // 3. On the map for Blue. Trap: a Castle that Red owns is not Blue's.
  await page.getByRole('button', { name: 'Take me there' }).click()
  await expect(page).toHaveURL(/\/maps\/crossing$/)
  await expectRingOn(page, 'tool-structures')
  await shot(page, 'hexagons-4')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Castle').click()
  await pickSide(page, 'Red Army').click()
  await clickHex(page, 'pointy', 7, 1)
  await expectCurrent(page, 'Put it on the map for Blue')
  await pickSide(page, 'Blue Army').click()
  await expectCurrent(page, 'Put it on the map for Blue')
  await clickHex(page, 'pointy', 4, 7)
  await expectTicked(page, 'Put it on the map for Blue', 'Paint hexagons')

  // 4. Paint mountains. Picking the terrain is not painting; three hexagons are not enough.
  await expectRingOn(page, 'tool-terrain')
  await shot(page, 'hexagons-4b')
  await tool(page, 'Terrain').click()
  await expectRingOn(page, 'tool-terrain')
  await pickTerrain(page, 'Mountain').click()
  await expectCurrent(page, 'Paint hexagons')
  await dragHexes(page, 'pointy', [[0, 0], [1, 0]])
  await clickHex(page, 'pointy', 4, 0)
  await expectCurrent(page, 'Paint hexagons')
  await clickHex(page, 'pointy', 6, 0)
  await expectTicked(page, 'Paint hexagons', 'Put a unit beside it')

  // 5. A unit. Trap: a red unit is not a new blue one.
  await expectRingOn(page, 'tool-units')
  await shot(page, 'hexagons-5')
  await tool(page, 'Units').click()
  await pickUnitType(page, 'Infantry').click()
  await pickSide(page, 'Red Army').click()
  await clickHex(page, 'pointy', 4, 4)
  await expectCurrent(page, 'Put a unit beside it')
  await pickSide(page, 'Blue Army').click()
  await clickHex(page, 'pointy', 4, 5)
  await expectTicked(page, 'Put a unit beside it', 'Play your game')

  // 6. Play.
  await saved(page)
  await expectRingOn(page, 'play')
  await shot(page, 'hexagons-6')
  await expectCurrent(page, 'Play your game')
  await playAndClose(page, () => expect(page.getByTestId('learn-end')).toBeVisible())

  await finish(page, lh, 'hexagons', 'A game that has no problems and that you played.', PRACTICE)
  // What was made, on disk.
  const kinds = readGame(lh, PRACTICE, 'data/structures.json').structures
  expect(kinds.find((s: any) => s.id === 'castle')).toMatchObject({ name: 'Castle', rings: 1 })
  const map = readGame(lh, PRACTICE, 'maps/crossing.json')
  expect(map.structures.filter((s: any) => s.type === 'castle').map((s: any) => `${s.x},${s.y},${s.side}`).sort()).toEqual(['4,7,blue', '7,1,red'])
  expect(map.tiles[0].split('').filter((c: string) => c === 'm')).toHaveLength(4)
  expect(map.units.filter((u: any) => u.side === 'blue')).toHaveLength(4)
})
