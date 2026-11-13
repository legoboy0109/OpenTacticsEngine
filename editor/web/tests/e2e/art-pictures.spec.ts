// Picture card and picker: every kind for a terrain and a unit, per-side pictures, per-look pictures.
import { readFileSync } from 'node:fs'
import { clickPiece, hex, picker, pickerGrid, pieceColor, seedArt, sheetPng, tilesGraphics } from './artkit'
import { decodePng } from './png'
import { diskPath, editorUrl, expect, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'

const useIt = (page: import('@playwright/test').Page) => picker(page).getByRole('button', { name: 'Use this picture' })
const kind = (page: import('@playwright/test').Page, name: string) => picker(page).getByRole('radio', { name })

async function terrainWithTiles(request: any, game: any, template = 'strategy-rpg', cols = 4, rows = 4) {
  const id = await game.create(template, 'Pictures')
  await seedArt(request, id, { tiles: sheetPng(cols, rows) }, tilesGraphics())
  return id
}

test('assign a picture to a terrain: one piece, saved, previewed, shown on the card @smoke', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game)
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await expect(page.getByText('No picture yet.')).toBeVisible()
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await expect(picker(page)).toBeVisible()
  await expect(useIt(page)).toBeDisabled()
  await expect(page.getByTestId('chosen-summary')).toHaveText('Nothing chosen yet.')
  await clickPiece(page, pickerGrid(page), 2, 1)
  await expect(page.getByTestId('chosen-summary')).toHaveText('1 piece')
  await useIt(page).click()
  await expect(picker(page)).toBeHidden()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain).toEqual({ forest: { sheet: 'tiles', cell: [2, 1] } })
  await expect(page.getByRole('button', { name: 'Change picture…' })).toBeVisible()
  await expect(page.getByRole('img', { name: 'How Forest looks on a map' })).toBeVisible()
  expect(await game.problems(id)).toEqual([])
  // The sample on the card is drawn from that piece.
  const c = page.getByRole('img', { name: 'How Forest looks on a map' })
  const px = await c.evaluate((el) => { const d = (el as HTMLCanvasElement).getContext('2d')!.getImageData(54, 54, 1, 1).data; return [d[0], d[1], d[2]] })
  expect(hex(px)).toBe(hex(pieceColor(2, 1)))
  // Remove picture.
  await page.getByRole('button', { name: 'Remove picture' }).click()
  await expect(page.getByText('No picture yet.')).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain).toBeUndefined()
})

test('random mix: several pieces with numbered badges, click again to take one out', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game)
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await kind(page, 'Random mix').click()
  const g = pickerGrid(page)
  for (const [c, r] of [[0, 0], [1, 0], [2, 0]]) await clickPiece(page, g, c, r)
  await expect(page.getByTestId('chosen-summary')).toHaveText('3 pieces')
  await clickPiece(page, g, 1, 0)
  await expect(page.getByTestId('chosen-summary')).toHaveText('2 pieces')
  await clickPiece(page, g, 3, 3)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.forest).toEqual({ sheet: 'tiles', variants: [[0, 0], [2, 0], [3, 3]] })
  // Reopening shows the same choice and kind.
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await expect(kind(page, 'Random mix')).toHaveAttribute('aria-checked', 'true')
  await expect(page.getByTestId('chosen-summary')).toHaveText('3 pieces')
  await page.keyboard.press('Escape')
  await expect(picker(page)).toBeHidden()
  expect(readDisk(id, 'data/graphics.json').terrain.forest.variants).toHaveLength(3) // cancelling changed nothing
})

test('animation: frames in order, speed presets and the exact time under Advanced', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game)
  await page.goto(editorUrl(id, 'terrain', 'water'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await kind(page, 'Animation').click()
  const g = pickerGrid(page)
  for (const [c, r] of [[0, 1], [1, 1], [0, 1]]) await clickPiece(page, g, c, r)
  await expect(page.getByTestId('chosen-summary')).toHaveText('3 pieces')
  await page.getByRole('button', { name: 'Remove last' }).click()
  await expect(page.getByTestId('chosen-summary')).toHaveText('2 pieces')
  await expect(page.getByLabel('Speed')).toHaveValue('medium')
  await page.getByLabel('Speed').selectOption('fast')
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.water).toEqual({ sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 200 })
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await expect(page.getByLabel('Speed')).toHaveValue('fast')
  await page.getByLabel('Speed').selectOption('slow')
  await picker(page).getByText('Advanced').click()
  await picker(page).getByLabel('Time per picture (milliseconds)').fill('333')
  await expect(page.getByLabel('Speed')).toHaveValue('custom')
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.water.frameMs).toBe(333)
  expect(await game.problems(id)).toEqual([])
})

