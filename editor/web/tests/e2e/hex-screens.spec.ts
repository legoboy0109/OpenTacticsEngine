// The screens of the editor on hexagon games: the grid shown on the Game screen, styles limited to top-down, the Structures
// screen's "Covers" control (writes `rings`), join blocks of 8 x 8 on the Art/Terrain screens (checked on disk, in the picker
// and on the map canvas), and the new-game cards that show each template's grid.
import { cpSync, readFileSync } from 'node:fs'
import path from 'node:path'
import { addImage, clickPiece, makePng, near, openArt, pieceColor, picker, pickerGrid, seedArt } from './artkit'
import { editorUrl, expect, gamesDir, readDisk, saved, test } from './fixtures'
import { canvas, openMap } from './mapkit'
import { hexGame, hexPoint, neighboursOf, type Layout } from './hexkit'
import { decodePng } from './png'

const LAYOUTS: Layout[] = ['pointy', 'flat']
const GRID_TEXT: Record<Layout, string> = { pointy: 'Hexagons, pointy-top', flat: 'Hexagons, flat-top' }
// A hexagon's box in pixels at the fixtures' tile size 24: pointy 24 x 28, flat 28 x 24.
const BOX: Record<Layout, [number, number]> = { pointy: [24, 28], flat: [28, 24] }

