// Objectives (docs/EVENTS.md): the Objectives tab, every kind of way to win and to lose, the "no objectives" state.
import { expect, readDisk, saved, test } from './fixtures'
import { canvas, clickCell, dragThrough, openMap, tab, tool } from './mapkit'
import { nameUnit, shot } from './storykit'

test('@smoke objectives of every kind, "no objectives" when empty, areas drawn on the map', async ({ page, game }) => {
  test.setTimeout(90_000)
  const id = await game.create('economy-tactics', 'Objectives')
  const mapFile = 'maps/crossroads.json'
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 9, 2, 'gorm')
  await nameUnit(page, 2, 2, 'eirik')
  await tab(page, 'Objectives').click()
  const panel = page.getByTestId('objectives-panel')
  await expect(page.getByTestId('no-objectives')).toContainText('No objectives (the game’s defeat rules decide)')

  await panel.getByLabel('Objective text').fill('Defeat Gorm')
  await expect(page.getByTestId('no-objectives')).toHaveCount(0)
  const win = (n: number) => panel.getByRole('group', { name: `map: Way to win ${n}` })
  const lose = (n: number) => panel.getByRole('group', { name: `map: Way to lose ${n}` })
  await panel.getByLabel('map: Add a way to win…').selectOption({ label: 'A unit is defeated' })
  await win(1).getByLabel('Which unit', { exact: true }).selectOption({ label: 'A specific unit' })
  await win(1).getByLabel('Which unit: unit').selectOption({ label: 'Infantry (gorm)' })
  await panel.getByLabel('map: Add a way to win…').selectOption({ label: 'A unit reaches an area' })
  await panel.getByRole('button', { name: 'Way to win 2: area: Pick on the map' }).click()
  await dragThrough(page, [[0, 0], [1, 1]])
  await panel.getByLabel('map: Add a way to win…').selectOption({ label: 'A round begins' })
  await win(3).getByLabel('Round number').fill('12')
  await panel.getByLabel('map: Add a way to win…').selectOption({ label: 'Survive some rounds' })
  await win(4).getByLabel('Rounds to survive').fill('8')

  await panel.getByLabel('map: Add a way to lose…').selectOption({ label: 'A unit is defeated' })
  await lose(1).getByLabel('Which unit', { exact: true }).selectOption({ label: 'Any unit of side…' })
  await lose(1).getByLabel('Which unit: side').selectOption({ label: 'Blue Army' })
  await panel.getByLabel('map: Add a way to lose…').selectOption({ label: 'A unit reaches an area' })
  await lose(2).getByLabel('Which unit', { exact: true }).selectOption({ label: 'Any unit' })
  await panel.getByRole('button', { name: 'Way to lose 2: area: Pick on the map' }).click()
  await clickCell(page, 11, 7)
  await panel.getByLabel('map: Add a way to lose…').selectOption({ label: 'A round begins' })
  await expect(lose(3).getByLabel('Round number')).toHaveValue('5')
  // "Survive" is a way to win only: it is not offered for losing.
  await expect(panel.getByLabel('map: Add a way to lose…').getByRole('option', { name: 'Survive some rounds' })).toHaveCount(0)
  await saved(page)
  await expect(canvas(page)).toHaveAttribute('data-areas', 'win:0,0,2,2;lose:11,7,1,1')
  await shot(page, 'objectives')

  expect(readDisk(id, mapFile).objectives).toEqual({
    text: 'Defeat Gorm',
    win: [{ defeated: { unit: 'gorm' } }, { reach: { x: 0, y: 0, w: 2, h: 2 } }, { round: 12 }, { survive: 8 }],
    lose: [{ defeated: { side: 'blue' } }, { reach: { x: 11, y: 7, w: 1, h: 1 }, by: {} }, { round: 5 }],
  })
  expect(await game.problems(id)).toEqual([])

  // Reorder and delete rules with the buttons; emptied lists and text leave the file entirely.
  await panel.getByRole('button', { name: 'Move Way to win 4 up' }).click()
  await saved(page)
  expect(readDisk(id, mapFile).objectives.win.map((r: any) => Object.keys(r)[0])).toEqual(['defeated', 'reach', 'survive', 'round'])
  for (let n = 0; n < 4; n++) await panel.getByRole('button', { name: 'Delete Way to win 1' }).click()
  for (let n = 0; n < 3; n++) await panel.getByRole('button', { name: 'Delete Way to lose 1' }).click()
  await expect(panel.getByRole('button', { name: /^Delete Way to/ })).toHaveCount(0)
  await panel.getByLabel('Objective text').fill('')
  await saved(page)
  expect(readDisk(id, mapFile)).not.toHaveProperty('objectives')
  await expect(page.getByTestId('no-objectives')).toBeVisible()
  expect(await game.problems(id)).toEqual([])
})
