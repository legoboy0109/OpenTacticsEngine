// Painting terrain. Both functions return the same map object when nothing changed
// (so a stroke over already-painted squares adds no undo step).
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

/** Flood fill: the 4-connected area of the same terrain around (x, y). */
export function fillFrom(map: MapFile, terrain: Terrain[], x: number, y: number, terrainId: string): MapFile {
  const g = readGrid(map, terrain)
  if (x < 0 || y < 0 || x >= g.width || y >= g.height) return map
  const from = g.cells[y * g.width + x]
  if (from === terrainId || !terrain.some((t) => t.id === terrainId)) return map
  const stack = [y * g.width + x]
  while (stack.length) {
    const i = stack.pop()!
    if (g.cells[i] !== from) continue
    g.cells[i] = terrainId
    const cx = i % g.width
    if (cx > 0) stack.push(i - 1)
    if (cx < g.width - 1) stack.push(i + 1)
    if (i >= g.width) stack.push(i - g.width)
    if (i < g.cells.length - g.width) stack.push(i + g.width)
  }
  return writeGrid(map, g, terrain)
}

/** The squares on a straight line between two squares (inclusive): a fast drag leaves no gaps. */
export function lineCells(a: Cell, b: Cell): Cell[] {
  const out: Cell[] = []
  const n = Math.max(Math.abs(b.x - a.x), Math.abs(b.y - a.y))
  for (let i = 0; i <= n; i++) {
    const t = n === 0 ? 0 : i / n
    out.push({ x: Math.round(a.x + (b.x - a.x) * t), y: Math.round(a.y + (b.y - a.y) * t) })
  }
  return out
}
