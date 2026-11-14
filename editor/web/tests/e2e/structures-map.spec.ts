// The map editor's Structures tool against the real helper: place (mouse and keyboard), the ghost, refusals, select / own /
// move / remove, the Select tool's effective numbers, resize, and the canvas matching the drawing rules (pixel checks).
import type { Page } from '@playwright/test'
import { bareGame, canvasPixel, expectPixel, hex, makePng, mapPixel, pieceColor, seedArt, sheetPng } from './artkit'
import { expect, readDisk, saved, test } from './fixtures'
import { canvas, cellPoint, clickCell, diskStructures, ghost, hoverCell, openMap, pickSide, pickStructure, pickTerrain, pickUnitType, status, tab, tool } from './mapkit'

const notice = (page: Page) => page.getByTestId('map-notice')
const props = (page: Page) => page.getByTestId('structure-props')

/** Adds structure kinds to a game through the API (like another program would). */
async function addKinds(request: any, id: string, kinds: object[]): Promise<void> {
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, structures: [...doc.structures, ...kinds] } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

test('@smoke place a structure on a map: the canvas shows it with its owner, the file has it, the game loads', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'Place')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickSide(page, 'Red Army').click()
  await pickStructure(page, 'City').click()
  await expect(page.getByRole('radiogroup', { name: 'Structure' }).getByRole('radio')).toHaveCount(3)
  await clickCell(page, 2, 6)
  await saved(page)

  expect(diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'city', x: 2, y: 6, side: 'red' })
  expect(readDisk(id, 'maps/crossroads.json')).not.toHaveProperty('owners')
  expect(await game.problems(id)).toEqual([])
  // It is selected right away and the status line names it.
  await expect(props(page)).toContainText('Selected structure')
  await expect(props(page)).toContainText('top-left at 3, 7')
  await hoverCell(page, 2, 6)
  await expect(status(page)).toContainText('City · owned by Red Army')
  // The block is the city's colour and has the red owner frame around it (look at it without the selection ring and the hover tint).
  await page.keyboard.press('Escape')
  await page.mouse.move(2, 2)
  await expectPixel(page, 2, 6, 0.2, 0.3, '#b7b7b7', 'the city block')
  await expectPixel(page, 2, 6, 0.06, 0.5, '#cc0000', 'the owner frame')
})

test('a 2 x 2 ghost follows the pointer: valid squares and ones where it sticks out or overlaps; a click that does not fit changes nothing', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'Ghost')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Headquarters').click()
  const before = diskStructures(id, 'crossroads')

  await hoverCell(page, 4, 4)
  expect(await ghost(page)).toBe('hq,4,4,ok')
  await hoverCell(page, 11, 7) // would stick out of the map to the right and below
  expect(await ghost(page)).toBe('hq,11,7,bad')
  await hoverCell(page, 4, 5) // covers (5,6): a city is there
  expect(await ghost(page)).toBe('hq,4,5,bad')
  await hoverCell(page, 1, 4) // a city stands here: clicking selects it, so no ghost
  expect(await ghost(page)).toBe('')

  await clickCell(page, 11, 7)
  await expect(notice(page)).toContainText('needs 2 × 2 squares and would stick out of the map')
  await clickCell(page, 4, 5)
  await expect(notice(page)).toContainText('would overlap the City')
  await page.waitForTimeout(900)
  expect(diskStructures(id, 'crossroads')).toEqual(before) // refused: nothing written

  await clickCell(page, 3, 5) // covers (3..4, 5..6): free
  await saved(page)
  expect(diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'hq', x: 3, y: 5 })
  await expectPixel(page, 4, 6, 0.8, 0.8, '#d4af37', 'the far corner of the footprint is part of the block')
  await expect(props(page)).toContainText('2 × 2 squares')
  expect(await game.problems(id)).toEqual([])
})

