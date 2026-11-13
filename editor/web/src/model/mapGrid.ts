// Maps are edited as a grid of terrain ids; `legend` + `tiles` are rebuilt from it.
import { assignLegend } from './legend'
import type { MapFile, Terrain } from './types'

export const MAP_MAX = 256
export interface Cell { x: number; y: number }
export interface Grid { width: number; height: number; cells: string[] }

const isSize = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n) && n >= 1 && n <= MAP_MAX
const rowsOf = (map: MapFile): string[] => (Array.isArray(map.tiles) ? map.tiles.map((r) => (typeof r === 'string' ? r : '')) : [])
const legendOf = (map: MapFile): Record<string, string> => (map.legend && typeof map.legend === 'object' ? map.legend : {})

/** Everything wrong with how the map's grid is stored, in plain words (empty = fine). */
export function gridProblems(map: MapFile, terrain: Terrain[]): string[] {
  const out: string[] = []
  const ids = new Set(terrain.map((t) => t.id))
  const width = map.grid?.width
  const height = map.grid?.height
  if (!isSize(width) || !isSize(height)) out.push(`The size is not a whole number from 1 to ${MAP_MAX}.`)
  if (!Array.isArray(map.tiles)) { out.push('The rows of squares are missing.'); return out }
  if (isSize(height) && map.tiles.length !== height) out.push(`It should have ${height} rows but has ${map.tiles.length}.`)
  const bad = map.tiles.findIndex((r) => typeof r !== 'string' || (isSize(width) && r.length !== width))
  if (bad >= 0) out.push(`Row ${bad + 1} is not ${isSize(width) ? width : 'the right number of'} squares wide.`)
  const legend = legendOf(map)
  const unknown = new Set<string>()
  for (const r of rowsOf(map)) for (const ch of r) if (!(ch in legend)) unknown.add(ch)
  if (unknown.size) out.push(`Some squares use a symbol with no terrain (${[...unknown].map((c) => `"${c}"`).join(', ')}).`)
  for (const [ch, id] of Object.entries(legend)) {
    if (!ids.has(id)) out.push(`The symbol "${ch}" names terrain "${id}", which this game does not have.`)
  }
  if (map.heights !== undefined && isSize(width) && isSize(height)) {
    const h = map.heights
    if (!Array.isArray(h) || h.length !== height || h.some((r) => !Array.isArray(r) || r.length !== width)) out.push('The height data does not match the size.')
  }
  return out
}

/** Reads any map, however damaged: bad squares become the first terrain. */
export function readGrid(map: MapFile, terrain: Terrain[]): Grid {
  const fallback = terrain[0]?.id ?? ''
  const ids = new Set(terrain.map((t) => t.id))
  const rows = rowsOf(map)
  const width = isSize(map.grid?.width) ? map.grid.width : Math.min(MAP_MAX, Math.max(1, ...rows.map((r) => r.length)))
  const height = isSize(map.grid?.height) ? map.grid.height : Math.min(MAP_MAX, Math.max(1, rows.length))
  const legend = legendOf(map)
  const cells: string[] = new Array(width * height)
  for (let y = 0; y < height; y++) {
    const row = rows[y] ?? ''
    for (let x = 0; x < width; x++) {
      const id = legend[row[x]]
      cells[y * width + x] = typeof id === 'string' && ids.has(id) ? id : fallback
    }
  }
  return { width, height, cells }
}

/** A copy of `map` whose grid, legend and tiles describe `grid`. Other keys are untouched. */
export function writeGrid(map: MapFile, grid: Grid, terrain: Terrain[]): MapFile {
  const legend = assignLegend([...new Set(grid.cells)], legendOf(map), terrain)
  const ch = new Map(Object.entries(legend).map(([c, id]) => [id, c]))
  const tiles: string[] = []
  for (let y = 0; y < grid.height; y++) {
    let row = ''
    for (let x = 0; x < grid.width; x++) row += ch.get(grid.cells[y * grid.width + x])
    tiles.push(row)
  }
  return { ...map, grid: { ...map.grid, width: grid.width, height: grid.height }, legend, tiles }
}

/** Fixes whatever `gridProblems` found: pads/crops rows, unknown symbols become the first terrain. */
export function repairMap(map: MapFile, terrain: Terrain[]): MapFile {
  const g = readGrid(map, terrain)
  const fixed = writeGrid(map, g, terrain)
  if (map.heights === undefined) return fixed
  const old = Array.isArray(map.heights) ? map.heights : []
  fixed.heights = Array.from({ length: g.height }, (_, y) => Array.from({ length: g.width }, (_, x) => {
    const v = (old[y] as unknown[] | undefined)?.[x]
    return typeof v === 'number' && Number.isInteger(v) ? v : 0
  }))
  return fixed
}

/** Terrain id at a cell of a consistent map; undefined outside or for unknown symbols. */
export const terrainAt = (map: MapFile, x: number, y: number): string | undefined => map.legend[map.tiles[y]?.[x] ?? '']

export const inBounds = (map: MapFile, x: number, y: number): boolean =>
  Number.isInteger(x) && Number.isInteger(y) && x >= 0 && y >= 0 && x < map.grid.width && y < map.grid.height
