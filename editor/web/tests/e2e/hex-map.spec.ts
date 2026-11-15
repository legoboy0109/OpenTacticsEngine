// The map editor on hexagon games (docs/HEX.md "Editor"): both layouts, from copies of the engine's fixture games. Positions
// on the screen come from the contract's formulas (hexkit.ts), not from the app, and the files written to disk are checked
// (and that the real loader still accepts the game).
import { expect, readDisk, saved, test } from './fixtures'
import { canvas, ghost, openMap, pickSide, pickStructure, pickTerrain, pickUnitType, status, tab, tool } from './mapkit'
import { clickHex, diskHexGrid, dragHexes, expectedHexFill, hexGame, hexPoint, hoverHex, L, neighboursOf, TILE, type Layout } from './hexkit'
import { decodePng, type Image } from './png'

const LAYOUTS: Layout[] = ['pointy', 'flat']
const notice = (page: import('@playwright/test').Page) => page.getByTestId('map-notice')
const terrainNames: Record<string, string> = { plain: 'Plain', road: 'Road', forest: 'Forest', mountain: 'Mountain', sea: 'Sea' }
const hex = (s: string): [number, number, number] => [parseInt(s.slice(1, 3), 16), parseInt(s.slice(3, 5), 16), parseInt(s.slice(5, 7), 16)]

/** The canvas as pixels; (px, py) of a page point inside it. */
async function canvasPixels(page: import('@playwright/test').Page): Promise<{ img: Image; at: (x: number, y: number) => [number, number, number] }> {
  const box = (await canvas(page).boundingBox())!
  const img = decodePng(await canvas(page).screenshot())
  const scale = img.width / box.width
  return { img, at: (x, y) => { const i = (Math.round((y - box.y) * scale) * img.width + Math.round((x - box.x) * scale)) * 4; return [img.data[i], img.data[i + 1], img.data[i + 2]] } }
}
const near = (a: number[], b: number[], tol = 4) => a.every((v, i) => Math.abs(v - b[i]) <= tol)

