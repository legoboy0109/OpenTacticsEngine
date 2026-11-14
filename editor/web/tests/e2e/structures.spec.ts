// The Structures section against the real helper: create kinds, set every field, sizes, cascades, the Terrain form's pointer,
// and the Game screen's per-style structure colours.
import type { Page } from '@playwright/test'
import { bareGame, expectPixel } from './artkit'
import { editorUrl, expect, readDisk, saved, test } from './fixtures'
import { openMap } from './mapkit'

const open = async (page: Page, id: string, item?: string) => {
  await page.goto(editorUrl(id, 'structures', item))
  await expect(page.getByRole('heading', { name: 'Structures', level: 1 })).toBeVisible()
}
const disk = (id: string) => readDisk(id, 'data/structures.json').structures as Array<Record<string, any>>

async function createKind(page: Page, name: string): Promise<void> {
  await page.getByRole('button', { name: 'New structure' }).click()
  await page.getByRole('dialog', { name: 'New structure' }).getByLabel('Name of the structure').fill(name)
  await page.getByRole('dialog', { name: 'New structure' }).getByRole('button', { name: 'Create' }).click()
  await expect(page.getByRole('heading', { name, level: 2 })).toBeVisible()
}

test('@smoke create a structure kind and set every field; the saved file is exactly that and the game still loads', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Kinds')
  await open(page, id)
  await createKind(page, 'Watch Tower')
  await expect(page).toHaveURL(/\/structures\/watch_tower$/)

  await page.getByLabel('Map letter').fill('W')
  await page.getByLabel('Watch Tower colour (hex code)').fill('#445566')
  await page.getByLabel('Width (squares)').fill('2')
  await page.getByLabel('Height (squares)').fill('3')
  await expect(page.getByTestId('footprint')).toHaveAccessibleName('Footprint: 2 squares wide and 3 squares high')
  await expect(page.getByTestId('footprint').locator('span')).toHaveCount(6)

  await page.getByLabel('Foot: rule').selectOption('cost')
  await page.getByLabel('Foot cost').fill('3')
  await page.getByLabel('Tread: rule').selectOption('blocked')
  await page.getByLabel('Add another number').fill('defense')
  await page.getByLabel('Add another number').press('Enter')
  await page.getByLabel('Defense Value').fill('5')

  await page.getByRole('checkbox', { name: /Can be captured/ }).check()
  await page.getByLabel('Capture points').fill('30')
  await page.getByRole('checkbox', { name: /Counts as headquarters/ }).check()
  await page.getByLabel('Funds Per turn').fill('200')
  await page.getByRole('checkbox', { name: 'Infantry' }).check()
  await saved(page)

  const tower = disk(id).find((s) => s.id === 'watch_tower')!
  expect(tower).toEqual({
    id: 'watch_tower', name: 'Watch Tower', color: '#445566', glyph: 'W', width: 2, height: 3,
    moveCost: { foot: 3, tread: 0 }, props: { defense: 5 }, capturePoints: 30, income: { funds: 200 }, produces: ['infantry'], tags: ['hq'],
  })
  expect(await game.problems(id)).toEqual([])

  // Back to "same as the ground" removes the key; the footprint goes back to 1 x 1 without keys.
  await page.getByLabel('Tread: rule').selectOption('ground')
  await page.getByLabel('Width (squares)').fill('1')
  await page.getByLabel('Height (squares)').fill('1')
  await saved(page)
  const again = disk(id).find((s) => s.id === 'watch_tower')!
  expect(again.moveCost).toEqual({ foot: 3 })
  expect(again).not.toHaveProperty('width')
  expect(again).not.toHaveProperty('height')
})

test('every size from 1 to 8 is saved and drawn as a footprint; the boxes refuse 0 and 9', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Sizes')
  await open(page, id)
  await createKind(page, 'Big One')
  for (const [w, h] of [[2, 2], [8, 8], [3, 1], [1, 5], [8, 1], [1, 8]]) {
    await page.getByLabel('Width (squares)').fill(String(w))
    await page.getByLabel('Height (squares)').fill(String(h))
    await expect(page.getByTestId('footprint').locator('span')).toHaveCount(w * h)
    await expect(page.getByText(`Covers ${w} × ${h} squares`)).toBeVisible()
    await saved(page)
    const k = disk(id).find((s) => s.id === 'big_one')!
    expect([k.width ?? 1, k.height ?? 1]).toEqual([w, h])
  }
  await page.getByLabel('Width (squares)').fill('9')
  await expect(page.getByText('At most 8.')).toBeVisible()
  await page.getByLabel('Width (squares)').fill('0')
  await expect(page.getByText('At least 1.')).toBeVisible()
  await page.getByLabel('Width (squares)').blur()
  expect(await game.problems(id)).toEqual([])
})

