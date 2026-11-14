// Kinds of view per style, templates with an Isometric style, the Heights tool and the Play style picker (docs/VIEWS.md).
import type { Page } from '@playwright/test'
import { bareGame } from './artkit'
import { editorUrl, expect, readDisk, requirePlayer, saved, test } from './fixtures'
import { canvas, clickCell, dragThrough, openMap, row, tool } from './mapkit'

const ISO_STYLE = { id: 'isometric', name: 'Isometric', view: 'isometric', background: '#161a24' }

async function addStyle(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'Add a style' }).click()
  const dlg = page.getByRole('dialog', { name: 'New visual style' })
  await dlg.getByLabel('Name of the style').fill(name)
  await dlg.getByRole('button', { name: 'Create' }).click()
}

test('@smoke a style switches to Isometric and back; the file is exactly as expected and the game loads', async ({ page, game }) => {
  const id = await game.create('blank', 'Views')
  expect(readDisk(id, 'game.json').styles).toBeUndefined()
  await page.goto(editorUrl(id, 'game'))
  await addStyle(page, 'Angled')
  const kind = page.getByLabel('Angled: Kind of view')
  await expect(kind).toHaveValue('top-down')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'angled', name: 'Angled' }]) // top-down writes no view key

  await kind.selectOption('isometric')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'angled', name: 'Angled', view: 'isometric' }])
  expect(await game.problems(id)).toEqual([])

  // The four sizes sit in a collapsed Advanced area; the standard sizes are the placeholders.
  const sizes = page.locator('.subcard').getByText('Advanced')
  await expect(page.getByLabel('Angled: Square width')).toBeHidden()
  await sizes.click()
  await expect(page.getByLabel('Angled: Square width')).toHaveAttribute('placeholder', '48')
  await expect(page.getByLabel('Angled: Square height')).toHaveAttribute('placeholder', '24')
  await expect(page.getByLabel('Angled: Height step')).toHaveAttribute('placeholder', '12')
  await expect(page.getByLabel('Angled: Unit width')).toHaveAttribute('placeholder', '24')

  // An odd number or one out of range is refused with a sentence and never saved.
  await page.getByLabel('Angled: Square width').fill('63')
  await expect(page.getByText('Use an even number.')).toBeVisible()
  await page.getByLabel('Angled: Square width').fill('9000')
  await expect(page.getByText('At most 512.')).toBeVisible()
  await page.getByLabel('Angled: Square width').fill('64')
  await expect(page.getByLabel('Angled: Square height')).toHaveAttribute('placeholder', '32') // follows the width
  await page.getByLabel('Angled: Height step').fill('0')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'angled', name: 'Angled', view: 'isometric', isometric: { tileWidth: 64, heightStep: 0 } }])
  expect(await game.problems(id)).toEqual([])

  // Emptying a box leaves the key out again.
  await page.getByLabel('Angled: Height step').fill('')
  await saved(page)
  expect(readDisk(id, 'game.json').styles[0].isometric).toEqual({ tileWidth: 64 })

  // Back to top-down: the file is what it was at the start.
  await kind.selectOption('top-down')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'angled', name: 'Angled' }])
  expect(await game.problems(id)).toEqual([])
})

test('@smoke a style switches to 3D isometric and back; the file is exactly as expected, the game loads, Play offers it', async ({ page, game }) => {
  const id = await game.create('blank', 'Views3d')
  await page.goto(editorUrl(id, 'game'))
  await addStyle(page, 'Turning')
  const kind = page.getByLabel('Turning: Kind of view')
  await kind.selectOption('isometric-3d')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'turning', name: 'Turning', view: 'isometric-3d' }])
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByText(/terrain picture lies flat on top of the block/)).toBeVisible()
  // The same four sizes as Isometric.
  await page.locator('.subcard').getByText('Advanced').click()
  await expect(page.getByLabel('Turning: Unit width')).toHaveAttribute('placeholder', '24')
  await page.getByLabel('Turning: Square width').fill('64')
  await saved(page)
  expect(readDisk(id, 'game.json').styles[0]).toEqual({ id: 'turning', name: 'Turning', view: 'isometric-3d', isometric: { tileWidth: 64 } })
  expect(await game.problems(id)).toEqual([])
  await kind.selectOption('isometric')
  await saved(page)
  expect(readDisk(id, 'game.json').styles[0]).toEqual({ id: 'turning', name: 'Turning', view: 'isometric', isometric: { tileWidth: 64 } })
  await kind.selectOption('top-down')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'turning', name: 'Turning' }])
  expect(await game.problems(id)).toEqual([])
})