test('select, change owner and kind, drag to move (a refused move changes nothing), Delete removes, Undo brings it back', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'Edit')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  const hqAt = () => diskStructures(id, 'crossroads').find((s) => s.type === 'hq' && s.side === 'red')!

  // Grab the red headquarters (10,2) by its lower-right square (11,3) and drop it two rows up.
  const from = await cellPoint(page, 11, 3)
  const to = await cellPoint(page, 11, 1)
  await page.mouse.move(from.x, from.y)
  await page.mouse.down()
  await page.mouse.move(to.x, to.y, { steps: 4 })
  expect(await ghost(page)).toBe('hq,10,0,ok')
  await page.mouse.up()
  await saved(page)
  expect(hqAt()).toMatchObject({ x: 10, y: 0 })
  await expect(props(page)).toContainText('top-left at 11, 1')

  // A drag onto the city at (10,4) is refused: the ghost is red, a notice says why, nothing is written.
  const a = await cellPoint(page, 10, 0)
  const b = await cellPoint(page, 10, 3)
  await page.mouse.move(a.x, a.y)
  await page.mouse.down()
  await page.mouse.move(b.x, b.y, { steps: 4 })
  expect(await ghost(page)).toBe('hq,10,3,bad')
  await page.mouse.up()
  await expect(notice(page)).toContainText('would overlap')
  await page.waitForTimeout(900)
  expect(hqAt()).toMatchObject({ x: 10, y: 0 })
  expect(await ghost(page)).not.toMatch(/^hq/) // the pointer is over an empty square again: the chosen kind's ghost, not the drag's

  // Owner and kind of the selected structure.
  await props(page).getByLabel('Owner').selectOption({ label: 'Nobody' })
  await saved(page)
  expect(diskStructures(id, 'crossroads').find((s) => s.x === 10 && s.y === 0)).toEqual({ type: 'hq', x: 10, y: 0 })
  await props(page).getByLabel('Kind').selectOption({ label: 'City' })
  await saved(page)
  expect(diskStructures(id, 'crossroads').find((s) => s.x === 10 && s.y === 0)).toEqual({ type: 'city', x: 10, y: 0 })
  await props(page).getByLabel('Kind').selectOption({ label: 'Headquarters' })
  await saved(page)

  // Delete removes it; Undo brings it back; the Remove button does the same.
  const count = diskStructures(id, 'crossroads').length
  await canvas(page).focus()
  await page.keyboard.press('Delete')
  await saved(page)
  expect(diskStructures(id, 'crossroads')).toHaveLength(count - 1)
  await page.keyboard.press('Control+z')
  await saved(page)
  expect(diskStructures(id, 'crossroads')).toHaveLength(count)
  await clickCell(page, 11, 1) // select it again (Delete had deselected)
  await page.getByRole('button', { name: 'Remove structure' }).click()
  await saved(page)
  expect(diskStructures(id, 'crossroads')).toHaveLength(count - 1)
  expect(await game.problems(id)).toEqual([])
})

test('keyboard: arrow keys move the cursor (the ghost follows it), Enter places', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'Keys')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'City').click()
  await canvas(page).focus()
  for (const k of ['ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowRight', 'ArrowDown']) await page.keyboard.press(k)
  await expect.poll(() => ghost(page)).toBe('city,4,1,ok')
  await page.keyboard.press('Enter')
  await saved(page)
  expect(diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'city', x: 4, y: 1 })
})

test('painting terrain under a structure is allowed and leaves the structure', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'PaintUnder')
  await openMap(page, id, 'crossroads')
  const before = diskStructures(id, 'crossroads')
  await pickTerrain(page, 'Forest').click()
  await clickCell(page, 5, 0) // a city stands here
  await saved(page)
  expect(diskStructures(id, 'crossroads')).toEqual(before)
  expect(readDisk(id, 'maps/crossroads.json').legend[readDisk(id, 'maps/crossroads.json').tiles[0][5]]).toBe('forest')
  expect(await game.problems(id)).toEqual([])
})

