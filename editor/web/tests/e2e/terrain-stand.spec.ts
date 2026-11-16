// The "Standing part" of a terrain in a 3D style (docs/GRAPHICS.md "Standing terrain"): an upright picture (a random mix) or
// 3D models (uploaded and from the game's files), width / height / turn / scatter, the files on disk exactly as expected, the
// game loading and Play starting, the ground picture untouched, and the section hidden in 2D styles.
import { readFileSync } from 'node:fs'
import path from 'node:path'
import type { APIRequestContext, Page } from '@playwright/test'
import { bareGame, clickPiece, picker, pickerGrid, seedArt, sheetPng } from './artkit'
import { diskPath, editorUrl, expect, readDisk, requirePlayer, saved, test } from './fixtures'
import { openMap } from './mapkit'

const figure = path.resolve(import.meta.dirname, '../../../../tests/fixtures/models/figure.glb')
const bend = path.resolve(import.meta.dirname, '../../../../tests/fixtures/models/bend.glb')

/** A game with an image "ground" (the forest's picture in the 3D styles), an image "trees" and two 3D styles next to the template's. */
async function setup(request: APIRequestContext, game: any): Promise<string> {
  const id = await bareGame(request, game, 'economy-tactics', 'Standing')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const styles = [...doc.game.styles, { id: 'solid', name: 'Solid', view: 'isometric-3d' }, { id: 'orbit', name: 'Orbit', view: '3d' }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: { ...doc.game, styles } } })).ok()).toBeTruthy()
  await seedArt(request, id, { ground: sheetPng(4, 1), trees: sheetPng(4, 2) }, {
    sheets: [{ id: 'ground', file: 'art/ground.png', cellWidth: 24, cellHeight: 24, name: 'Ground' }, { id: 'trees', file: 'art/trees.png', cellWidth: 24, cellHeight: 24, name: 'Trees' }],
    styles: { solid: { terrain: { forest: { sheet: 'ground', cell: [0, 0] } } } },
  })
  return id
}

const forStyle = (page: Page, name: string) => page.getByLabel('For style:').selectOption({ label: name })
const part = (page: Page) => page.getByRole('region', { name: 'Standing part' })
const asKind = (page: Page, name: string) => part(page).getByRole('radio', { name, exact: true })
const useIt = (page: Page) => picker(page).getByRole('button', { name: 'Use this picture' })
const stand = (id: string, style = 'solid') => readDisk(id, 'data/graphics.json').styles[style].terrain.forest

test('@smoke an upright picture: a random mix, width and scatter; the ground picture stays; no problems', async ({ page, request, game }) => {
  const id = await setup(request, game)
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await forStyle(page, 'Solid')
  await expect(part(page)).toBeVisible()
  await expect(part(page)).toContainText('In 3D styles a terrain can have something standing on its ground, like trees or a peak. The ground picture stays underneath.')
  await expect(asKind(page, 'Nothing')).toHaveAttribute('aria-checked', 'true')
  await asKind(page, 'An upright picture').click()
  await expect(part(page).getByText('No picture chosen yet.').first()).toBeVisible()
  expect(readDisk(id, 'data/graphics.json').styles.solid.terrain.forest).toEqual({ sheet: 'ground', cell: [0, 0] }) // nothing written until a picture is chosen

  await page.getByTestId('stand-choose-picture').click()
  await expect(picker(page)).toBeVisible()
  await expect(picker(page).getByRole('radio', { name: 'Joins with neighbours' })).toHaveCount(0) // a standing picture does not join
  await picker(page).getByLabel('Image', { exact: true }).selectOption('trees')
  await picker(page).getByRole('radio', { name: 'Random mix' }).click()
  for (const [c, r] of [[0, 0], [1, 0], [2, 1]]) await clickPiece(page, pickerGrid(page), c, r)
  await expect(page.getByTestId('chosen-summary')).toHaveText('3 pieces')
  await useIt(page).click()
  await expect(picker(page)).toBeHidden()
  await part(page).getByLabel('Picture width (pixels)').fill('30')
  await part(page).getByRole('checkbox', { name: /Vary position and turn/ }).check()
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [0, 0], stand: { sheet: 'trees', variants: [[0, 0], [1, 0], [2, 1]], width: 30, scatter: true } })
  expect(await game.problems(id)).toEqual([])

  // Changing the ground picture keeps the standing part.
  await page.getByRole('button', { name: 'Change picture…' }).first().click()
  await clickPiece(page, pickerGrid(page), 2, 0)
  await useIt(page).click()
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [2, 0], stand: { sheet: 'trees', variants: [[0, 0], [1, 0], [2, 1]], width: 30, scatter: true } })

  // Empty width and "Nothing" take the settings out again; undo brings them back.
  await part(page).getByLabel('Picture width (pixels)').fill('')
  await saved(page)
  expect(stand(id).stand.width).toBeUndefined()
  await asKind(page, 'Nothing').click()
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [2, 0] })
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(stand(id).stand).toEqual({ sheet: 'trees', variants: [[0, 0], [1, 0], [2, 1]], scatter: true })
})