test('a style switches to Full 3D and back: no isometric block or measure fields, the file is exactly as expected', async ({ page, game }) => {
  const id = await game.create('blank', 'ViewsFull3d')
  await page.goto(editorUrl(id, 'game'))
  await addStyle(page, 'Orbit')
  const kind = page.getByLabel('Orbit: Kind of view')
  await kind.selectOption('isometric')
  await page.locator('.subcard').getByText('Advanced').click()
  await page.getByLabel('Orbit: Square width').fill('64')
  await saved(page)
  await kind.selectOption('3d')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'orbit', name: 'Orbit', view: '3d' }])
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByTestId('view-full3d-help')).toHaveText(/real blocks under a camera/)
  await expect(page.getByLabel('Orbit: Square width')).toHaveCount(0)
  await expect(page.locator('.subcard').getByText('Advanced')).toHaveCount(0)
  await kind.selectOption('top-down')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'orbit', name: 'Orbit' }])
})

test('@smoke the two starting templates have an Isometric style; the blank one has none', async ({ page, game, request }) => {
  for (const template of ['strategy-rpg', 'economy-tactics']) {
    const id = await game.create(template, `Iso ${template}`)
    expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'classic', name: 'Classic' }, ISO_STYLE])
    expect(await game.problems(id)).toEqual([])
    const graphics = (await (await request.get(`/api/projects/${id}`)).json()).graphics
    expect(graphics.styles.isometric.terrain).toBeTruthy() // the generated isometric art came with it
  }
  expect(readDisk(await game.create('blank', 'Iso blank'), 'game.json').styles).toBeUndefined()
  const id = await game.create('strategy-rpg', 'Iso shown')
  await page.goto(editorUrl(id, 'game'))
  await expect(page.getByLabel('Isometric: Kind of view')).toHaveValue('isometric')
  await expect(page.getByLabel('Classic: Kind of view')).toHaveValue('top-down')
})

test('picture cards: an isometric style inherits no general picture and shows the piece itself', async ({ page, game, request }) => {
  const id = await game.create('strategy-rpg', 'Pictures')
  await page.goto(editorUrl(id, 'terrain', 'forest'))
  await page.getByLabel('For style:').selectOption('isometric')
  await expect(page.getByText(/does not use the pictures from “All styles”/)).toBeVisible()
  await expect(page.getByRole('radio', { name: 'Same as the others' })).toHaveCount(0)
  // The template draws its own isometric forest, so "A picture of its own" is on and the preview is the piece (1 piece, not a patch).
  await expect(page.getByRole('radio', { name: 'A picture of its own' })).toHaveAttribute('aria-checked', 'true')
  await page.getByRole('radio', { name: 'No picture in this style' }).click()
  await saved(page)
  const graphics = (await (await request.get(`/api/projects/${id}`)).json()).graphics
  expect(graphics.styles.isometric.terrain.forest).toBeUndefined() // removed, not written as null
  await page.getByLabel('For style:').selectOption('classic')
  await expect(page.getByRole('radio', { name: 'Same as the others' })).toBeVisible()
  await expect(page.getByText(/does not use the pictures/)).toHaveCount(0)
})

test('the map editor stays top-down for an isometric style, and says so', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'MapNote')
  await openMap(page, id, 'ford')
  await expect(page.getByText('This map editor shows the top-down view. Play shows the game in its styles.')).toBeVisible()
  await page.getByLabel('Style to preview').selectOption('isometric')
  await expect(canvas(page)).toBeVisible()
  // The canvas still draws a square grid with the general pictures: a square is as wide as it is tall and nothing crashed.
  const [scale] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  expect(scale).toBeGreaterThan(4)
  await expect(page.getByRole('alert')).toHaveCount(0)
})

const heightsOf = (id: string, map: string): number[][] | undefined => readDisk(id, `maps/${map}.json`).heights