test('joins with neighbours: the 4 x 4 block follows the pointer, a guide explains it and downloads', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game, 'strategy-rpg', 6, 5)
  await page.goto(editorUrl(id, 'terrain', 'mountain'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await kind(page, 'Joins with neighbours').click()
  const g = pickerGrid(page)
  await expect(picker(page).getByText('Which piece goes where')).toBeVisible()
  // A block that would reach outside the image is refused with an explanation.
  await clickPiece(page, g, 4, 0)
  await expect(page.getByRole('alert').filter({ hasText: 'would reach outside the image' })).toBeVisible()
  await expect(useIt(page)).toBeDisabled()
  await clickPiece(page, g, 1, 1)
  await expect(page.getByTestId('chosen-summary')).toHaveText('2 across, 2 down')
  // Other terrain it also joins with, and the map edge.
  await picker(page).getByRole('checkbox', { name: 'Forest' }).check()
  await picker(page).getByRole('checkbox', { name: 'Joins at the edge of the map' }).uncheck()
  const [download] = await Promise.all([page.waitForEvent('download'), picker(page).getByRole('button', { name: 'Download a guide image' }).click()])
  expect(download.suggestedFilename()).toBe('join-guide-24x24.png')
  const guide = decodePng(readFileSync((await download.path())!))
  expect([guide.width, guide.height]).toEqual([96, 96])
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.mountain).toEqual({ sheet: 'tiles', join: [1, 1], joinWith: ['forest'], joinEdge: false })
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('button', { name: 'Change picture…' }).click()
  await expect(kind(page, 'Joins with neighbours')).toHaveAttribute('aria-checked', 'true')
  await expect(picker(page).getByRole('checkbox', { name: 'Forest' })).toBeChecked()
  await expect(picker(page).getByRole('checkbox', { name: 'Joins at the edge of the map' })).not.toBeChecked()
})

test('keyboard: arrow keys move over the pieces and Enter chooses', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game)
  await page.goto(editorUrl(id, 'terrain', 'plain'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await pickerGrid(page).focus()
  await page.keyboard.press('ArrowRight') // first key shows the cursor at the start, then moves
  await page.keyboard.press('ArrowRight')
  await page.keyboard.press('ArrowDown')
  await page.keyboard.press('Enter')
  await expect(page.getByTestId('chosen-summary')).toHaveText('1 piece')
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').terrain.plain.cell).toHaveLength(2)
})

test('units: one piece and animation; a different picture for a side; sides come from the maps', async ({ page, request, game }) => {
  const id = await game.create('strategy-rpg', 'Unit Pictures')
  await seedArt(request, id, { men: sheetPng(4, 3) }, { sheets: [{ id: 'men', file: 'art/men.png', cellWidth: 24, cellHeight: 24 }] })
  await page.goto(editorUrl(id, 'units', 'lord'))
  await expect(page.getByText('Choose a picture first; then each side can get its own.')).toBeVisible()
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await expect(kind(page, 'Joins with neighbours')).toHaveCount(0) // units cannot join or mix
  await expect(kind(page, 'Random mix')).toHaveCount(0)
  await clickPiece(page, pickerGrid(page), 0, 0)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').units).toEqual({ lord: { sheet: 'men', cell: [0, 0] } })
  // Sides of this game's maps: blue and red.
  const sides = page.getByRole('region', { name: 'A different picture for a side' })
  await expect(sides.getByText('Blue Company')).toBeVisible()
  await expect(sides.getByText('Red Warband')).toBeVisible()
  await sides.getByRole('button', { name: 'Choose… Red Warband' }).click()
  await expect(picker(page)).toBeVisible()
  await kind(page, 'Animation').click()
  for (const [c, r] of [[0, 2], [1, 2]]) await clickPiece(page, pickerGrid(page), c, r)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').units.lord).toEqual({
    sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', frames: [[0, 2], [1, 2]], frameMs: 400 } },
  })
  // Changing the unit's own picture keeps the side picture.
  await page.getByRole('button', { name: 'Change picture…' }).first().click()
  await clickPiece(page, pickerGrid(page), 3, 1)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').units.lord).toMatchObject({ cell: [3, 1], sides: { red: { frames: [[0, 2], [1, 2]] } } })
  await sides.getByRole('button', { name: 'Remove Red Warband' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').units.lord).toEqual({ sheet: 'men', cell: [3, 1] })
  expect(await game.problems(id)).toEqual([])
})

test('looks: a picture per visual style, "same as the others" and "no picture in this look"', async ({ page, request, game }) => {
  const id = await terrainWithTiles(request, game)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  doc.game.styles = [{ id: 'day', name: 'Day' }, { id: 'night', name: 'Night', background: '#001133' }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: doc.game } })).ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await clickPiece(page, pickerGrid(page), 0, 0)
  await useIt(page).click()
  await saved(page)
  await page.getByLabel('For look:').selectOption('night')
  await expect(page.getByRole('radio', { name: 'Same as the others' })).toHaveAttribute('aria-checked', 'true')
  await expect(page.getByRole('button', { name: 'Choose picture…' })).toHaveCount(0)
  await page.getByRole('radio', { name: 'A picture of its own' }).click()
  await clickPiece(page, pickerGrid(page), 3, 2)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json')).toMatchObject({
    terrain: { forest: { sheet: 'tiles', cell: [0, 0] } }, styles: { night: { terrain: { forest: { sheet: 'tiles', cell: [3, 2] } } } },
  })
  await page.getByRole('radio', { name: 'No picture in this look' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles.night.terrain.forest).toBeNull()
  expect(await game.problems(id)).toEqual([])
  await page.getByRole('radio', { name: 'Same as the others' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles).toBeUndefined()
  // The base picture is untouched and "All looks" shows it.
  await page.getByLabel('For look:').selectOption('')
  await expect(page.getByRole('button', { name: 'Change picture…' })).toBeVisible()
  expect(readDisk(id, 'data/graphics.json').terrain.forest.cell).toEqual([0, 0])
  // The map editor offers the same looks.
  await openMap(page, id, 'ford')
  await expect(page.getByLabel('Look to preview')).toBeVisible()
})