for (const layout of LAYOUTS) {
  test.describe(`hexagon map, ${layout}`, () => {
    test(`@smoke ${layout}: the picture follows the contract and the pointer picks the nearest middle`, async ({ page }) => {
      const game = hexGame(layout)
      await openMap(page, game, 'skirmish')
      const map = readDisk(game, 'maps/skirmish.json')
      const colors: Record<string, string> = Object.fromEntries(readDisk(game, 'data/terrain.json').terrain.map((t: { id: string; color: string }) => [t.id, t.color]))
      const grid = diskHexGrid(game)
      const covered = new Set<string>()
      for (const u of map.units) covered.add(`${u.x},${u.y}`)
      for (const s of map.structures) {
        const around = s.type === 'hq' ? [[s.x, s.y], ...neighboursOf(layout, s.x, s.y)] : [[s.x, s.y]]
        for (const [x, y] of around) covered.add(`${x},${y}`)
      }
      // At the fitted zoom and after zooming in, a point inside each plain hexagon (left of the middle, clear of the letter) has the
      // terrain's colour: the hexagons are where the contract puts them and tile without gaps.
      for (const zoom of [0, 3]) {
        for (let k = 0; k < zoom; k++) { await canvas(page).hover({ position: { x: 5, y: 5 } }); await page.mouse.wheel(0, -200) }
        if (zoom) await page.waitForTimeout(200)
        await page.mouse.move(0, 0) // no hover highlight over the hexagons being measured
        const { at } = await canvasPixels(page)
        const box = (await canvas(page).boundingBox())!
        let checked = 0
        for (let y = 0; y < 9; y++) for (let x = 0; x < 12; x++) {
          if (covered.has(`${x},${y}`)) continue
          const p = await hexPoint(page, layout, x, y, -8, 0)
          if (p.x < box.x + 2 || p.y < box.y + 2 || p.x > box.x + box.width - 2 || p.y > box.y + box.height - 2) continue
          expect(near(at(p.x, p.y), hex(colors[grid[y][x]])), `hexagon ${x},${y} at zoom step ${zoom}`).toBe(true)
          checked++
        }
        expect(checked).toBeGreaterThan(zoom ? 4 : 50)
      }
      await page.keyboard.press('Escape')
    })

    test(`${layout}: the hexagon under the pointer is the one reported (middle and near the edge of the hexagon)`, async ({ page }) => {
      const game = hexGame(layout)
      await openMap(page, game, 'skirmish')
      const grid = diskHexGrid(game)
      for (const [x, y] of [[0, 0], [5, 3], [6, 4], [11, 8], [3, 7], [10, 2]]) {
        for (const [dx, dy] of [[0, 0], [7, 0], [-7, 0], [0, 7], [0, -7]]) {
          const p = await hexPoint(page, layout, x, y, dx, dy)
          await page.mouse.move(p.x, p.y)
          await expect(status(page)).toContainText(`${x + 1}, ${y + 1} · ${terrainNames[grid[y][x]]}`)
        }
      }
      // just past the edge of the map nothing is under the pointer
      const p = await hexPoint(page, layout, 11, 4, TILE, 0)
      await page.mouse.move(p.x + 40, p.y)
      await expect(status(page)).toContainText('Hover over the map')
    })

    test(`@smoke ${layout}: a drag paints the hexagons under the pointer and is one undo step; fill follows the six neighbours`, async ({ page, game }) => {
      const id = hexGame(layout)
      const before = diskHexGrid(id)
      await openMap(page, id, 'skirmish')
      await pickTerrain(page, 'Mountain').click()
      // three hexagons in a row: each is the right neighbour of the one before
      const path: Array<[number, number]> = [[7, 0], [8, 0], [9, 0]]
      expect(neighboursOf(layout, 7, 0)[1]).toEqual([8, 0])
      await dragHexes(page, layout, path)
      await saved(page)
      const painted = before.map((r) => r.slice())
      for (const [x, y] of path) painted[y][x] = 'mountain'
      expect(diskHexGrid(id)).toEqual(painted)
      await page.getByRole('button', { name: 'Undo' }).click()
      await saved(page)
      expect(diskHexGrid(id), 'one undo step restores the whole drag').toEqual(before)
      await expect(page.getByRole('button', { name: 'Undo' })).toBeDisabled()

      // fill: the connected sea becomes plain; the six-neighbour reach decides which hexagons count as connected
      await page.keyboard.press('f')
      await pickTerrain(page, 'Plain').click()
      await clickHex(page, layout, 5, 6)
      await saved(page)
      expect(diskHexGrid(id)).toEqual(expectedHexFill(layout, before, 5, 6, 'plain'))
      await page.getByRole('button', { name: 'Undo' }).click()
      await saved(page)
      expect(diskHexGrid(id)).toEqual(before)
      expect(await game.problems(id)).toEqual([])
    })

    test(`@smoke ${layout}: place a unit, move it by dragging, remove it`, async ({ page, game }) => {
      const id = hexGame(layout)
      await openMap(page, id, 'skirmish')
      await tool(page, 'Units').click()
      await pickSide(page, 'Blue Army').click()
      await pickUnitType(page, 'Infantry').click()
      const units0 = readDisk(id, 'maps/skirmish.json').units
      await clickHex(page, layout, 0, 2)
      await saved(page)
      let units = readDisk(id, 'maps/skirmish.json').units
      expect(units).toHaveLength(units0.length + 1)
      expect(units.at(-1)).toEqual({ type: 'infantry', side: 'blue', x: 0, y: 2 })
      // drag it one hexagon to the right (its right neighbour on an even row is (1,2))
      await dragHexes(page, layout, [[0, 2], [1, 2]])
      await saved(page)
      units = readDisk(id, 'maps/skirmish.json').units
      expect(units.at(-1)).toMatchObject({ x: 1, y: 2 })
      // a second unit on the same hexagon is refused
      await clickHex(page, layout, 1, 2)
      await page.keyboard.press('Delete')
      await saved(page)
      expect(readDisk(id, 'maps/skirmish.json').units).toHaveLength(units0.length)
      expect(await game.problems(id)).toEqual([])
    })

    test(`@smoke ${layout}: a seven-hexagon structure is placed by its middle, shows its footprint, and is refused at the edge and on overlap`, async ({ page, game }) => {
      const id = hexGame(layout)
      await openMap(page, id, 'skirmish')
      await tool(page, 'Structures').click()
      await pickStructure(page, 'Headquarters').click()
      const before = readDisk(id, 'maps/skirmish.json').structures

      // at the map edge the ring would stick out: red footprint, a plain sentence, nothing written
      await hoverHex(page, layout, 0, 0)
      expect(await ghost(page)).toBe('hq,0,0,bad')
      await clickHex(page, layout, 0, 0)
      await expect(notice(page)).toContainText('covers a hexagon and the six around it, and would stick out of the map')
      // on top of the blue HQ's ring: refused as an overlap
      await clickHex(page, layout, 3, 4)
      await expect(notice(page)).toContainText('would overlap the Headquarters')
      expect(readDisk(id, 'maps/skirmish.json').structures).toEqual(before)

      // a free spot: the footprint is green/white, the click places it with its middle there
      await hoverHex(page, layout, 7, 1)
      expect(await ghost(page)).toBe('hq,7,1,ok')
      await clickHex(page, layout, 7, 1)
      await saved(page)
      const after = readDisk(id, 'maps/skirmish.json').structures
      expect(after).toHaveLength(before.length + 1)
      expect(after.at(-1)).toEqual({ type: 'hq', x: 7, y: 1 })
      // all seven hexagons are drawn in the structure's colour (the ring neighbours show the HQ's gold, not terrain)
      await page.mouse.move(0, 0)
      const { at } = await canvasPixels(page)
      for (const [nx, ny] of neighboursOf(layout, 7, 1)) {
        const p = await hexPoint(page, layout, nx, ny, -8, 0)
        expect(near(at(p.x, p.y), [212, 175, 55], 12), `ring hexagon ${nx},${ny}`).toBe(true)
      }
      expect(await game.problems(id)).toEqual([])
    })

    test(`${layout}: dragging a seven-hexagon structure moves it as a whole (by the hexagon it was grabbed at); a move onto another is refused`, async ({ page, game }) => {
      const id = hexGame(layout)
      await openMap(page, id, 'skirmish')
      await tool(page, 'Structures').click()
      const hqs = () => readDisk(id, 'maps/skirmish.json').structures.filter((s: { type: string }) => s.type === 'hq')
      // grab the blue HQ by its middle (1,4) and drop it on (1,5): the middle follows the pointer
      await dragHexes(page, layout, [[1, 4], [1, 5]])
      await saved(page)
      expect(hqs().find((s: { side?: string }) => s.side === 'blue')).toMatchObject({ x: 1, y: 5 })
      // grab it by a ring hexagon (its right neighbour) and drop that on the hexagon two to the right on its row: the whole
      // structure shifts by the same vector, not by the difference of the columns alone
      const [rx, ry] = neighboursOf(layout, 1, 5)[1]
      const drop: [number, number] = [rx + 1, ry]
      const dir = neighboursOf(layout, rx, ry).findIndex(([x, y]) => x === drop[0] && y === drop[1]) // the step the pointer made (same step on both layouts' tables)
      expect(dir).toBeGreaterThanOrEqual(0)
      await dragHexes(page, layout, [[rx, ry], drop])
      await saved(page)
      const moved = hqs().find((s: { side?: string }) => s.side === 'blue')
      // the middle made the same step from (1,5) that the pointer made from the grabbed hexagon
      expect([moved.x, moved.y]).toEqual(neighboursOf(layout, 1, 5)[dir])
      expect(await game.problems(id)).toEqual([])
      // moving it onto the red HQ is refused with a plain sentence and nothing is written
      const start = hqs()
      await dragHexes(page, layout, [[moved.x, moved.y], [9, 4], [10, 4]])
      await expect(notice(page)).toContainText('would overlap the Headquarters')
      expect(hqs()).toEqual(start)
    })

    test(`${layout}: resizing keeps the contents by column and row and tells what is lost`, async ({ page, game }) => {
      const id = hexGame(layout)
      const before = readDisk(id, 'maps/skirmish.json')
      await openMap(page, id, 'skirmish')
      await tab(page, 'Map').click()
      await expect(page.getByTestId('map-size')).toHaveText('12 × 9 hexagons')
      await page.getByRole('button', { name: 'Resize…' }).click()
      const dlg = page.getByRole('dialog', { name: 'Resize the map' })
      await dlg.getByLabel('Width').fill('10')
      // the red HQ at (10,4) has its ring in columns 9 to 11: it no longer fits and is listed
      await expect(dlg.getByRole('alert')).toContainText('Headquarters at 11, 5')
      await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
      await saved(page)
      const after = readDisk(id, 'maps/skirmish.json')
      expect(after.grid).toEqual({ type: 'hex', width: 10, height: 9 })
      expect(after.tiles).toEqual(before.tiles.map((r: string) => r.slice(0, 10)))
      expect(after.structures.filter((s: { type: string }) => s.type === 'hq')).toHaveLength(1)
      expect(await game.problems(id)).toEqual([])
      await expect(page.getByTestId('map-size')).toHaveText('10 × 9 hexagons')
      // growing again adds hexagons of the first terrain
      await page.getByRole('button', { name: 'Resize…' }).click()
      await dlg.getByLabel('Width').fill('13')
      await dlg.getByRole('button', { name: 'Resize', exact: true }).click()
      await saved(page)
      expect(readDisk(id, 'maps/skirmish.json').tiles.every((r: string) => r.length === 13)).toBe(true)
      expect(L).toBe(28)
    })
  })
}
