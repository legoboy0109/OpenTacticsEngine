// 3D models for units and structures in a 3D isometric style: choose or upload a .glb, set its facts, switch back to a
// picture, and the player starting such a style without errors.
import { readFileSync } from 'node:fs'
import path from 'node:path'
import type { APIRequestContext, Page } from '@playwright/test'
import { bareGame } from './artkit'
import { diskPath, editorUrl, expect, readDisk, requirePlayer, saved, test } from './fixtures'
import { openMap } from './mapkit'

const figure = path.resolve(import.meta.dirname, '../../../../tests/fixtures/models/figure.glb')

/** A .glb that only has a JSON chunk: enough for the editor to read names from. */
function tinyGlb(json: object): Buffer {
  let text = JSON.stringify(json)
  while (text.length % 4) text += ' '
  const body = Buffer.from(text)
  const head = Buffer.alloc(20)
  head.write('glTF', 0); head.writeUInt32LE(2, 4); head.writeUInt32LE(20 + body.length, 8)
  head.writeUInt32LE(body.length, 12); head.writeUInt32LE(0x4e4f534a, 16)
  return Buffer.concat([head, body])
}

/** A game with no art and a 3D isometric style "solid" next to the template's own styles. */
async function setup(request: APIRequestContext, game: any): Promise<string> {
  const id = await bareGame(request, game, 'economy-tactics', 'Models')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const styles = [...doc.game.styles, { id: 'solid', name: 'Solid', view: 'isometric-3d' }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: { ...doc.game, styles } } })).ok()).toBeTruthy()
  return id
}

const forStyle = (page: Page, name: string) => page.getByLabel('For style:').selectOption({ label: name })
const asModel = (page: Page) => page.getByRole('radio', { name: '3D model', exact: true })
const asPicture = (page: Page) => page.getByRole('radio', { name: 'Picture', exact: true })
const upload = (page: Page, file: string | { name: string; mimeType: string; buffer: Buffer }) => page.getByTestId('upload-model-input').setInputFiles(file)

test('@smoke a unit gets a 3D model: upload, height and turn, saved as expected, no problems, Play starts, back to a picture', async ({ page, request, game }) => {
  requirePlayer()
  const id = await setup(request, game)
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()) })
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await forStyle(page, 'Solid')
  await asModel(page).click()
  await expect(page.getByText('The editor shows no preview of a 3D model. Press Play to see it in this style.')).toBeVisible()
  await upload(page, figure)
  await expect(page.getByLabel('Model file')).toHaveValue('models/figure.glb')
  await page.getByLabel('Model height (squares)', { exact: true }).fill('1.2')
  await page.getByLabel('Model turn (degrees)', { exact: true }).fill('90')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles).toEqual({ solid: { units: { infantry: { model: 'models/figure.glb', height: 1.2, turn: 90 } } } })
  expect(readFileSync(diskPath(id, 'models/figure.glb')).equals(readFileSync(figure))).toBe(true)
  expect(await game.problems(id)).toEqual([])

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

  // Back to a picture: the entry goes, and undo brings it back exactly.
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await forStyle(page, 'Solid')
  await expect(asModel(page)).toHaveAttribute('aria-checked', 'true')
  await asPicture(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles).toBeUndefined()
  await expect(page.getByLabel('Model file')).toHaveCount(0)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles.solid.units.infantry).toEqual({ model: 'models/figure.glb', height: 1.2, turn: 90 })
})