test('the Select tool shows the ground and the structure of a square with the effective movement and numbers', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Select')
  await addKinds(request, id, [{ id: 'gate', name: 'Gate', color: '#775533', glyph: 'G', width: 2, height: 1, moveCost: { foot: 1 }, props: { defense: 5, cover: 7 } }])
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Gate').click()
  await clickCell(page, 2, 0) // forest at (2,0): the foot cost 1 replaces the forest's... forest costs tread 2
  await saved(page)
  await tool(page, 'Select').click()
  await clickCell(page, 3, 0) // the second square of the gate
  const costs = page.getByTestId('effective-costs')
  await expect(page.getByText('Structure', { exact: true })).toBeVisible()
  await expect(page.getByText('Gate (2 × 1)')).toBeVisible()
  await expect(costs).toContainText('Foot 1 (the structure)')
  await expect(page.getByTestId('effective-numbers')).toContainText('defense 5 (the structure), cover 7 (the structure)')
  // Plain ground without a structure: only the ground's own numbers, no "(the structure)".
  await clickCell(page, 4, 0)
  await expect(page.getByTestId('effective-numbers')).not.toContainText('the structure')
  await expect(page.getByText('none', { exact: true }).first()).toBeVisible()
  // A city square: the structure's defense 3 replaces the ground's.
  await clickCell(page, 5, 0)
  await expect(page.getByTestId('effective-numbers')).toContainText('defense 3 (the structure)')
})

test('a structure that would strand a unit is refused, and a unit cannot be placed on a square the structure blocks', async ({ page, game, request }) => {
  const id = await game.create('economy-tactics', 'Walls')
  await addKinds(request, id, [{ id: 'wall', name: 'Wall', color: '#554433', glyph: 'W', moveCost: { foot: 0, tread: 0 } }])
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Wall').click()
  const before = diskStructures(id, 'crossroads')
  await clickCell(page, 2, 2) // an infantry unit stands here
  await expect(notice(page)).toContainText('Infantry stands on a square of it and could not enter a Wall')
  await page.waitForTimeout(900)
  expect(diskStructures(id, 'crossroads')).toEqual(before)
  await clickCell(page, 6, 3)
  await saved(page)
  expect(diskStructures(id, 'crossroads').at(-1)).toEqual({ type: 'wall', x: 6, y: 3 })
  await tool(page, 'Units').click()
  await pickSide(page, 'Blue').click()
  await pickUnitType(page, 'Infantry').click()
  await clickCell(page, 6, 3)
  await expect(notice(page)).toContainText('cannot stand on a Wall')
  expect(await game.problems(id)).toEqual([])
})

test('resize lists the structures that would no longer fit completely; confirming removes exactly those; Undo restores', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Crop')
  await openMap(page, id, 'crossroads')
  const before = readDisk(id, 'maps/crossroads.json')
  await tab(page, 'Map').click()
  await page.getByRole('button', { name: 'Resize…' }).click()
  const dlg = page.getByRole('dialog', { name: 'Resize the map' })
  await dlg.getByLabel('Width').fill('11') // the last column goes: the red headquarters reaches it
  await dlg.getByRole('radio', { name: 'top left' }).click()
  await expect(dlg).toContainText('This will remove:')
  await expect(dlg).toContainText('Headquarters at 11, 3')
  await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
  await saved(page)
  const after = readDisk(id, 'maps/crossroads.json')
  expect(after.grid.width).toBe(11)
  expect(after.structures).toHaveLength(before.structures.length - 1)
  expect(after.structures.some((s: any) => s.type === 'hq' && s.side === 'red')).toBe(false)
  expect(await game.problems(id)).toEqual([])
  await page.keyboard.press('Control+z')
  await saved(page)
  expect(readDisk(id, 'maps/crossroads.json').structures).toEqual(before.structures)
})

