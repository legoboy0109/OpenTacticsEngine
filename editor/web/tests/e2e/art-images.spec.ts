// The Art screen: adding images (PNG and other formats), the limits, the grid, replace, remove, unused files.
import { existsSync, readdirSync, readFileSync } from 'node:fs'
import { addImage, addImages, bareGame, imageList, makePng, openArt, png, sheetPng, solidPng } from './artkit'
import { diskPath, expect, readDisk, saved, test } from './fixtures'

const noConflict = (page: import('@playwright/test').Page) => expect(page.getByText(/changed on disk/i)).toHaveCount(0)

test('add a PNG: it is listed, divided by the first guess, saved, and no false conflict @smoke', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art Add')
  await openArt(page, id)
  await expect(page.getByText('No images yet.').first()).toBeVisible()
  await addImage(page, 'Terrain Tiles.png', sheetPng(4, 2), 'Terrain Tiles')
  await expect(page.getByTestId('grid-summary')).toHaveText('4 across, 2 down')
  await expect(page.getByRole('radio', { name: 'A grid of pieces' })).toHaveAttribute('aria-checked', 'true')
  await expect(page.getByLabel('Piece width')).toHaveValue('24')
  await noConflict(page)
  expect(existsSync(diskPath(id, 'art/terrain_tiles.png'))).toBe(true)
  expect(readDisk(id, 'data/graphics.json')).toEqual({
    sheets: [{ id: 'terrain_tiles', file: 'art/terrain_tiles.png', cellWidth: 24, cellHeight: 24, name: 'Terrain Tiles' }],
  })
  expect(await game.problems(id)).toEqual([])
  // The uploaded bytes are exactly the PNG that was chosen.
  expect(readFileSync(diskPath(id, 'art/terrain_tiles.png')).equals(sheetPng(4, 2))).toBe(true)
  // More edits after the upload save normally (the revision was adopted).
  await page.getByLabel('Name', { exact: true }).fill('Ground')
  await saved(page)
  await noConflict(page)
  expect(readDisk(id, 'data/graphics.json').sheets[0].name).toBe('Ground')
})

test('a small image is one picture; the same name twice gets a different file name', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art One')
  await openArt(page, id)
  await addImage(page, 'hero.png', solidPng(24, 24, [200, 30, 30, 255]))
  await expect(page.getByRole('radio', { name: 'One picture' })).toHaveAttribute('aria-checked', 'true')
  await expect(page.getByTestId('grid-summary')).toHaveText('1 across, 1 down')
  await addImages(page, [png('hero.png', solidPng(30, 20, [30, 30, 200, 255]))])
  await expect(imageList(page).getByRole('button')).toHaveCount(2)
  await saved(page)
  expect(existsSync(diskPath(id, 'art/hero.png')) && existsSync(diskPath(id, 'art/hero_2.png'))).toBe(true)
  expect(readDisk(id, 'data/graphics.json').sheets.map((s: any) => s.file)).toEqual(['art/hero.png', 'art/hero_2.png'])
  // Several files at once; the last one added is shown.
  await addImages(page, [png('a.png', solidPng(8, 8, [1, 2, 3, 255])), png('b.png', solidPng(8, 8, [4, 5, 6, 255]))])
  await expect(imageList(page).getByRole('button')).toHaveCount(4)
  await expect(page).toHaveURL(/\/art\/b$/)
})

test('an image that is not a PNG is converted in the browser before it goes up', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art Convert')
  await openArt(page, id)
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="48" height="24"><rect width="24" height="24" fill="#d22"/><rect x="24" width="24" height="24" fill="#22d"/></svg>'
  await addImages(page, [{ name: 'flags.svg', mimeType: 'image/svg+xml', buffer: Buffer.from(svg) }])
  await expect(page.getByText('“flags.svg” was converted to a PNG image.')).toBeVisible()
  await expect(imageList(page).getByRole('button', { name: /flags/ })).toBeVisible()
  await saved(page)
  const bytes = readFileSync(diskPath(id, 'art/flags.png'))
  expect([...bytes.subarray(0, 8)]).toEqual([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a])
  expect(bytes.readUInt32BE(16)).toBe(48)
  expect(bytes.readUInt32BE(20)).toBe(24)
  expect(readDisk(id, 'data/graphics.json').sheets[0]).toMatchObject({ file: 'art/flags.png', cellWidth: 48, cellHeight: 24 })
  expect(await game.problems(id)).toEqual([])
})