test('3D models: upload, add a file of the game, height and turn, remove; Play starts the style', async ({ page, request, game }) => {
  requirePlayer()
  const id = await setup(request, game)
  expect((await request.put(`/api/projects/${id}/models/bend.glb`, { data: readFileSync(bend) })).status()).toBe(201)
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()) })
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await forStyle(page, 'Orbit') // no picture of its own in this style yet
  await expect(part(page)).toContainText('First give this terrain a picture of its own in this style. The standing part stands on it.')
  await forStyle(page, 'Solid')
  await asKind(page, 'A 3D model').click()
  await expect(part(page).getByText('No model chosen yet.')).toBeVisible()
  await part(page).getByTestId('upload-stand-model-input').setInputFiles(figure)
  await expect(part(page).getByRole('list', { name: 'Model files' })).toContainText('figure.glb')
  await part(page).getByLabel('Add a model file…').selectOption('models/bend.glb')
  await part(page).getByLabel('Model height (squares)', { exact: true }).fill('1.5')
  await part(page).getByLabel('Model turn (degrees)', { exact: true }).fill('90')
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [0, 0], stand: { variants: ['models/figure.glb', 'models/bend.glb'], height: 1.5, turn: 90 } })
  expect(readFileSync(diskPath(id, 'models/figure.glb')).equals(readFileSync(figure))).toBe(true)
  expect(await game.problems(id)).toEqual([])

  // The game loads and the style plays.
  await openMap(page, id, 'crossroads')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await panel.getByLabel('Style').selectOption('solid')
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  await page.waitForTimeout(1500)
  expect(errors).toEqual([])
  await page.keyboard.press('Escape')

  // Remove one file: one file is written as "model" again, with the settings kept.
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await forStyle(page, 'Solid')
  await part(page).getByRole('button', { name: 'Remove figure.glb' }).click()
  await saved(page)
  expect(stand(id).stand).toEqual({ model: 'models/bend.glb', height: 1.5, turn: 90 })
  // The helper refuses to delete a model the saved style still uses.
  expect((await request.delete(`/api/projects/${id}/models/bend.glb`)).status()).toBe(409)
  await part(page).getByRole('button', { name: 'Remove bend.glb' }).click()
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [0, 0] })
})

test('hidden in 2D styles; a picture and models are not mixed; unknown keys of a stand stay', async ({ page, request, game }) => {
  const id = await setup(request, game)
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await expect(part(page)).toHaveCount(0) // "All styles" is a top-down view
  await forStyle(page, 'Isometric')
  await expect(part(page)).toHaveCount(0) // 2D isometric
  await forStyle(page, 'Solid')
  await expect(part(page)).toBeVisible()
  await forStyle(page, 'Orbit')
  await expect(part(page)).toBeVisible() // full 3D, shown but needs its own ground picture first

  // A stand with a key the editor does not know keeps it through an edit.
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const graphics = { ...doc.graphics, styles: { solid: { terrain: { forest: { sheet: 'ground', cell: [0, 0], stand: { sheet: 'trees', cell: [1, 0], future: { keep: 1 } } } } } } }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics } })).ok()).toBeTruthy()
  await page.reload()
  await forStyle(page, 'Solid')
  await expect(asKind(page, 'An upright picture')).toHaveAttribute('aria-checked', 'true')
  await part(page).getByLabel('Picture width (pixels)').fill('20')
  await saved(page)
  expect(stand(id).stand).toEqual({ sheet: 'trees', cell: [1, 0], future: { keep: 1 }, width: 20 })
  // Switching to models drops the picture (and the width), as the file format does not mix them.
  await asKind(page, 'A 3D model').click()
  await saved(page)
  expect(stand(id)).toEqual({ sheet: 'ground', cell: [0, 0] })
})