for (const layout of LAYOUTS) {
  test(`@smoke ${layout}: the Game screen shows the grid (read only) and a style can be top-down or isometric`, async ({ page }) => {
    const id = hexGame(layout)
    await page.goto(editorUrl(id, 'game'))
    await expect(page.getByTestId('game-grid')).toHaveText(GRID_TEXT[layout])
    await expect(page.getByText('The grid is chosen when the game is made and cannot be changed later.')).toBeVisible()
    // nothing on the screen can change it
    await expect(page.getByTestId('game-grid').locator('input, select, button')).toHaveCount(0)

    await page.getByRole('button', { name: 'Add a style' }).click()
    await page.getByLabel('Name of the style').fill('Night')
    await page.getByRole('button', { name: 'Create' }).click()
    await saved(page)
    const kind = page.getByLabel('Night: Kind of view')
    await expect(kind).toHaveValue('top-down')
    // every kind is offered for hexagons now (the helper says which): no note about missing kinds
    await expect(page.getByTestId('view-hex-help')).toHaveCount(0)
    await expect(kind.locator('option')).toHaveText(['Top-down', 'Isometric', '3D isometric', 'Full 3D'])
    await expect(kind.locator('option', { hasText: /^Isometric$/ })).toBeEnabled()

    // Isometric: its settings have no square height (the angle of a hexagon is fixed)
    await kind.selectOption({ label: 'Isometric' })
    await saved(page)
    expect(readDisk(id, 'game.json').styles[0].view).toBe('isometric')
    await page.locator('.subcard').getByText('Advanced').click()
    await expect(page.getByLabel('Night: Tile width')).toBeVisible()
    await expect(page.getByLabel('Night: Height step')).toHaveAttribute('placeholder', '12')
    await expect(page.getByLabel('Night: Unit width')).toBeVisible()
    await expect(page.getByLabel('Night: Square height')).toHaveCount(0)
    await expect(page.getByLabel('Night: Square width')).toHaveCount(0)
    await page.getByLabel('Night: Tile width').fill('64')
    await saved(page)
    expect(readDisk(id, 'game.json').styles[0].isometric).toEqual({ tileWidth: 64 })

    // and back: the kind and its sizes leave the file
    await kind.selectOption({ label: 'Top-down' })
    await saved(page)
    expect(readDisk(id, 'game.json').styles[0].view).toBeUndefined()
    expect(readDisk(id, 'game.json').styles[0].isometric).toBeUndefined()
    expect(readDisk(id, 'game.json').grid).toEqual({ type: 'hex', layout })
  })

  test(`@smoke ${layout}: the Structures screen offers 1 or 7 hexagons (no width and height) and writes rings`, async ({ page, game }) => {
    const id = hexGame(layout)
    await page.goto(editorUrl(id, 'structures', 'city'))
    const size = page.getByRole('radiogroup', { name: 'Covers' })
    await expect(size).toBeVisible()
    await expect(page.getByLabel('Width (squares)')).toHaveCount(0)
    await expect(page.getByLabel('Height (squares)')).toHaveCount(0)
    await expect(size.getByRole('radio', { name: '1 hexagon' })).toHaveAttribute('aria-checked', 'true')
    await expect(page.getByTestId('footprint')).toHaveAttribute('aria-label', 'Footprint: one hexagon')
    // the Headquarters already covers seven hexagons
    await page.goto(editorUrl(id, 'structures', 'hq'))
    await expect(size.getByRole('radio', { name: /^7 hexagons/ })).toHaveAttribute('aria-checked', 'true')
    await expect(page.getByTestId('footprint')).toHaveAttribute('aria-label', 'Footprint: a hexagon and the six around it')
    // a new kind: 1 hexagon writes nothing, 7 hexagons writes "rings": 1, back to 1 removes it again
    await page.goto(editorUrl(id, 'structures'))
    await page.getByRole('button', { name: 'New' }).click()
    await page.getByLabel('Name of the structure').fill('Citadel')
    await page.getByRole('button', { name: 'Create' }).click()
    await saved(page)
    const citadel = () => readDisk(id, 'data/structures.json').structures.find((s: { id: string }) => s.id === 'citadel')
    expect(citadel()).not.toHaveProperty('rings')
    await size.getByRole('radio', { name: /^7 hexagons/ }).click()
    await saved(page)
    expect(citadel().rings).toBe(1)
    expect(citadel()).not.toHaveProperty('width')
    expect(await game.problems(id)).toEqual([])
    await expect(page.getByRole('list', { name: 'Structures' }).getByText('7 hexagons').first()).toBeVisible()
    await size.getByRole('radio', { name: '1 hexagon' }).click()
    await saved(page)
    expect(citadel()).not.toHaveProperty('rings')
  })

  test(`${layout}: a new image is divided into hexagon-sized pieces (the box of a hexagon), with a quick button for it`, async ({ page }) => {
    const id = hexGame(layout)
    const [cw, ch] = BOX[layout]
    await openArt(page, id)
    await addImage(page, 'ground.png', makePng(10 * cw, 9 * ch, (x, y) => pieceColor(Math.floor(x / cw), Math.floor(y / ch))), 'ground')
    await expect(page.getByLabel('Piece width')).toHaveValue(String(cw))
    await expect(page.getByLabel('Piece height')).toHaveValue(String(ch))
    await expect(page.getByRole('button', { name: `Hexagon size (${cw} × ${ch})` })).toHaveAttribute('aria-pressed', 'true')
    expect(readDisk(id, 'data/graphics.json').sheets[0]).toMatchObject({ cellWidth: cw, cellHeight: ch })
  })

  test(`@smoke ${layout}: join blocks are 8 x 8 in the picker; the map draws the piece of the six-neighbour number`, async ({ page, request, game }) => {
    const id = hexGame(layout)
    const [cw, ch] = BOX[layout]
    // an image of 10 x 9 pieces, each piece one flat colour: an 8 x 8 block fits at (0,0) and (1,1) but not at (3,0)
    const sheet = makePng(10 * cw, 9 * ch, (x, y) => pieceColor(Math.floor(x / cw), Math.floor(y / ch)))
    await seedArt(request, id, { tiles: sheet }, { sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: cw, cellHeight: ch, name: 'Tiles' }] })
    await page.goto(editorUrl(id, 'terrain', 'road'))
    await page.getByRole('button', { name: 'Choose picture…' }).click()
    await picker(page).getByRole('radio', { name: 'Joins with neighbours' }).click()
    await expect(picker(page).getByText('Point at the top-left piece of the 64 pieces')).toBeVisible()
    await expect(picker(page).getByText('Each piece shows its hexagon with a bar towards every neighbour')).toBeVisible()
    const g = pickerGrid(page)
    await clickPiece(page, g, 3, 0)
    await expect(page.getByRole('alert').filter({ hasText: 'The 64 pieces from there would reach outside the image' })).toBeVisible()
    await expect(picker(page).getByRole('button', { name: 'Use this picture' })).toBeDisabled()
    await clickPiece(page, g, 1, 1)
    const [download] = await Promise.all([page.waitForEvent('download'), picker(page).getByRole('button', { name: 'Download a guide image' }).click()])
    expect(download.suggestedFilename()).toBe(`join-guide-${cw}x${ch}.png`)
    const guide = decodePng(readFileSync((await download.path())!))
    expect([guide.width, guide.height]).toEqual([8 * cw, 8 * ch])
    await picker(page).getByRole('button', { name: 'Use this picture' }).click()
    await saved(page)
    expect(readDisk(id, 'data/graphics.json').terrain.road).toEqual({ sheet: 'tiles', join: [1, 1] })
    expect(await game.problems(id)).toEqual([])

    // On the map a road hexagon shows the piece numbered by its six neighbours (bit i = neighbour i in the contract order,
    // the map's edge counts as joined), at column n % 8, row n / 8 of the block.
    await openMap(page, id, 'skirmish')
    const grid = readDisk(id, 'maps/skirmish.json').tiles.map((r: string) => [...r])
    const legend = readDisk(id, 'maps/skirmish.json').legend
    const isRoad = (x: number, y: number) => (x < 0 || y < 0 || y >= grid.length || x >= grid[0].length ? true : legend[grid[y][x]] === 'road')
    for (const [x, y] of [[3, 2], [4, 2], [8, 2]]) {
      expect(legend[grid[y][x]]).toBe('road')
      let n = 0
      neighboursOf(layout, x, y).forEach(([nx, ny], i) => { if (isRoad(nx, ny)) n |= 1 << i })
      const want = pieceColor(1 + (n % 8), 1 + Math.floor(n / 8))
      const p = await hexPoint(page, layout, x, y, -8, 0)
      const box = (await canvas(page).boundingBox())!
      await expect.poll(async () => {
        const px = await canvas(page).evaluate((el, [cx, cy]) => {
          const c = el as HTMLCanvasElement
          const d = c.getContext('2d')!.getImageData(cx, cy, 1, 1).data
          return [d[0], d[1], d[2]]
        }, [Math.round(p.x - box.x), Math.round(p.y - box.y)])
        return near(px, want, 6)
      }, { message: `road hexagon ${x},${y}: piece ${n}`, timeout: 7000 }).toBe(true)
    }
  })
}

