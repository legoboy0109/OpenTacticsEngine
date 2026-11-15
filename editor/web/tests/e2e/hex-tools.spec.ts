// The other map tools on hexagon games: story areas (still rectangles of columns and rows, drawn as the hexagons they contain),
// the sight aid ("Show what a side sees" counts hexagon steps), the Select panel and the Heights tool.
import type { Page } from '@playwright/test'
import { expect, readDisk, saved, test } from './fixtures'
import { canvas, openMap, tab, tool } from './mapkit'
import { clickHex, dragHexes, hexGame, hexPoint, neighboursOf, type Layout } from './hexkit'
import { decodePng } from './png'

const LAYOUTS: Layout[] = ['pointy', 'flat']

async function pixelAt(page: Page, layout: Layout, x: number, y: number, dx: number, dy: number): Promise<number[]> {
  const p = await hexPoint(page, layout, x, y, dx, dy)
  const box = (await canvas(page).boundingBox())!
  const img = decodePng(await canvas(page).screenshot())
  const i = (Math.round(p.y - box.y) * img.width + Math.round(p.x - box.x)) * 4
  return [img.data[i], img.data[i + 1], img.data[i + 2]]
}

/** Cells within `r` steps of (x, y) on a 12 x 9 map, by walking the contract's neighbour tables (not the app's code). */
function within(layout: Layout, x: number, y: number, r: number): Set<string> {
  const seen = new Map<string, number>([[`${x},${y}`, 0]])
  const queue: Array<[number, number]> = [[x, y]]
  while (queue.length) {
    const [cx, cy] = queue.shift()!
    const d = seen.get(`${cx},${cy}`)!
    if (d === r) continue
    for (const [nx, ny] of neighboursOf(layout, cx, cy)) {
      if (nx < 0 || ny < 0 || nx >= 12 || ny >= 9 || seen.has(`${nx},${ny}`)) continue
      seen.set(`${nx},${ny}`, d + 1)
      queue.push([nx, ny])
    }
  }
  return new Set(seen.keys())
}

for (const layout of LAYOUTS) {
  test(`@smoke ${layout}: a story area is still columns and rows and is drawn as the hexagons it contains`, async ({ page, game }) => {
    const id = hexGame(layout)
    await openMap(page, id, 'skirmish')
    const before = await pixelAt(page, layout, 3, 1, -8, 0)
    await tab(page, 'Objectives').click()
    const panel = page.getByTestId('objectives-panel')
    await panel.getByLabel('map: Add a way to win…').selectOption({ label: 'A unit reaches an area' })
    await panel.getByRole('button', { name: 'Way to win 1: area: Pick on the map' }).click()
    await expect(page.getByText('Click or drag on the map')).toBeVisible()
    // drag from hexagon (2,1) to hexagon (4,3): the area is the rectangle of columns 3-5, rows 2-4 (from 1), 3 x 3
    await dragHexes(page, layout, [[2, 1], [3, 2], [4, 3]])
    await saved(page)
    await expect(canvas(page)).toHaveAttribute('data-areas', 'win:2,1,3,3')
    expect(readDisk(id, 'maps/skirmish.json').objectives.win).toEqual([{ reach: { x: 2, y: 1, w: 3, h: 3 } }])
    await expect(panel.getByText('area 3, 2 (3 × 3)', { exact: true })).toBeVisible()
    // the hexagons inside are tinted, one outside is not
    await page.mouse.move(0, 0)
    const inside = await pixelAt(page, layout, 3, 1, -8, 0)
    expect(inside.join()).not.toBe(before.join())
    const outside = await pixelAt(page, layout, 8, 0, -8, 0)
    const colors: Record<string, string> = Object.fromEntries(readDisk(id, 'data/terrain.json').terrain.map((t: { id: string; color: string }) => [t.id, t.color]))
    const grid = readDisk(id, 'maps/skirmish.json').tiles.map((r: string) => [...r])
    const legend = readDisk(id, 'maps/skirmish.json').legend
    const base = colors[legend[grid[0][8]]]
    expect(Math.abs(outside[0] - parseInt(base.slice(1, 3), 16))).toBeLessThanOrEqual(4)
    expect(await game.problems(id)).toEqual([])
  })

  test(`@smoke ${layout}: the sight aid dims everything beyond hexagon distance`, async ({ page, game }) => {
    const id = hexGame(layout)
    await openMap(page, id, 'skirmish')
    await tab(page, 'Map').click()
    await page.getByRole('checkbox', { name: /^Fog of war/ }).check()
    await saved(page)
    expect(readDisk(id, 'maps/skirmish.json').fog).toBe(true)
    await page.getByRole('checkbox', { name: /^Show what a side sees/ }).check()
    await page.getByLabel('Side', { exact: true }).selectOption({ label: 'Blue Army' })
    // what blue's team sees: every blue unit (sight 2, the default) and one step around every hexagon of every structure blue owns
    const map = readDisk(id, 'maps/skirmish.json')
    const want = new Set<string>()
    for (const u of map.units.filter((u: { side: string }) => u.side === 'blue')) within(layout, u.x, u.y, 2).forEach((c) => want.add(c))
    for (const s of map.structures.filter((s: { side?: string }) => s.side === 'blue')) {
      const cells: Array<[number, number]> = s.type === 'hq' ? [[s.x, s.y], ...neighboursOf(layout, s.x, s.y)] : [[s.x, s.y]]
      for (const [cx, cy] of cells) if (cx >= 0 && cy >= 0 && cx < 12 && cy < 9) within(layout, cx, cy, 1).forEach((c) => want.add(c))
    }
    const expected = Array.from({ length: 9 }, (_, y) => Array.from({ length: 12 }, (_, x) => (want.has(`${x},${y}`) ? '1' : '0')).join('')).join('')
    await expect(canvas(page)).toHaveAttribute('data-fog', expected)
    expect(expected).toContain('0')
    expect(expected).toContain('1')
    expect(await game.problems(id)).toEqual([])
  })

  test(`${layout}: Select tells what is on a hexagon; Heights raises the hexagons of one drag in one undo step`, async ({ page }) => {
    const id = hexGame(layout)
    await openMap(page, id, 'skirmish')
    await tool(page, 'Select').click()
    await clickHex(page, layout, 5, 3)
    const panel = page.getByRole('heading', { name: /^Hexagon/ })
    await expect(panel).toContainText('6, 4')
    await expect(page.getByTestId('square-height')).toHaveText('0')
    await tool(page, 'Heights').click()
    await expect(page.getByTestId('heights-tool')).toContainText('Click or drag over hexagons')
    await dragHexes(page, layout, [[2, 0], [3, 0], [4, 0]])
    await saved(page)
    const heights = () => readDisk(id, 'maps/skirmish.json').heights
    expect(heights()[0].slice(0, 6)).toEqual([0, 0, 1, 1, 1, 0])
    await page.getByRole('button', { name: 'Undo' }).click()
    await saved(page)
    expect(readDisk(id, 'maps/skirmish.json')).not.toHaveProperty('heights')
  })
}
