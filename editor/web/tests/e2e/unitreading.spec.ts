// Variable commands that read a unit's number, and the "Take an item away" command (docs/EVENTS.md), through the real editor:
// the form's "Take the number from" choice, the unit and stat pickers, the exact JSON written (and loaded by the real game
// loader, through the helper's problems list), a campaign scene, and a phone.
import type { Page } from '@playwright/test'
import { editorUrl, expect, expectNoHScroll, readDisk, saved, test } from './fixtures'
import { addSceneCommand, openChapter, sceneForm } from './campaignkit'
import { openMap, tool } from './mapkit'
import { addCommand, createItem, editor, eventsTab, form, nameUnit, newEvent, shot } from './storykit'

async function prepare(page: Page, game: { create: (t: string, n?: string) => Promise<string> }, name: string): Promise<string> {
  const id = await game.create('economy-tactics', name)
  await page.goto(`/p/${id}/rules`)
  await page.getByLabel('Add a variable').fill('Bar')
  await page.getByLabel('Add a variable').press('Enter')
  await saved(page)
  await page.goto(`/p/${id}/items`)
  await createItem(page, 'Potion')
  await saved(page)
  await openMap(page, id, 'crossroads')
  await tool(page, 'Select').click()
  await nameUnit(page, 2, 2, 'eirik')
  await nameUnit(page, 9, 2, 'gorm')
  await saved(page)
  return id
}

test('@smoke a variable that reads two units, an item taken away: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await prepare(page, game, 'Reading')
  await eventsTab(page).click()
  await newEvent(page, 'sum')

  // --- Set the variable to eirik's level ("A number I type" is what a new command starts with)
  await addCommand(page, 'Then', 'Set a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
  await expect(form(page).getByLabel('Take the number from')).toHaveValue('number')
  await expect(form(page).getByLabel('Set it to')).toBeVisible()
  await form(page).getByLabel('Take the number from').selectOption({ label: 'A unit’s level' })
  await expect(form(page).getByLabel('Set it to')).toHaveCount(0)
  await form(page).getByLabel('Which unit').selectOption('eirik')
  await shot(page, 'reading-form')

  // --- Add gorm's level, then eirik's Move stat, then gorm's most hit points
  await addCommand(page, 'Then', 'Add to a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
  await form(page).getByLabel('Take the number from').selectOption({ label: 'A unit’s level' })
  await form(page).getByLabel('Which unit').selectOption('gorm')
  await addCommand(page, 'Then', 'Add to a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
  await form(page).getByLabel('Take the number from').selectOption({ label: 'A stat of a unit' })
  await form(page).getByLabel('Which stat').selectOption({ label: 'Move' })
  await form(page).getByLabel('Which unit').selectOption('eirik')
  await addCommand(page, 'Then', 'Add to a variable')
  await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
  await form(page).getByLabel('Take the number from').selectOption({ label: 'A unit’s most hit points' })
  await form(page).getByLabel('Which unit').selectOption('gorm')
  // Switching back to a number gives the plain command again.
  await form(page).getByLabel('Take the number from').selectOption({ label: 'A number I type' })
  await expect(form(page).getByLabel('Add', { exact: true })).toHaveValue('1')

  // --- Take an item away
  await addCommand(page, 'Then', 'Take an item away')
  await form(page).getByLabel('Item', { exact: true }).selectOption({ label: 'Potion' })
  await form(page).getByLabel('Who loses it', { exact: true }).selectOption({ label: 'A specific unit' })
  await form(page).getByLabel('Who loses it: unit').selectOption({ label: 'Infantry (eirik)' })
  await form(page).getByLabel('Take every copy').check()
  await shot(page, 'takeitem-form')
  await saved(page)

  const ev = readDisk(id, 'maps/crossroads.json').events
  expect(ev).toHaveLength(1)
  const move = readDisk(id, 'game.json').stats.find((s: any) => s.name === 'Move').id
  expect(ev[0].do).toEqual([
    { setVariable: 'bar', level: 'eirik' },
    { addVariable: 'bar', level: 'gorm' },
    { addVariable: 'bar', stat: move, of: 'eirik' },
    { addVariable: 'bar', by: 1 },
    { takeItem: 'potion', from: { unit: 'eirik' }, all: true },
  ])
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('event-checks')).toHaveCount(0)

  // The command list says it in words.
  await expect(editor(page).getByTestId('command-setVariable')).toContainText('the level of')
  await expect(editor(page).getByTestId('command-takeItem')).toContainText('Takes every “Potion” from')

})

test('@smoke a scene reads a character and takes an item away: the file is exactly that and the game loads', async ({ page, game }) => {
  test.setTimeout(150_000)
  const id = await prepare(page, game, 'Scene reading')
  await page.goto(editorUrl(id, 'campaign'))
  await page.getByRole('button', { name: 'Create a campaign' }).click()
  await page.getByRole('button', { name: 'Add a chapter' }).click()
  await page.getByLabel('Name of the chapter').fill('Opening')
  await page.keyboard.press('Enter')
  await saved(page)
  await openChapter(page, 'Opening')
  await addSceneCommand(page, 'after', 'After the battle', 'Set a variable')
  await sceneForm(page, 'after').getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
  await sceneForm(page, 'after').getByLabel('Take the number from').selectOption({ label: 'A unit’s level' })
  // A scene cannot read hit points between battles, so that choice is not offered.
  await expect(sceneForm(page, 'after').getByLabel('Take the number from').locator('option', { hasText: 'hit points now' })).toHaveCount(0)
  await sceneForm(page, 'after').getByLabel('Which unit').selectOption('eirik')
  // (a scene names the player's own characters only: eirik stands on the player's side, gorm does not)
  await addSceneCommand(page, 'after', 'After the battle', 'Take an item away')
  await sceneForm(page, 'after').getByLabel('Item', { exact: true }).selectOption({ label: 'Potion' })
  await sceneForm(page, 'after').getByLabel('Which character loses it').selectOption('eirik')
  await saved(page)
  const campaign = readDisk(id, 'data/campaign.json')
  expect(campaign.chapters.find((c: any) => c.id === 'opening').after).toEqual([
    { setVariable: 'bar', level: 'eirik' },
    { takeItem: 'potion', from: 'eirik' },
  ])
  expect(await game.problems(id)).toEqual([])
})

test.describe('phone', () => {
  test.use({ viewport: { width: 360, height: 740 }, hasTouch: true, isMobile: true })

  test('the reading form and the take-an-item form at 360 px have no sideways scroll', async ({ page, game }) => {
    test.setTimeout(90_000)
    await prepare(page, game, 'Reading phone')
    await eventsTab(page).click()
    await newEvent(page, 'sum')
    await addCommand(page, 'Then', 'Set a variable')
    await form(page).getByLabel('Variable', { exact: true }).selectOption({ label: 'Bar' })
    await form(page).getByLabel('Take the number from').selectOption({ label: 'A stat of a unit' })
    await form(page).getByLabel('Which unit').selectOption('eirik')
    await expectNoHScroll(page)
    await addCommand(page, 'Then', 'Take an item away')
    await expectNoHScroll(page)
    await shot(page, 'reading-phone')
    await expect(form(page).getByLabel('Take every copy')).toBeVisible()
  })
})
