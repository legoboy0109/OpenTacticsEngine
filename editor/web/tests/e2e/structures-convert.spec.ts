// Old projects (made before structures existed): the offer, the explanation, the one-step conversion against the real helper,
// Undo, and the engine accepting the result. The old files are written into a game folder in the old format.
import { editorUrl, expect, readDisk, requirePlayer, saved, test, writeDisk } from './fixtures'
import { OLD_MAP } from '../unit/oldProject'
import { seedOldGame } from './oldkit'

const banner = (page: import('@playwright/test').Page) => page.getByText('This game was made with an older version of the editor.')

test('@smoke an old project offers Convert with a plain explanation; Not now changes nothing; Convert is one step, the game loads afterwards', async ({ page, request, game }) => {
  const id = await game.create('economy-tactics', 'Old Project')
  await seedOldGame(request, id)
  const problems = await game.problems(id)
  expect(problems.map((p) => p.message).join('\n')).toMatch(/now belongs to structures/)
  const filesBefore = ['data/terrain.json', 'maps/crossroads.json', 'game.json', 'data/graphics.json'].map((f) => JSON.stringify(readDisk(id, f)))

  await page.goto(editorUrl(id, 'maps', 'crossroads'))
  await expect(banner(page)).toBeVisible()
  await expect(page.getByText(/Until the game is updated it will not load/)).toBeVisible()
  await page.getByRole('button', { name: 'See what will change…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Update this game for structures' })
  const text = dlg.getByTestId('convert-explanation')
  await expect(text).toContainText('Buildings used to be kinds of terrain')
  await expect(text).toContainText('“City” (can be captured and earns income) becomes a structure with the same name, colour and letter and those same settings. 6 squares on your maps get one.')
  await expect(text).toContainText('“Factory” (can be captured, earns income and builds units)')
  await expect(text).toContainText('“Headquarters” (can be captured, earns income and headquarters)')
  await expect(text).toContainText('The terrain itself stays')
  await expect(text).toContainText('6 owned squares move to the structures')
  await expect(text).toContainText('1 owned square sits on ground that is not one of those buildings')
  await expect(text).toContainText('You can undo all of it in one step')

  // Not now: nothing was written anywhere.
  await dlg.getByRole('button', { name: 'Not now' }).click()
  await expect(dlg).toBeHidden()
  await page.waitForTimeout(1200)
  expect(['data/terrain.json', 'maps/crossroads.json', 'game.json', 'data/graphics.json'].map((f) => JSON.stringify(readDisk(id, f)))).toEqual(filesBefore)

  // Convert.
  await page.getByRole('button', { name: 'See what will change…' }).click()
  await dlg.getByRole('button', { name: 'Convert this game' }).click()
  await expect(banner(page)).toHaveCount(0)
  await expect(page.getByText('The game now uses structures. You can undo this with the Undo button.')).toBeVisible()
  await saved(page)

  const structures = readDisk(id, 'data/structures.json').structures
  expect(structures).toEqual([
    { id: 'city', name: 'City', color: '#b7b7b7', glyph: 'c', capturePoints: 20, income: { funds: 1000 } },
    { id: 'factory', name: 'Factory', color: '#9a9a9a', glyph: 'F', capturePoints: 20, income: { funds: 1000 }, produces: ['infantry', 'tank', 'artillery'] },
    { id: 'hq', name: 'Headquarters', color: '#d4af37', glyph: 'H', capturePoints: 20, income: { funds: 1000 }, tags: ['hq'] },
  ])
  const terrain = readDisk(id, 'data/terrain.json').terrain
  expect(terrain.find((t: any) => t.id === 'city')).toEqual({ id: 'city', name: 'City', color: '#b7b7b7', glyph: 'c', props: { defense: 3 } })
  expect(terrain.find((t: any) => t.id === 'hq')).not.toHaveProperty('tags')
  const map = readDisk(id, 'maps/crossroads.json')
  expect(map).not.toHaveProperty('owners')
  expect(map.tiles).toEqual(OLD_MAP.tiles)
  expect(map.structures).toHaveLength(10)
  expect(map.structures).toContainEqual({ type: 'hq', x: 1, y: 2, side: 'blue' })
  expect(map.structures).toContainEqual({ type: 'city', x: 5, y: 0 })
  expect(map.units).toEqual(OLD_MAP.units)
  const night = readDisk(id, 'game.json').styles.find((s: any) => s.id === 'night')
  expect(night.structureColors).toEqual({ city: '#6a6a78', hq: '#8a7a30' })
  expect(night.terrainColors.city).toBe('#6a6a78')
  const graphics = readDisk(id, 'data/graphics.json')
  expect(graphics.structures.city).toEqual({ sheet: 'tiles', cell: [3, 1] })
  expect(graphics.structures.hq).toEqual({ sheet: 'tiles', frames: [[0, 2], [1, 2]], frameMs: 300 })
  expect(graphics.styles.night.structures.city).toEqual({ sheet: 'tiles', cell: [4, 1] })
  expect(await game.problems(id)).toEqual([])

  // The editor shows the structures, and a reload does not offer anything any more.
  await page.goto(editorUrl(id, 'structures'))
  await expect(page.getByRole('list', { name: 'Structures' }).getByRole('button')).toHaveCount(3)
  await page.reload()
  await expect(page.getByRole('heading', { name: 'Structures', level: 1 })).toBeVisible()
  await expect(banner(page)).toHaveCount(0)
})

