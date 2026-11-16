// Characters screenshots for design review (SHOTS=1 SHOTS_DIR=<folder>; skipped otherwise).
import type { Page } from '@playwright/test'
import { newCharacter, switchOn } from './characterskit'
import { editorUrl, expect, readDisk, saved, test, writeDisk } from './fixtures'
import { clickCell, openMap, pickSide, tool } from './mapkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string, fullPage = false) {
  await page.waitForTimeout(250)
  await page.screenshot({ path: `${OUT}/${name}.png`, fullPage })
}

async function fill(page: Page, id: string) {
  await switchOn(page, id)
  await newCharacter(page, 'Aldric')
  const form = page.locator('.md-detail')
  await form.getByLabel('Class', { exact: true }).selectOption('cavalier')
  await form.getByLabel('Description').fill('Captain of the Blue Company.')
  await form.getByLabel('Joins at level').fill('3')
  await form.getByLabel('Strength personal bonus').fill('2')
  await form.getByLabel('Skill personal bonus').fill('-1')
  await form.getByLabel('Strength personal growth bonus').fill('10')
  await newCharacter(page, 'Mira')
  await page.locator('.md-detail').getByLabel('Class', { exact: true }).selectOption('archer')
  await saved(page)
}

test('desktop', async ({ page, game }) => {
  await page.setViewportSize({ width: 1180, height: 820 })
  await page.emulateMedia({ colorScheme: 'light' })
  const id = await game.create('strategy-rpg', 'Shots')
  await switchOn(page, id)
  await shot(page, 'e1-01-characters-empty')
  await fill(page, id)
  await page.goto(editorUrl(id, 'characters/aldric'))
  await expect(page.getByTestId('character-checks')).toBeVisible()
  await shot(page, 'e1-02-characters-form')
  await page.locator('table.three-col').first().scrollIntoViewIfNeeded()
  await shot(page, 'e1-03-characters-stats')

  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await page.getByRole('tab', { name: 'Characters', exact: true }).click()
  await pickSide(page, 'Blue').click()
  await page.getByRole('radiogroup', { name: 'Characters' }).getByRole('radio', { name: /Aldric/ }).click()
  await clickCell(page, 3, 1)
  await expect(page.getByTestId('unit-character')).toBeVisible()
  await saved(page)
  await shot(page, 'e1-04-map-characters-tab')
  await page.getByTestId('unit-props').scrollIntoViewIfNeeded()
  await page.getByTestId('unit-character').scrollIntoViewIfNeeded()
  await shot(page, 'e1-05-map-character-unit')

  // A problem: a class that is gone (a file edited by hand), in the loader's words.
  const file = readDisk(id, 'data/characters.json')
  file.characters[1].type = 'knight'
  writeDisk(id, 'data/characters.json', JSON.stringify(file))
  await page.goto(editorUrl(id, 'characters/mira'))
  await page.getByRole('button', { name: /^\d+ problems?$/ }).click()
  await shot(page, 'e1-06-characters-check')
})

test('phone', async ({ page, game }) => {
  await page.setViewportSize({ width: 360, height: 740 })
  await page.emulateMedia({ colorScheme: 'light' })
  const id = await game.create('strategy-rpg', 'Shots phone')
  await fill(page, id)
  await page.goto(editorUrl(id, 'characters'))
  await shot(page, 'e1-07-phone-list')
  await page.goto(editorUrl(id, 'characters/aldric'))
  await shot(page, 'e1-08-phone-form', true)
  await openMap(page, id, 'ford')
  await tool(page, 'Units').click()
  await page.getByRole('tab', { name: 'Characters', exact: true }).click()
  await shot(page, 'e1-09-phone-map-tab')
})