test('@smoke the new-game cards show each template’s grid, and a game made from a hexagon template is a hexagon game with hexagon maps', async ({ page, request, game }) => {
  // A hexagon template: the engine's pointy fixture, copied into the helper's templates folder under its own name.
  const tpl = path.join(gamesDir(), '..', 'templates', 'zz-hex-fixture')
  cpSync(path.resolve(import.meta.dirname, '../../../../tests/fixtures/hex-pointy'), tpl, { recursive: true })
  await page.goto('/')
  await page.getByRole('button', { name: 'New game' }).click()
  const cards = page.getByRole('radiogroup').locator('label')
  await expect(cards.filter({ hasText: /^Blank/ }).getByTestId('template-cells')).toHaveText('Squares')
  await expect(cards.filter({ hasText: /^Strategy RPG/ }).getByTestId('template-cells')).toHaveText('Squares')
  await expect(cards.filter({ hasText: /Hex Fixture/ }).getByTestId('template-cells')).toHaveText('Hexagons, pointy-top')
  await page.getByRole('button', { name: 'Cancel' }).click()

  const id = await game.create('zz-hex-fixture', 'Made Hex')
  await page.goto(editorUrl(id, 'game'))
  await expect(page.getByTestId('game-grid')).toHaveText('Hexagons, pointy-top')
  await page.goto(editorUrl(id, 'maps'))
  await page.getByRole('button', { name: 'Show map list' }).click()
  await page.getByRole('button', { name: 'New map' }).click()
  await page.getByLabel('Name of the map').fill('Fresh')
  await page.keyboard.press('Enter')
  await expect(page).toHaveURL(/maps\/fresh/)
  await saved(page)
  expect(readDisk(id, 'game.json').grid).toEqual({ type: 'hex', layout: 'pointy' })
  expect(readDisk(id, 'maps/fresh.json').grid).toMatchObject({ type: 'hex', width: 12, height: 8 })
  expect(await game.problems(id)).toEqual([])
  await expect(page.getByRole('button', { name: /^Fresh/ })).toContainText('12 × 8 hexagons')
  void request
})