test('@smoke the Heights tool raises squares; one drag is one undo step; maps without heights stay without', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Heights')
  const before = JSON.stringify(readDisk(id, 'maps/crossroads.json'))
  await openMap(page, id, 'crossroads')
  await page.keyboard.press('h')
  await expect(tool(page, 'Heights')).toHaveAttribute('aria-pressed', 'true')
  await expect(page.getByRole('radio', { name: 'Raise' })).toHaveAttribute('aria-checked', 'true')

  await clickCell(page, 1, 1)
  await saved(page)
  const h1 = heightsOf(id, 'crossroads')!
  expect(h1[1][1]).toBe(1)
  expect(h1.flat().filter((v) => v !== 0)).toHaveLength(1)

  // A drag over three squares, coming back over the first, raises each once.
  await dragThrough(page, [[3, 2], [4, 2], [5, 2], [3, 2]])
  await saved(page)
  expect(heightsOf(id, 'crossroads')![2].slice(3, 6)).toEqual([1, 1, 1])

  // One undo takes back the whole drag, the next one the click; then the file has no heights again, byte for byte.
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(heightsOf(id, 'crossroads')![2].slice(3, 6)).toEqual([0, 0, 0])
  expect(heightsOf(id, 'crossroads')![1][1]).toBe(1)
  await page.getByRole('button', { name: 'Undo' }).click()
  await saved(page)
  expect(heightsOf(id, 'crossroads')).toBeUndefined()
  expect(JSON.stringify(readDisk(id, 'maps/crossroads.json'))).toBe(before)
  expect(await game.problems(id)).toEqual([])
})

test('Heights: Lower, Set to, the limits, and the height shown in Select', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Heights2')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Heights').click()
  await page.getByRole('radio', { name: 'Lower' }).click()
  await clickCell(page, 0, 0)
  await saved(page)
  expect(heightsOf(id, 'crossroads')![0][0]).toBe(-1)

  await page.getByRole('radio', { name: 'Set to' }).click()
  await page.getByLabel('Height', { exact: true }).fill('40')
  await expect(page.getByText('At most 32.')).toBeVisible()
  await page.getByLabel('Height', { exact: true }).fill('32')
  await dragThrough(page, row(3, 0, 2))
  await saved(page)
  expect(heightsOf(id, 'crossroads')![3].slice(0, 3)).toEqual([32, 32, 32])
  await page.getByRole('radio', { name: 'Raise' }).click()
  await clickCell(page, 0, 3)
  await page.waitForTimeout(300)
  expect(heightsOf(id, 'crossroads')![3][0]).toBe(32) // the top

  await tool(page, 'Select').click()
  await clickCell(page, 1, 3)
  await expect(page.getByTestId('square-height')).toHaveText('32')
  await clickCell(page, 7, 5)
  await expect(page.getByTestId('square-height')).toHaveText('0')
  expect(await game.problems(id)).toEqual([])
})

test('resizing a map keeps its heights with the squares', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'HeightsResize')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Heights').click()
  await clickCell(page, 0, 0)
  await saved(page)
  const m = readDisk(id, 'maps/crossroads.json')
  await page.getByRole('tab', { name: 'Map', exact: true }).click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill(String(m.grid.width + 1))
  await dlg.getByLabel('Height').fill(String(m.grid.height + 1))
  await dlg.getByRole('radio', { name: 'bottom right' }).click()
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await saved(page)
  const h = heightsOf(id, 'crossroads')!
  expect(h).toHaveLength(m.grid.height + 1)
  expect(h[1][1]).toBe(1) // the old (0, 0) moved by one square
  expect(h[0].every((v) => v === 0)).toBe(true)
  expect(await game.problems(id)).toEqual([])
})

test('Play offers the styles and starts in the chosen one', async ({ page, game, request }) => {
  requirePlayer()
  const id = await bareGame(request, game, 'strategy-rpg', 'PlayStyle')
  await openMap(page, id, 'ford')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(panel.getByLabel('Style')).toHaveValue('classic')
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford`)
  await panel.getByLabel('Style').selectOption('isometric')
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford&style=isometric`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})

test('Play offers a 3D isometric style, starts it, and the player loads without errors', async ({ page, game, request }) => {
  requirePlayer()
  const id = await bareGame(request, game, 'strategy-rpg', 'Play3d')
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const styles = [...doc.game.styles, { id: 'turning', name: 'Turning', view: 'isometric-3d' }]
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, game: { ...doc.game, styles } } })).ok()).toBeTruthy()
  const errors: string[] = []
  page.on('pageerror', (e) => errors.push(e.message))
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()) })
  await openMap(page, id, 'ford')
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  const panel = page.getByRole('dialog', { name: 'Play', exact: true })
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await panel.getByLabel('Style').selectOption('turning')
  await expect(panel.locator('iframe')).toHaveAttribute('src', `/player/embed.html?project=${id}&map=ford&style=turning`)
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
  expect(errors).toEqual([])
})