test('a structure gets a model from the project\'s files, and the choice is not offered in other kinds of view', async ({ page, request, game }) => {
  const id = await setup(request, game)
  expect((await request.put(`/api/projects/${id}/models/fort.glb`, { data: readFileSync(figure) })).status()).toBe(201)
  await page.goto(editorUrl(id, 'structures', 'hq'))
  await expect(asModel(page)).toHaveCount(0) // "All styles" is a top-down view
  await forStyle(page, 'Isometric')
  await expect(asModel(page)).toHaveCount(0)
  await forStyle(page, 'Solid')
  await asModel(page).click()
  await page.getByLabel('Model file').selectOption('models/fort.glb')
  await page.getByLabel('Model height (squares)', { exact: true }).fill('2')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles).toEqual({ solid: { structures: { hq: { model: 'models/fort.glb', height: 2 } } } })
  expect(await game.problems(id)).toEqual([])
  await page.getByLabel('Model height (squares)', { exact: true }).fill('')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles.solid.structures.hq).toEqual({ model: 'models/fort.glb' })
  await asPicture(page).click()
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles).toBeUndefined()
})

test('names from the file fill the dropdowns; only what differs from the defaults is written; unknown keys stay', async ({ page, request, game }) => {
  const id = await setup(request, game)
  const glb = tinyGlb({ asset: { version: '2.0' }, materials: [{ name: 'cloth' }, { name: 'side' }], animations: [{ name: 'Idle' }, { name: 'Walk' }, { name: 'Slash' }] })
  expect((await request.put(`/api/projects/${id}/models/hero.glb`, { data: glb })).status()).toBe(201)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const graphics = { sheets: [], styles: { solid: { units: { infantry: { model: 'models/hero.glb', future: { keep: 1 } } } } } }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics } })).ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await forStyle(page, 'Solid')
  const anim = (name: string) => page.getByLabel(name, { exact: true })
  await expect(page.getByLabel('Side colour material')).toHaveValue('side')
  await expect(anim('Idle')).toHaveValue('Idle')
  await expect(anim('Move')).toHaveValue('') // the file has no "Move": shown as None, nothing written
  await expect(anim('Move').locator('option')).toHaveText(['None', 'Idle', 'Walk', 'Slash'])
  await anim('Move').selectOption('Walk')
  await anim('Attack').selectOption('Slash')
  await page.getByLabel('Side colour material').selectOption('cloth')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles.solid.units.infantry).toEqual({
    model: 'models/hero.glb', future: { keep: 1 }, sideMaterial: 'cloth', animations: { move: 'Walk', attack: 'Slash' },
  })
  await anim('Idle').selectOption('') // None writes an empty name...
  await anim('Idle').selectOption('Idle') // ...and the default name writes nothing again
  await page.getByLabel('Side colour material').selectOption('side')
  await saved(page)
  expect(readDisk(id, 'data/graphics.json').styles.solid.units.infantry.sideMaterial).toBeUndefined()
  expect(readDisk(id, 'data/graphics.json').styles.solid.units.infantry.animations).toEqual({ move: 'Walk', attack: 'Slash' })
})

test('a missing model file shows in the problems list, and a file that is not a model is refused', async ({ page, request, game }) => {
  const id = await setup(request, game)
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const graphics = { sheets: [], styles: { solid: { units: { infantry: { model: 'models/gone.glb' } } } } }
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics } })).ok()).toBeTruthy()
  await page.goto(editorUrl(id, 'units', 'infantry'))
  await forStyle(page, 'Solid')
  await expect(page.getByLabel('Model file')).toHaveValue('models/gone.glb')
  await expect(page.getByRole('option', { name: 'models/gone.glb (file not found)' })).toHaveCount(1)
  await page.getByRole('button', { name: /problem/i }).click()
  await expect(page.getByRole('region', { name: 'Problems' })).toContainText('model file "models/gone.glb" does not exist')
  await upload(page, { name: 'notes.glb', mimeType: 'model/gltf-binary', buffer: Buffer.from('this is not a model at all, sorry') })
  await expect(page.getByRole('alert').filter({ hasText: '“notes.glb” is not a .glb model' })).toBeVisible()
  expect((await request.get(`/api/projects/${id}/models`)).ok()).toBeTruthy()
  expect((await (await request.get(`/api/projects/${id}/models`)).json()).models).toEqual([])
})
