// Helpers for the hexagon tests: hexagon games copied from the engine's fixtures into the test's temp games folder, and
// pointer positions for hexagons worked out here from docs/HEX.md (not from the app's own code), so a wrong picture or a
// wrong picking rule shows up as a failing test.
import type { Page } from '@playwright/test'
import { cpSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import { canvas } from './mapkit'
import { parseLoose } from '../unit/fixtures'
import { expect, diskPath, readDisk } from './fixtures'

export type Layout = 'pointy' | 'flat'
const repo = path.resolve(import.meta.dirname, '../../../..')
let counter = 0

/** Copies tests/fixtures/hex-<layout> (a game without art) into the temp games folder under a fresh id; returns the id. */
export function hexGame(layout: Layout): string {
  const id = `hx${layout[0]}${Date.now().toString(36)}${(counter++).toString(36)}${Math.random().toString(36).slice(2, 5)}`
  cpSync(path.join(repo, 'tests/fixtures', `hex-${layout}`), diskPath(id, ''), { recursive: true })
  // The README is not part of a game.
  rmSync(diskPath(id, 'README.md'), { force: true })
  // The fixtures are hand-written with comments; the tests read the files the editor writes, which are strict JSON.
  for (const f of jsonFiles(diskPath(id, ''))) writeFileSync(f, `${JSON.stringify(parseLoose(readFileSync(f, 'utf8')), null, 2)}\n`)
  return id
}

function jsonFiles(dir: string): string[] {
  return readdirSync(dir).flatMap((n) => {
    const p = path.join(dir, n)
    return statSync(p).isDirectory() ? jsonFiles(p) : p.endsWith('.json') ? [p] : []
  })
}

export const TILE = 24
/** L: the multiple of 4 nearest to T * 1.1547 (docs/HEX.md). */
export const L = 28

/** Middle of hexagon (x, y) in pixels of the tile size, straight from the contract's box formulas. */
export function middleOf(layout: Layout, x: number, y: number): { x: number; y: number } {
  if (layout === 'pointy') return { x: x * TILE + (y % 2 ? TILE / 2 : 0) + TILE / 2, y: (y * 3 * L) / 4 + L / 2 }
  return { x: (x * 3 * L) / 4 + L / 2, y: y * TILE + (x % 2 ? TILE / 2 : 0) + TILE / 2 }
}

/** Page coordinates of the middle of hexagon (x, y), from the canvas' own zoom and pan (data-view = "scale,x,y", scale = pixels per tile). */
export async function hexPoint(page: Page, layout: Layout, x: number, y: number, dx = 0, dy = 0): Promise<{ x: number; y: number }> {
  await page.evaluate(() => new Promise<void>((r) => requestAnimationFrame(() => requestAnimationFrame(() => r()))))
  const box = (await canvas(page).boundingBox())!
  const [scale, vx, vy] = (await canvas(page).getAttribute('data-view'))!.split(',').map(Number)
  const m = middleOf(layout, x, y)
  return { x: box.x + vx + ((m.x + dx) / TILE) * scale, y: box.y + vy + ((m.y + dy) / TILE) * scale }
}

export async function clickHex(page: Page, layout: Layout, x: number, y: number): Promise<void> {
  const p = await hexPoint(page, layout, x, y)
  await page.mouse.click(p.x, p.y)
}

/** One drag through the middles of the given hexagons. */
export async function dragHexes(page: Page, layout: Layout, cells: Array<[number, number]>): Promise<void> {
  const pts = []
  for (const [x, y] of cells) pts.push(await hexPoint(page, layout, x, y))
  await page.mouse.move(pts[0].x, pts[0].y)
  await page.mouse.down()
  for (const p of pts.slice(1)) await page.mouse.move(p.x, p.y, { steps: 4 })
  await page.mouse.up()
}

export async function hoverHex(page: Page, layout: Layout, x: number, y: number): Promise<void> {
  const p = await hexPoint(page, layout, x, y)
  await page.mouse.move(p.x, p.y)
  await page.evaluate(() => new Promise<void>((r) => requestAnimationFrame(() => requestAnimationFrame(() => r()))))
}

/** The map's saved grid of terrain ids. */
export function diskHexGrid(game: string, mapFile = 'skirmish'): string[][] {
  const m = readDisk(game, `maps/${mapFile}.json`)
  return m.tiles.map((r: string) => [...r].map((c) => m.legend[c]))
}

/** The six neighbours of (x, y) in the contract's order (written out here from the contract text, independently of the app). */
export function neighboursOf(layout: Layout, x: number, y: number): Array<[number, number]> {
  const odd = (layout === 'pointy' ? y : x) % 2 === 1
  const table = layout === 'pointy'
    ? (odd ? [[1, -1], [1, 0], [1, 1], [0, 1], [-1, 0], [0, -1]] : [[0, -1], [1, 0], [0, 1], [-1, 1], [-1, 0], [-1, -1]])
    : (odd ? [[0, -1], [1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0]] : [[0, -1], [1, -1], [1, 0], [0, 1], [-1, 0], [-1, -1]])
  return table.map(([dx, dy]) => [x + dx, y + dy] as [number, number])
}

/** What a six-neighbour flood fill of `to` from (x, y) must produce. */
export function expectedHexFill(layout: Layout, grid: string[][], x: number, y: number, to: string): string[][] {
  const out = grid.map((r) => r.slice())
  const from = out[y][x]
  if (from === to) return out
  const todo: Array<[number, number]> = [[x, y]]
  while (todo.length) {
    const [cx, cy] = todo.pop()!
    if (out[cy]?.[cx] !== from) continue
    out[cy][cx] = to
    todo.push(...neighboursOf(layout, cx, cy))
  }
  return out
}

export { expect }
