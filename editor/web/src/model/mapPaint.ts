// Painting terrain. Both functions return the same map object when nothing changed
// (so a stroke over already-painted squares adds no undo step).
import { lineOf, neighboursOf, SQUARE, type GridSpec } from './gridSpec'
import { readGrid, writeGrid, type Cell } from './mapGrid'
import type { MapFile, Terrain } from './types'

export function paintCells(map: MapFile, terrain: Terrain[], cells: Cell[], terrainId: string): MapFile {
  if (!terrain.some((t) => t.id === terrainId)) return map
  const g = readGrid(map, terrain)
  let changed = false
  for (const { x, y } of cells) {
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) continue
    const i = y * g.width + x
    if (g.cells[i] !== terrainId) { g.cells[i] = terrainId; changed = true }
  }
  return changed ? writeGrid(map, g, terrain) : map
}

/** Flood fill: the connected area of the same terrain around (x, y), through the neighbours the grid has
 *  (squares: the four around; hexagons: the six around). */
export function fillFrom(map: MapFile, terrain: Terrain[], x: number, y: number, terrainId: string, grid: GridSpec = SQUARE): MapFile {
  const g = readGrid(map, terrain)
  if (x < 0 || y < 0 || x >= g.width || y >= g.height) return map
  const from = g.cells[y * g.width + x]
  if (from === terrainId || !terrain.some((t) => t.id === terrainId)) return map
  const stack = [y * g.width + x]
  while (stack.length) {
    const i = stack.pop()!
    if (g.cells[i] !== from) continue
    g.cells[i] = terrainId
    for (const n of neighboursOf(grid, g.width, g.height, { x: i % g.width, y: Math.floor(i / g.width) })) stack.push(n.y * g.width + n.x)
  }
  return writeGrid(map, g, terrain)
}

/** The cells on a straight line between two cells (inclusive): a fast drag leaves no gaps. */
export const lineCells = (a: Cell, b: Cell, grid: GridSpec = SQUARE): Cell[] => lineOf(grid, a, b)
