// Art in the real player, and the demos / templates with their starter art opening cleanly in the editor.
import { readFileSync } from 'node:fs'
import { bareGame, clickPiece, hex, makePng, mapPixel, picker, pickerGrid, seedArt, type Rgba } from './artkit'
import { diskPath, editorUrl, expect, requirePlayer, saved, test } from './fixtures'
import { canvas, openMap } from './mapkit'
import { countColor, decodePng } from './png'

const panel = (page: import('@playwright/test').Page) => page.getByRole('dialog', { name: 'Play', exact: true })
const frameShot = async (page: import('@playwright/test').Page) => decodePng(await panel(page).locator('.play-frame').screenshot())

test('Play shows the art in the real player: terrain and unit pictures chosen in the editor', async ({ page, request, game }) => {
  requirePlayer()
  const MAGENTA: Rgba = [250, 0, 250, 255], CYAN: Rgba = [0, 250, 250, 255]
  const rgb = (c: Rgba): [number, number, number] => [c[0], c[1], c[2]]
  const id = await bareGame(request, game, 'strategy-rpg', 'Play Art')
  // Two flat pieces; the pictures are chosen in the editor with the picker, like a person would.
  await seedArt(request, id, { flat: makePng(48, 24, (x) => (x < 24 ? MAGENTA : CYAN)) }, { sheets: [{ id: 'flat', file: 'art/flat.png', cellWidth: 24, cellHeight: 24 }] })
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await clickPiece(page, pickerGrid(page), 0, 0)
  await picker(page).getByRole('button', { name: 'Use this picture' }).click()
  await page.goto(editorUrl(id, 'units', 'lord'))
  await page.getByRole('button', { name: 'Choose picture…' }).click()
  await clickPiece(page, pickerGrid(page), 1, 0)
  await picker(page).getByRole('button', { name: 'Use this picture' }).click()
  await saved(page)
  await page.getByRole('button', { name: 'Play', exact: true }).click() // Play saves first, then starts the first map
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  // Six forest squares of 24 x 24 logical pixels, scaled up: thousands of magenta pixels; the lord is a cyan sprite.
  await expect.poll(async () => countColor(await frameShot(page), rgb(MAGENTA), 4), { timeout: 20_000 }).toBeGreaterThan(2000)
  expect(countColor(await frameShot(page), rgb(CYAN), 4)).toBeGreaterThan(150)
})

for (const name of ['demo-srpg', 'demo-economy'] as const) {
  test(`${name} opens in the editor with its starter art: images, pictures, canvas, no problems, nothing rewritten`, async ({ page, request }) => {
    const before = readFileSync(diskPath(name, 'data/graphics.json'), 'utf8')
    const doc = await (await request.get(`/api/projects/${name}`)).json()
    const g = doc.graphics
    await page.goto(editorUrl(name, 'art'))
    await expect(page.getByRole('heading', { name: 'Art', level: 1 })).toBeVisible()
    for (const sheet of g.sheets) await expect(page.getByRole('list', { name: 'Art' }).getByRole('button', { name: new RegExp(sheet.name ?? sheet.id, 'i') }).first()).toBeVisible()
    await expect(page.getByRole('button', { name: /No problems/ })).toBeVisible()
    await expect(page.getByRole('region', { name: 'Unused files' })).toHaveCount(0)
    await expect(page.getByTestId('grid-summary')).toBeVisible()
    // A terrain with a picture shows it on its card; a joining one too.
    const firstTerrain = Object.keys(g.terrain)[0]
    await page.goto(editorUrl(name, 'terrain', firstTerrain))
    await expect(page.getByRole('button', { name: 'Change picture…' })).toBeVisible()
    const unit = Object.keys(g.units)[0]
    await page.goto(editorUrl(name, 'units', unit))
    await expect(page.getByRole('button', { name: 'Change picture…' }).first()).toBeVisible()
    // The map canvas draws art: a square of the first terrain is not the plain colour of the terrain.
    const map = doc.game.maps[0].id
    await openMap(page, name, map)
    await expect(canvas(page)).toHaveAttribute('data-view', /.+/)
    const m = doc.maps[map]
    const t = doc.terrain.find((x: any) => x.id === m.legend[m.tiles[0][0]])
    expect(hex(await mapPixel(page, 0, 0, 0.1, 0.1))).not.toBe(t.color)
    // Looking around changed nothing on disk.
    await page.waitForTimeout(1200)
    expect(readFileSync(diskPath(name, 'data/graphics.json'), 'utf8')).toBe(before)
  })
}

test('every template opens with its starter art and no problems', async ({ page, game }) => {
  for (const template of ['strategy-rpg', 'economy-tactics', 'blank']) {
    const id = await game.create(template, `Starter ${template}`)
    expect(await game.problems(id)).toEqual([])
    await page.goto(editorUrl(id, 'art'))
    await expect(page.getByRole('list', { name: 'Art' }).getByRole('button').first()).toBeVisible()
    await expect(page.getByRole('button', { name: /No problems/ })).toBeVisible()
  }
})
