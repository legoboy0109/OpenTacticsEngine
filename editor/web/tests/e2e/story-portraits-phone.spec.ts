// Portraits in the Art area (suggested from story names, a piece clicked in the picker), and the story panels on a phone.
// No test here depends on the loader accepting or rejecting graphics.json's "portraits" key.
import { readFileSync } from 'node:fs'
import { expect, expectNoHScroll, diskPath, readDisk, saved, test } from './fixtures'
import { clickCell, openMap, tool } from './mapkit'
import { addCommand, editor, eventsTab, form, newEvent, nameUnit, shot } from './storykit'
import { tab } from './mapkit'

test('@smoke portraits: suggestions from story names, a piece clicked in the picker, rename, reorder, delete', async ({ page, game, request }) => {
  test.setTimeout(90_000)
  const id = await game.create('economy-tactics', 'Portraits')
  // Two named units in the map.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.maps.crossroads.units[0].id = 'eirik'
  doc.maps.crossroads.units[3].id = 'gorm'
  doc.maps.crossroads.units[3].name = 'Gorm'
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: { crossroads: doc.maps.crossroads } } })).ok()).toBeTruthy()

  await page.goto(`/p/${id}/art`)
  const card = page.getByTestId('portraits')
  await expect(card).toContainText('No portraits yet')
  await card.getByRole('button', { name: 'Add Gorm (gorm)' }).click()
  await card.getByRole('button', { name: 'Add Infantry (eirik)' }).click()
  await card.getByLabel('Name of a new portrait').fill('villager')
  await card.getByRole('button', { name: 'Add portrait' }).click()
  await saved(page)
  const sheet = readDisk(id, 'data/graphics.json').sheets[0]
  expect(readDisk(id, 'data/graphics.json').portraits).toEqual({
    gorm: { sheet: sheet.id, cell: [0, 0] }, eirik: { sheet: sheet.id, cell: [0, 0] }, villager: { sheet: sheet.id, cell: [0, 0] },
  })

  // Pick a piece by clicking it in the picker (the same piece grid as for terrain and units).
  const png = readFileSync(diskPath(id, sheet.file))
  const imgW = png.readUInt32BE(16), imgH = png.readUInt32BE(20)
  await card.getByRole('button', { name: 'Choose picture…' }).first().click()
  const dlg = page.getByRole('dialog', { name: /Picture for/ })
  const grid = dlg.locator('canvas').first()
  await expect(grid).toBeVisible()
  const box = (await grid.boundingBox())!
  const col = 2, row = 1
  await page.mouse.click(box.x + ((col + 0.5) * sheet.cellWidth / imgW) * box.width, box.y + ((row + 0.5) * sheet.cellHeight / imgH) * box.height)
  await shot(page, 'portrait-picker')
  await dlg.getByRole('button', { name: 'Use this picture' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').portraits.gorm).toEqual({ sheet: sheet.id, cell: [col, row] })
  await shot(page, 'portraits')

  // Rename keeps the place in the list; reorder and delete with the buttons.
  await card.getByLabel('Name of portrait 1').fill('gorm_the_great')
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').portraits)).toEqual(['gorm_the_great', 'eirik', 'villager'])
  await card.getByRole('button', { name: 'Move portrait villager up' }).click()
  await card.getByRole('button', { name: 'Delete portrait eirik' }).click()
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').portraits)).toEqual(['gorm_the_great', 'villager'])
  await card.getByLabel('Name of portrait 2').fill('bad name!')
  await expect(card.getByRole('alert')).toBeVisible()
  await page.getByRole('button', { name: 'Undo' }).click()
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(Object.keys(readDisk(id, 'data/graphics.json').portraits)).toEqual(['gorm_the_great', 'eirik', 'villager'])
  // Deleting the image the portraits use asks first and takes them along.
  await page.goto(`/p/${id}/art/${sheet.id}`)
  await page.getByRole('button', { name: 'Remove', exact: true }).click()
  await expect(page.getByRole('dialog')).toContainText('Portrait: gorm_the_great')
})

test('@smoke the story panels fit a 360px phone: Events with a nested If open, Objectives, the Rules lists', async ({ page, game }) => {
  test.setTimeout(90_000)
  const id = await game.create('economy-tactics', 'StoryPhone')
  await page.setViewportSize({ width: 360, height: 740 })
  await page.goto(`/p/${id}/rules`)
  await page.getByLabel('Add a switch').fill('Village saved')
  await page.getByLabel('Add a switch').press('Enter')
  await page.getByLabel('Add a variable').fill('Gold found')
  await page.getByLabel('Add a variable').press('Enter')
  await saved(page)
  await expectNoHScroll(page)
  await shot(page, 'rules-switches-phone')

  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await nameUnit(page, 9, 2, 'gorm')
  await eventsTab(page).click()
  await expectNoHScroll(page)
  await shot(page, 'events-empty-phone')
  await newEvent(page, 'ambush')
  await editor(page).getByLabel('When', { exact: true }).selectOption({ label: 'A unit reaches an area' })
  await editor(page).getByLabel('Which unit reaches it', { exact: true }).selectOption({ label: 'A specific unit' })
  await expect(editor(page).getByRole('button', { name: 'When: area: Pick on the map' })).toBeVisible()
  await addCommand(page, 'Then', 'Say something')
  await form(page).getByLabel('Who speaks').selectOption({ label: 'Infantry (gorm)' })
  await form(page).getByLabel('What is said').fill('Now! Strike from the forest, everyone, now!')
  await addCommand(page, 'Then', 'If… then… otherwise…')
  await addCommand(page, 'If 2: Then', 'If… then… otherwise…')
  await addCommand(page, 'If 2: Then, If 1: Then', 'If… then… otherwise…')
  await addCommand(page, 'If 2: Then, If 1: Then, If 1: Then', 'Add a unit')
  await expectNoHScroll(page)
  await shot(page, 'event-open-phone')
  for (const kind of ['Move a unit', 'Change the objectives', 'Give resources']) {
    await addCommand(page, 'Then', kind)
    await expectNoHScroll(page)
  }
  await shot(page, 'event-forms-phone')

  // Picking on the map works by tapping the map, which stays at the top of the phone screen.
  await editor(page).getByRole('button', { name: 'When: area: Pick on the map' }).scrollIntoViewIfNeeded()
  await editor(page).getByRole('button', { name: 'When: area: Pick on the map' }).click()
  await page.getByTestId('map-canvas').scrollIntoViewIfNeeded()
  await clickCell(page, 3, 3)
  await expect(editor(page).getByLabel('When: area: Left column')).toHaveValue('4')
  await saved(page)

  await tab(page, 'Objectives').click()
  await page.getByLabel('map: Add a way to win…').selectOption({ label: 'A unit reaches an area' })
  await page.getByLabel('map: Add a way to lose…').selectOption({ label: 'A round begins' })
  await expectNoHScroll(page)
  await shot(page, 'objectives-phone')
  await page.goto(`/p/${id}/art`)
  await expectNoHScroll(page)
  await shot(page, 'art-portraits-phone')
})