test('the canvas follows the drawing rules: pictures across the footprint, per-owner pictures, tall pictures stick out above, owner frame, a unit over the structure it stands on', async ({ page, request, game }) => {
  const id = await game.create('economy-tactics', 'Pixels')
  const tall = makePng(48, 48, (x) => pieceColor(Math.floor(x / 24), 0)) // two pieces, each 24 wide and 48 tall
  await seedArt(request, id, { structs: sheetPng(4, 2), tall }, {
    sheets: [
      { id: 'structs', file: 'art/structs.png', cellWidth: 24, cellHeight: 24 },
      { id: 'tall', file: 'art/tall.png', cellWidth: 24, cellHeight: 48 },
    ],
    structures: {
      city: { sheet: 'structs', cell: [0, 0], sides: { red: { sheet: 'structs', cell: [1, 0] } } },
      hq: { sheet: 'structs', cell: [0, 1] },
      tower: { sheet: 'tall', cell: [1, 0] },
    },
  })
  await addKinds(request, id, [{ id: 'tower', name: 'Tower', color: '#aaaaaa', glyph: 'T' }])
  const doc = await (await request.get(`/api/projects/${id}`)).json()
  const m = doc.maps.crossroads
  m.structures.push({ type: 'tower', x: 7, y: 5 })
  m.units.push({ type: 'infantry', side: 'blue', x: 0, y: 2 }) // on the upper-left square of blue's 2 x 2 headquarters
  expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, maps: doc.maps } })).ok()).toBeTruthy()
  await openMap(page, id, 'crossroads')

  const col = (c: number, r: number) => hex(pieceColor(c, r))
  await expectPixel(page, 5, 0, 0.5, 0.5, col(0, 0), 'a neutral city shows the general picture')
  await expectPixel(page, 10, 4, 0.5, 0.5, col(1, 0), 'a red city shows the red picture')
  await expectPixel(page, 1, 4, 0.5, 0.5, col(0, 0), 'a blue city has no picture of its own for blue: the general one')
  await expectPixel(page, 1, 3, 0.5, 0.5, col(0, 1), 'the 2 x 2 headquarters picture covers the whole footprint (lower right square)')
  await expectPixel(page, 1, 2, 0.5, 0.5, col(0, 1), 'and the upper right one')
  await expectPixel(page, 0, 3, 0.5, 0.9, col(0, 1), 'and the lower left one')
  await expectPixel(page, 0, 3, 0.06, 0.5, '#3d85c6', 'the owner frame is drawn over the picture')
  // The tall picture (24 x 48 piece on a 1 x 1 footprint) is 2 squares tall: it sticks out above, not below.
  await expectPixel(page, 7, 5, 0.5, 0.5, col(1, 0), 'tall picture, footprint square')
  await expectPixel(page, 7, 4, 0.5, 0.5, col(1, 0), 'tall picture, the square above')
  expect(hex(await mapPixel(page, 7, 3, 0.5, 0.5)), 'nothing two squares above').not.toBe(col(1, 0))
  expect(hex(await mapPixel(page, 7, 6, 0.5, 0.5)), 'nothing below').not.toBe(col(1, 0))
  // A unit on the upper-left square of the headquarters is drawn over it (its block is blue, the picture is green).
  await expectPixel(page, 0, 2, 0.5, 0.2, '#3d85c6', 'the unit is in front of the structure it stands on')
  expect(await canvas(page).getAttribute('data-ghost')).toBe('')
})

test('without pictures: a block in the structure colour over the whole footprint, inset, with its letter', async ({ page, request, game }) => {
  const id = await bareGame(request, game, 'economy-tactics', 'Blocks')
  await openMap(page, id, 'crossroads')
  // hq (0,2) covers (0..1, 2..3): every square of the footprint is block colour away from the letter, and the 1-pixel outline is dark.
  for (const [x, y] of [[0, 2], [1, 2], [0, 3], [1, 3]]) await expectPixel(page, x, y, x === 0 ? 0.3 : 0.7, y === 2 ? 0.3 : 0.7, '#d4af37', `hq square ${x},${y}`)
  // The one-pixel dark outline sits just inside the footprint's top edge (an owned structure has its frame there; the neutral city at (5, 0) does not).
  const [scale, vx, vy] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  const k = Math.max(1, Math.round(scale / 24))
  const outline = await canvasPixel(canvas(page), Math.round(vx + 5.5 * scale), Math.round(vy) + k)
  expect(Math.max(outline[0], outline[1], outline[2]), 'the outline inside the footprint edge is dark').toBeLessThan(140)
})

test('the Structures tool is remembered across a reload, and a tool remembered under its old name does not break the editor', async ({ page, game }) => {
  const id = await game.create('economy-tactics', 'Remember')
  await openMap(page, id, 'crossroads')
  await tool(page, 'Structures').click()
  await pickStructure(page, 'Factory').click()
  await page.reload()
  await expect(tool(page, 'Structures')).toHaveAttribute('aria-pressed', 'true')
  await expect(pickStructure(page, 'Factory')).toHaveAttribute('aria-checked', 'true')
  await page.evaluate((key) => localStorage.setItem(key, JSON.stringify({ tool: 'owners', ownerSide: 'red' })), `ote.mapTools.${id}`)
  await page.reload()
  await expect(tool(page, 'Terrain')).toHaveAttribute('aria-pressed', 'true')
})