test('the first structure of a game without any creates the file; settings of rules that are off are hidden with a pointer to Rules', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'First')
  await open(page, id)
  await expect(page.getByText('No structures yet.').first()).toBeVisible()
  await expect(page.getByText(/Structures stand on top of the ground/)).toBeVisible()
  await createKind(page, 'Fort')
  await expect(page.getByRole('checkbox', { name: /Can be captured/ })).toHaveCount(0)
  await expect(page.getByTestId('hidden-settings')).toContainText('capturing, income, building units, headquarters')
  await expect(page.getByTestId('hidden-settings').getByRole('link', { name: 'Open Rules' })).toBeVisible()
  await page.getByLabel('Add another number').fill('defense')
  await page.getByLabel('Add another number').press('Enter')
  await page.getByLabel('Defense Value').fill('4')
  await saved(page)
  expect(disk(id)).toEqual([expect.objectContaining({ id: 'fort', name: 'Fort', props: { defense: 4 } })])
  expect(await game.problems(id)).toEqual([])
  // Undo past the first structure: the list goes back to nothing and the file follows (an empty list), never the old state.
  for (let i = 0; i < 4; i++) await page.keyboard.press('Control+z')
  await saved(page)
  expect(disk(id)).toEqual([])
  expect(await game.problems(id)).toEqual([])
})

test('deleting a kind lists the maps and counts, removes the placed ones, and one Undo brings everything back', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Delete')
  const before = JSON.stringify(readDisk(id, 'maps/crossroads.json'))
  const kindsBefore = disk(id).map((s) => s.id)
  await open(page, id, 'city')
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  const dlg = page.getByRole('dialog', { name: /Delete the structure/ })
  await expect(dlg).toContainText('Map “Crossroads” has 6 of them placed')
  await dlg.getByRole('button', { name: 'Cancel' }).click()
  expect(disk(id).map((s) => s.id)).toEqual(kindsBefore)
  await page.getByRole('button', { name: 'Delete', exact: true }).click()
  await page.getByRole('dialog', { name: /Delete the structure/ }).getByRole('button', { name: 'Delete', exact: true }).click()
  await saved(page)
  expect(disk(id).map((s) => s.id)).toEqual(['factory', 'hq'])
  const map = readDisk(id, 'maps/crossroads.json')
  expect(map.structures.some((s: any) => s.type === 'city')).toBe(false)
  expect(map.structures).toHaveLength(4)
  expect(await game.problems(id)).toEqual([])
  await page.keyboard.press('Control+z')
  await saved(page)
  expect(disk(id).map((s) => s.id)).toEqual(kindsBefore)
  expect(JSON.stringify(readDisk(id, 'maps/crossroads.json'))).toBe(before)
})

test('duplicate makes a copy next to it; the new kind can be renamed and ids never change', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Dup')
  await open(page, id, 'factory')
  await page.getByRole('button', { name: 'Duplicate' }).click()
  await expect(page).toHaveURL(/\/structures\/factory_copy$/)
  await page.getByLabel('Name', { exact: true }).fill('Big Factory')
  await saved(page)
  expect(disk(id).map((s) => s.id)).toEqual(['city', 'factory', 'factory_copy', 'hq'])
  expect(disk(id)[2]).toMatchObject({ id: 'factory_copy', name: 'Big Factory', produces: ['infantry', 'tank', 'artillery'] })
})

test('the Terrain form no longer has capture, income, build lists or headquarters, and points to Structures', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'TerrainForm')
  await page.goto(editorUrl(id, 'terrain', 'city'))
  await expect(page.getByLabel('Name', { exact: true })).toBeVisible()
  for (const word of [/Can be captured/, /Counts as headquarters/, /Units that can be built here/, /^Income/]) await expect(page.getByText(word)).toHaveCount(0)
  await page.getByRole('link', { name: 'Open Structures' }).click()
  await expect(page).toHaveURL(/\/structures$/)
})

test('a style can recolour a structure; the map shows it in that style, and Reset goes back', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'StyleColours')
  await page.goto(editorUrl(id, 'game'))
  await page.getByRole('button', { name: 'Add a style' }).click()
  await page.getByRole('dialog', { name: 'New visual style' }).getByLabel('Name of the style').fill('Dusk')
  await page.getByRole('dialog', { name: 'New visual style' }).getByRole('button', { name: 'Create' }).click()
  await expect(page.getByText('Structure colours in this style')).toBeVisible()
  await page.getByLabel('Dusk: City colour (hex code)').fill('#123456')
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'dusk', name: 'Dusk', structureColors: { city: '#123456' } }])
  expect(await game.problems(id)).toEqual([])
  // The map starts with the first style: the city at (5, 0) is a block in the style's colour, the factory keeps its own.
  await openMap(page, id, 'crossroads')
  await expectPixel(page, 5, 0, 0.2, 0.3, '#123456', 'the city in the Dusk style')
  await expectPixel(page, 2, 3, 0.2, 0.3, '#9a9a9a', 'the factory is not recoloured')
  await page.goto(editorUrl(id, 'game'))
  await page.getByRole('button', { name: 'Reset' }).first().click()
  await saved(page)
  expect(readDisk(id, 'game.json').styles).toEqual([{ id: 'dusk', name: 'Dusk' }])
})
