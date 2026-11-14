// Animated joins (several 4 x 4 blocks), "joins with everything except", and the Saving card.
import { clickPiece, picker, pickerGrid, seedArt, sheetPng, tilesGraphics } from './artkit'
import { editorUrl, expect, readDisk, saved, test } from './fixtures'

const useIt = (page: import('@playwright/test').Page) => picker(page).getByRole('button', { name: 'Use this picture' })
const kind = (page: import('@playwright/test').Page, name: string) => picker(page).getByRole('radio', { name })

test('water as an animated join (two blocks) and a terrain joining with everything except water @smoke', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Joins')
  await seedArt(request, id, { tiles: sheetPng(8, 8) }, tilesGraphics())

  // Water: two 4 x 4 blocks, fast.
  await page.goto(editorUrl(id, 'terrain', 'water'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await kind(page, 'Joins with neighbours').click()
  const g = pickerGrid(page)
  await clickPiece(page, g, 0, 0)
  await expect(page.getByTestId('chosen-summary')).toHaveText('1 across, 1 down')
  await expect(picker(page).getByLabel('Speed')).toHaveCount(0) // one block does not move
  await clickPiece(page, g, 4, 4)
  await expect(page.getByTestId('chosen-summary')).toHaveText('2 sets of 16 pieces, played in order')
  await picker(page).getByLabel('Speed').selectOption('fast')
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.water).toEqual({ sheet: 'tiles', join: [[0, 0], [4, 4]], frameMs: 200 })
  expect(await game.problems(id)).toEqual([])

  // Mountain: one block, joins with everything except water.
  await page.goto(editorUrl(id, 'terrain', 'mountain'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await kind(page, 'Joins with neighbours').click()
  await clickPiece(page, pickerGrid(page), 0, 4)
  await picker(page).getByLabel('Joins with').selectOption({ label: 'Everything except the terrain I choose' })
  await picker(page).getByRole('checkbox', { name: 'River' }).check()
  await expect(picker(page).getByRole('checkbox', { name: 'Plain' })).not.toBeChecked()
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.mountain).toEqual({ sheet: 'tiles', join: [0, 4], joinExcept: ['water'] })
  expect(await game.problems(id)).toEqual([])

  // Reopening shows the same choices; cancelling changes nothing.
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await expect(picker(page).getByLabel('Joins with')).toHaveValue('except')
  await expect(picker(page).getByRole('checkbox', { name: 'River' })).toBeChecked()
  await page.keyboard.press('Escape')
  await page.goto(editorUrl(id, 'terrain', 'water'))
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await expect(page.getByTestId('chosen-summary')).toHaveText('2 sets of 16 pieces, played in order')
  await expect(picker(page).getByLabel('Speed')).toHaveValue('fast')
  await page.keyboard.press('Escape')
  expect(readDisk(id, 'data/graphics.json').terrain.water.join).toEqual([[0, 0], [4, 4]])
})

test('the Saving card writes the game setting; "anywhere" removes the key @smoke', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Saving')
  await page.goto(editorUrl(id, 'rules'))
  const card = page.getByRole('radiogroup', { name: 'Saving' })
  await expect(card.getByRole('radio', { name: 'Players can save anywhere' })).toHaveAttribute('aria-checked', 'true')
  expect('saving' in readDisk(id, 'game.json')).toBe(false)
  await card.getByRole('radio', { name: 'Only the automatic Continue save' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').saving).toBe('continue')
  expect(await game.problems(id)).toEqual([])
  await card.getByRole('radio', { name: 'No saving' }).click()
  await saved(page)
  expect(readDisk(id, 'game.json').saving).toBe('off')
  expect(await game.problems(id)).toEqual([])
  await page.reload()
  await expect(page.getByRole('radiogroup', { name: 'Saving' }).getByRole('radio', { name: 'No saving' })).toHaveAttribute('aria-checked', 'true')
  await page.getByRole('radiogroup', { name: 'Saving' }).getByRole('radio', { name: 'Players can save anywhere' }).click()
  await saved(page)
  expect('saving' in readDisk(id, 'game.json')).toBe(false)
  expect(await game.problems(id)).toEqual([])
})