test('Undo takes the conversion back in one step and saves the old project exactly; Redo converts again', async ({ page, request, game }) => {
  const id = await game.create('economy-tactics', 'Old Undo')
  await seedOldGame(request, id)
  const before = {
    terrain: readDisk(id, 'data/terrain.json'), map: readDisk(id, 'maps/crossroads.json'), game: readDisk(id, 'game.json'), graphics: readDisk(id, 'data/graphics.json'),
  }
  await page.goto(editorUrl(id, 'terrain'))
  await page.getByRole('button', { name: 'See what will change…' }).click()
  await page.getByRole('dialog', { name: 'Update this game for structures' }).getByRole('button', { name: 'Convert this game' }).click()
  await saved(page)
  expect(await game.problems(id)).toEqual([])

  await page.getByRole('button', { name: 'Undo' }).click() // ONE press
  await expect(banner(page)).toBeVisible()
  await saved(page)
  expect(readDisk(id, 'data/terrain.json')).toEqual(before.terrain)
  expect(readDisk(id, 'maps/crossroads.json')).toEqual(before.map)
  expect(readDisk(id, 'game.json')).toEqual(before.game)
  expect(readDisk(id, 'data/graphics.json')).toEqual(before.graphics)
  expect(readDisk(id, 'data/structures.json').structures).toEqual([]) // nothing of the conversion is left
  expect((await game.problems(id)).map((p) => p.message).join('\n')).toMatch(/now belongs to structures/)

  await page.getByRole('button', { name: 'Redo' }).click()
  await expect(banner(page)).toHaveCount(0)
  await saved(page)
  expect(readDisk(id, 'data/structures.json').structures).toHaveLength(3)
  expect(await game.problems(id)).toEqual([])
})

test('an empty owners list (older editors wrote one into every map) loads without any offer and is left out when the map is saved', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Empty Owners')
  const map = readDisk(id, 'maps/crossroads.json')
  writeDisk(id, 'maps/crossroads.json', JSON.stringify({ ...map, owners: [] }, null, 2))
  expect(await game.problems(id)).toEqual([]) // the engine accepts an empty list
  await page.goto(editorUrl(id, 'maps', 'crossroads'))
  await expect(page.getByRole('toolbar', { name: 'Map tools' })).toBeVisible()
  await expect(banner(page)).toHaveCount(0)
  // Any edit of that map writes it back without the empty list.
  await page.getByRole('tab', { name: 'Map', exact: true }).click()
  await page.getByLabel('Name', { exact: true }).fill('Crossroads II')
  await saved(page)
  const after = readDisk(id, 'maps/crossroads.json')
  expect(after.name).toBe('Crossroads II')
  expect(after).not.toHaveProperty('owners')
  expect(after.structures).toEqual(map.structures)
})

test('a non-empty owners list on its own is offered for conversion too, and is never dropped without it', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Owners Only')
  const map = readDisk(id, 'maps/crossroads.json')
  const withOwners = { ...map, structures: [], owners: [{ x: 5, y: 0, side: 'red' }] }
  writeDisk(id, 'maps/crossroads.json', JSON.stringify(withOwners, null, 2))
  await page.goto(editorUrl(id, 'maps', 'crossroads'))
  await expect(banner(page)).toBeVisible()
  // Editing the map keeps the list on disk: the user has not agreed to convert yet.
  await page.getByRole('tab', { name: 'Map', exact: true }).click()
  await page.getByLabel('Name', { exact: true }).fill('Renamed')
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').owners).toEqual([{ x: 5, y: 0, side: 'red' }])
  await page.getByRole('button', { name: 'See what will change…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Update this game for structures' })
  await expect(dlg.getByTestId('convert-explanation')).toContainText('1 owned square sits on ground that is not one of those buildings')
  await expect(dlg.getByTestId('convert-explanation')).not.toContainText('Terrain that becomes structures')
  await dlg.getByRole('button', { name: 'Convert this game' }).click()
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json')).not.toHaveProperty('owners')
  expect(await game.problems(id)).toEqual([])
})

test('the converted game plays: structures are in the real player, and the game starts', async ({ page, request, game }) => {
  requirePlayer()
  const id = await game.create('economy-tactics', 'Old Play')
  await seedOldGame(request, id)
  await page.goto(editorUrl(id, 'maps', 'crossroads'))
  await page.getByRole('button', { name: 'See what will change…' }).click()
  await page.getByRole('dialog', { name: 'Update this game for structures' }).getByRole('button', { name: 'Convert this game' }).click()
  await saved(page)
  await page.getByRole('button', { name: 'Play', exact: true }).click()
  await expect(page.getByRole('dialog', { name: 'Play', exact: true })).toBeVisible()
  await expect(page.getByTestId('play-loading')).toBeHidden({ timeout: 30_000 })
  await expect(page.getByTestId('play-error')).toHaveCount(0)
})
