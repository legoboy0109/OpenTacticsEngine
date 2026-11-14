// Helpers for driving the map editor's canvas like a person would (mouse / touch), never through internals.
import type { Locator, Page } from '@playwright/test'
import { editorUrl, expect, readDisk } from './fixtures'

/** The first map of each starting template. */
export const TEMPLATE_MAPS = {
  'strategy-rpg': 'ford',
  'economy-tactics': 'crossroads',
  blank: 'start',
} as const
export type TemplateName = keyof typeof TEMPLATE_MAPS

export const canvas = (page: Page): Locator => page.getByTestId('map-canvas')
const frames = (page: Page) => page.evaluate(() => new Promise<void>((r) => requestAnimationFrame(() => requestAnimationFrame(() => r()))))

export async function openMap(page: Page, game: string, mapId: string): Promise<void> {
  await page.goto(editorUrl(game, 'maps', mapId))
  await expect(canvas(page)).toHaveAttribute('data-view', /.+/)
  await frames(page)
}

/** Page coordinates of the middle of square (x, y), from the canvas' own view. */
export async function cellPoint(page: Page, x: number, y: number): Promise<{ x: number; y: number }> {
  await frames(page)
  const box = (await canvas(page).boundingBox())!
  const [scale, vx, vy] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  return { x: box.x + vx + (x + 0.5) * scale, y: box.y + vy + (y + 0.5) * scale }
}

export async function clickCell(page: Page, x: number, y: number, opts: { modifiers?: Array<'Alt' | 'Shift'> } = {}): Promise<void> {
  const p = await cellPoint(page, x, y)
  for (const m of opts.modifiers ?? []) await page.keyboard.down(m)
  await page.mouse.click(p.x, p.y)
  for (const m of opts.modifiers ?? []) await page.keyboard.up(m)
}

/** Press on the first square, move through the others, release. One drag. */
export async function dragThrough(page: Page, cells: Array<[number, number]>): Promise<void> {
  const pts = []
  for (const [x, y] of cells) pts.push(await cellPoint(page, x, y))
  await page.mouse.move(pts[0].x, pts[0].y)
  await page.mouse.down()
  for (const p of pts.slice(1)) await page.mouse.move(p.x, p.y, { steps: 3 })
  await page.mouse.up()
}

export const row = (y: number, x0: number, x1: number): Array<[number, number]> =>
  Array.from({ length: x1 - x0 + 1 }, (_, i) => [x0 + i, y] as [number, number])

export const tool = (page: Page, label: 'Terrain' | 'Units' | 'Structures' | 'Heights' | 'Select') => page.getByRole('toolbar', { name: 'Map tools' }).getByRole('button', { name: new RegExp(`^${label}`) })
export const pickTerrain = (page: Page, name: string) => page.getByRole('radiogroup', { name: 'Terrain' }).getByRole('radio', { name })
export const pickUnitType = (page: Page, name: string) => page.getByRole('radiogroup', { name: 'Unit type' }).getByRole('radio', { name })
export const pickStructure = (page: Page, name: string) => page.getByRole('radiogroup', { name: 'Structure' }).getByRole('radio', { name: new RegExp(`^${name}`) })
export const pickSide = (page: Page, name: string) => page.getByRole('radiogroup', { name: /^(Side|Owner)$/ }).getByRole('radio', { name })

export const tab = (page: Page, name: 'Tool' | 'Map' | 'Sides') => page.getByRole('tab', { name, exact: true })

/** The terrain id at a square, read from the saved file (stable, independent of the UI). */
export function diskCell(game: string, mapFile: string, x: number, y: number): string {
  const m = readDisk(game, `maps/${mapFile}.json`)
  return m.legend[m.tiles[y][x]]
}

export const status = (page: Page) => page.getByTestId('map-status')

/** The saved map as a grid of terrain ids. */
export function diskGrid(game: string, mapFile: string): string[][] {
  const m = readDisk(game, `maps/${mapFile}.json`)
  return m.tiles.map((r: string) => [...r].map((c) => m.legend[c]))
}

/** What a 4-connected flood fill of `to` from (x, y) must produce (independent of the app's own code). */
export function expectedFill(grid: string[][], x: number, y: number, to: string): string[][] {
  const out = grid.map((r) => r.slice())
  const from = out[y][x]
  if (from === to) return out
  const todo: Array<[number, number]> = [[x, y]]
  while (todo.length) {
    const [cx, cy] = todo.pop()!
    if (out[cy]?.[cx] !== from) continue
    out[cy][cx] = to
    todo.push([cx + 1, cy], [cx - 1, cy], [cx, cy + 1], [cx, cy - 1])
  }
  return out
}

/** Hover the pointer over square (x, y) (no click): a structure ghost follows it. */
export async function hoverCell(page: Page, x: number, y: number): Promise<void> {
  const p = await cellPoint(page, x, y)
  await page.mouse.move(p.x, p.y)
  await frames(page)
}

/** The structure ghost the canvas shows: "kind,x,y,ok|bad" (empty = none). */
export const ghost = (page: Page): Promise<string> => canvas(page).evaluate((c) => (c as HTMLCanvasElement).dataset.ghost ?? '')

/** The saved structures of a map file, as written on disk. */
export const diskStructures = (game: string, mapFile: string): Array<{ type: string; x: number; y: number; side?: string }> =>
  readDisk(game, `maps/${mapFile}.json`).structures ?? []