test('an image that cannot be opened, or is too large, is refused with a plain message and nothing changes', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art Refuse')
  await openArt(page, id)
  await addImages(page, [{ name: 'notes.png', mimeType: 'image/png', buffer: Buffer.from('this is not a picture') }])
  await expect(page.getByText('“notes.png” could not be opened as an image.')).toBeVisible()
  await addImages(page, [png('huge.png', solidPng(3000, 1200, [9, 9, 9, 255]))])
  await expect(page.getByText('This image is 3000 × 1200 pixels. The largest that works on every device is 2048 × 2048.')).toBeVisible()
  await expect(page.getByText('No images yet.').first()).toBeVisible()
  await saved(page)
  expect(readdirSync(diskPath(id, 'art'))).toEqual([])
  expect(readDisk(id, 'data/graphics.json')).toEqual({ sheets: [] })
  // 2048 x 2048 is the largest that works.
  await addImage(page, 'max.png', solidPng(2048, 2048, [5, 5, 5, 255]))
  expect(readDisk(id, 'data/graphics.json').sheets[0]).toMatchObject({ cellWidth: 16 })
})

test('when the helper refuses an upload the message is shown and the document is untouched', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art Helper No')
  await openArt(page, id)
  await page.route('**/api/projects/*/art/*.png*', (r) => r.fulfill({ status: 400, contentType: 'application/json', body: '{"error":"the file is not a PNG image"}' }))
  await addImages(page, [png('x.png', solidPng(24, 24, [1, 1, 1, 255]))])
  await expect(page.getByText('“x.png” was not added: the file is not a PNG image')).toBeVisible()
  await expect(page.getByText('No images yet.').first()).toBeVisible()
  await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()
  expect(readDisk(id, 'data/graphics.json')).toEqual({ sheets: [] })
  expect(readdirSync(diskPath(id, 'art'))).toEqual([])
})

test('set how an image is divided: grid size, quick sizes, left-over pixels', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'strategy-rpg', 'Art Grid')
  await openArt(page, id)
  await addImage(page, 'sheet.png', makePng(100, 50, () => [90, 120, 60, 255]))
  await expect(page.getByTestId('grid-summary')).toHaveText('4 across, 2 down') // nothing divides 100 x 50 evenly: the tile size
  await expect(page.getByTestId('leftover')).toHaveText('Not used by the game: the rightmost 4 pixels and the bottom 2 pixels (they do not make a whole piece).')
  await page.getByRole('button', { name: '16 × 16' }).click()
  await expect(page.getByTestId('grid-summary')).toHaveText('6 across, 3 down')
  await page.getByLabel('Piece width').fill('20')
  await page.getByLabel('Piece height').fill('10')
  await expect(page.getByTestId('grid-summary')).toHaveText('5 across, 5 down')
  await expect(page.getByTestId('leftover')).toHaveCount(0)
  await page.getByRole('radio', { name: 'One picture' }).click()
  await expect(page.getByTestId('grid-summary')).toHaveText('1 across, 1 down')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').sheets[0]).toMatchObject({ cellWidth: 100, cellHeight: 50 })
  // Back to a grid: starts from the tile size again.
  await page.getByRole('radio', { name: 'A grid of pieces' }).click()
  await expect(page.getByLabel('Piece width')).toHaveValue('24')
  // Pieces bigger than the image leave no whole piece: say so.
  await page.getByLabel('Piece width').fill('101')
  await expect(page.getByLabel('Piece width')).toHaveValue('101')
  await page.getByLabel('Piece width').blur()
})
