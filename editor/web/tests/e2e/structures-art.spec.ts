// Pictures for structures: the Picture card and picker (one piece or animation only), a different picture per owner, per style,
// duplicating and deleting, and the Art screen counting structures as users of an image.
import type { Page } from '@playwright/test'
import { clickPiece, hex, picker, pickerGrid, pieceColor, seedArt, sheetPng } from './artkit'
import { editorUrl, expect, readDisk, saved, test } from './fixtures'

const useIt = (page: Page) => picker(page).getByRole('button', { name: 'Use this picture' })
const kind = (page: Page, name: string) => picker(page).getByRole('radio', { name })
const sheets = { sheets: [{ id: 'blds', file: 'art/blds.png', cellWidth: 24, cellHeight: 24, name: 'Buildings' }] }

async function setup(request: any, game: any, template = 'economy-tactics') {
  const id = await game.create(template, 'Structure Pictures')
  await seedArt(request, id, { blds: sheetPng(4, 3) }, sheets)
  return id
}

test('@smoke give a structure a picture: one piece, saved, previewed at its footprint, removed again', async ({ page, request, game }) => {
  const id = await setup(request, game)
  await page.goto(editorUrl(id, 'structures', 'hq'))
  await expect(page.getByText('No picture yet.')).toBeVisible()
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await expect(picker(page)).toBeVisible()
  await expect(kind(page, 'One piece')).toBeVisible()
  await expect(kind(page, 'Animation')).toBeVisible()
  await expect(kind(page, 'Random mix')).toHaveCount(0) // structures cannot mix or join
  await expect(kind(page, 'Joins with neighbours')).toHaveCount(0)
  await clickPiece(page, pickerGrid(page), 2, 1)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures).toEqual({ hq: { sheet: 'blds', cell: [2, 1] } })
  expect(await game.problems(id)).toEqual([])
  // The card's sample shows the structure at its real 2 x 2 footprint, drawn from that piece.
  const sample = page.getByRole('img', { name: /^Headquarters on the ground/ })
  await expect(sample).toBeVisible()
  const box = (await sample.boundingBox())!
  expect(box.width).toBeGreaterThan(box.height * 0.9) // two structures side by side: wider than tall
  const px = await sample.evaluate((el) => {
    const c = el as HTMLCanvasElement
    const dpr = c.width / c.getBoundingClientRect().width
    const s = c.getBoundingClientRect().height / 3 // 2 rows of structure + 1 row of ground
    const d = c.getContext('2d')!.getImageData(Math.round(1.5 * s * dpr), Math.round(0.7 * s * dpr), 1, 1).data // inside the first 2 x 2 footprint
    return [d[0], d[1], d[2]]
  })
  expect(hex(px)).toBe(hex(pieceColor(2, 1)))
  await page.getByRole('button', { name: 'Remove picture' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures).toBeUndefined()
})

test('animation; a different picture for an owner (sides come from the maps), kept when the general picture changes', async ({ page, request, game }) => {
  const id = await setup(request, game)
  await page.goto(editorUrl(id, 'structures', 'city'))
  await expect(page.getByText('Choose a picture first; then each owner can get its own.')).toBeVisible()
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await clickPiece(page, pickerGrid(page), 0, 0)
  await useIt(page).click()
  await saved(page)
  const owners = page.getByRole('region', { name: 'A different picture for an owner' })
  await expect(owners.getByText('Blue Army')).toBeVisible()
  await expect(owners.getByText('Red Army')).toBeVisible()
  await owners.getByRole('button', { name: 'Choose… Red Army' }).click()
  await kind(page, 'Animation').click()
  for (const [c, r] of [[0, 2], [1, 2]]) await clickPiece(page, pickerGrid(page), c, r)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures.city).toEqual({
    sheet: 'blds', cell: [0, 0], sides: { red: { sheet: 'blds', frames: [[0, 2], [1, 2]], frameMs: 400 } },
  })
  await page.getByRole('button', { name: 'Change picture…' }).first().click()
  await clickPiece(page, pickerGrid(page), 3, 1)
  await useIt(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures.city).toMatchObject({ cell: [3, 1], sides: { red: { frames: [[0, 2], [1, 2]] } } })
  await owners.getByRole('button', { name: 'Remove Red Army' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures.city).toEqual({ sheet: 'blds', cell: [3, 1] })
  expect(await game.problems(id)).toEqual([])
})

test('a picture per style, duplicating copies pictures, deleting a kind clears them, and the Art screen lists structures as users', async ({ page, request, game }) => {
  const id = await setup(request, game)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const put = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: { ...doc.game, styles: [{ id: 'classic', name: 'Classic' }, { id: 'night', name: 'Night' }] } } })
  expect(put.ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'structures', 'factory'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await clickPiece(page, pickerGrid(page), 1, 1)
  await useIt(page).click()
  await page.getByLabel('For style:').selectOption('night')
  await page.getByRole('radio', { name: 'No picture in this style' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').structures.factory).toEqual({ sheet: 'blds', cell: [1, 1] })
  expect(readDisk(id, 'data/graphics.json').styles.night.structures).toEqual({ factory: null })
  expect(await game.problems(id)).toEqual([])

  await page.getByLabel('For style:').selectOption('')
  await page.getByRole('button', { name: 'Duplicate' }).click()
  await saved(page)
  const g = readDisk(id, 'data/graphics.json')
  expect(g.structures.factory_copy).toEqual({ sheet: 'blds', cell: [1, 1] })
  expect(g.styles.night.structures.factory_copy).toBeNull()

  await page.goto(editorUrl(id, 'art', 'blds'))
  await expect(page.getByRole('list', { name: 'Used by' }).getByText('Structure: Factory', { exact: true })).toBeVisible()
  await expect(page.getByRole('list', { name: 'Used by' }).getByText('Structure: Factory copy', { exact: true })).toBeVisible()

  await page.goto(editorUrl(id, 'structures', 'factory_copy'))
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the structure/ })
  await expect(dlg).toContainText('Its picture is cleared')
  await dlg.getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  const after = readDisk(id, 'data/graphics.json')
  expect(after.structures.factory_copy).toBeUndefined()
  expect(after.styles.night.structures.factory_copy).toBeUndefined()
  expect(after.structures.factory).toBeDefined()
})
