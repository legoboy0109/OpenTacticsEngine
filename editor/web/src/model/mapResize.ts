// Changing a map's size. The anchor is where the existing map stays.
import { MAP_MAX, readGrid, writeGrid } from './mapGrid'
import { footprint } from './mapStructures'
import { removeSlot } from './prep'
import type { GridSpec } from './gridSpec'
import type { MapFile, MapStructure, MapUnit, Structure, Terrain } from './types'

export type Anchor = 'nw' | 'n' | 'ne' | 'w' | 'c' | 'e' | 'sw' | 's' | 'se'
export const ANCHORS: Anchor[] = ['nw', 'n', 'ne', 'w', 'c', 'e', 'sw', 's', 'se']

/** How far the old top-left corner moves in the new grid. */
export function resizeOffset(oldW: number, oldH: number, newW: number, newH: number, anchor: Anchor): { dx: number; dy: number } {
  const dx = anchor.includes('w') ? 0 : anchor.includes('e') ? newW - oldW : Math.floor((newW - oldW) / 2)
  const dy = anchor.includes('n') ? 0 : anchor.includes('s') ? newH - oldH : Math.floor((newH - oldH) / 2)
  return { dx, dy }
}

export interface ResizeLoss { units: MapUnit[]; structures: MapStructure[]; /** Deployment slots (the preparation) outside, as their numbers from 1. */ slots: number[] }

export const validSize = (w: number, h: number): boolean =>
  Number.isInteger(w) && Number.isInteger(h) && w >= 1 && h >= 1 && w <= MAP_MAX && h <= MAP_MAX

/** What a resize would remove: units outside, and structures that no longer fit completely inside. */
/** `grid` = the game's grid (hexagon structures cover a middle hexagon and maybe its ring; squares when missing). */
export function resizeLoss(map: MapFile, structureTypes: Structure[], newW: number, newH: number, anchor: Anchor, grid?: GridSpec): ResizeLoss {
  const { dx, dy } = resizeOffset(map.grid.width, map.grid.height, newW, newH, anchor)
  const out = (x: number, y: number) => x + dx < 0 || y + dy < 0 || x + dx >= newW || y + dy >= newH
  return {
    units: (map.units ?? []).filter((u) => out(u.x, u.y)),
    slots: (map.prep?.slots ?? []).flatMap((s, i) => (out(s.x, s.y) ? [i + 1] : [])),
    // Squares: the footprint's two corners decide (a rectangle); hexagons: every covered hexagon must stay.
    structures: (map.structures ?? []).filter((s) => footprint(grid, structureTypes.find((t) => t.id === s.type), s.x, s.y).some((c) => out(c.x, c.y))),
  }
}

/** Resizes and removes what falls outside; new squares are the first terrain. Heights (when present) follow. */
export function resizeMap(map: MapFile, terrain: Terrain[], structureTypes: Structure[], newW: number, newH: number, anchor: Anchor, grid?: GridSpec): MapFile {
  if (!validSize(newW, newH) || (newW === map.grid.width && newH === map.grid.height)) return map
  const old = readGrid(map, terrain)
  const { dx, dy } = resizeOffset(old.width, old.height, newW, newH, anchor)
  const first = terrain[0]?.id ?? ''
  const cells: string[] = new Array(newW * newH)
  for (let y = 0; y < newH; y++) {
    for (let x = 0; x < newW; x++) {
      const ox = x - dx, oy = y - dy
      cells[y * newW + x] = ox >= 0 && oy >= 0 && ox < old.width && oy < old.height ? old.cells[oy * old.width + ox] : first
    }
  }
  const inside = (x: number, y: number) => x + dx >= 0 && y + dy >= 0 && x + dx < newW && y + dy < newH
  const next = writeGrid(map, { width: newW, height: newH, cells }, terrain)
  if (map.units) next.units = map.units.filter((u) => inside(u.x, u.y)).map((u) => ({ ...u, x: u.x + dx, y: u.y + dy }))
  if (map.structures) {
    const lost = new Set(resizeLoss(map, structureTypes, newW, newH, anchor, grid).structures)
    next.structures = map.structures.filter((s) => !lost.has(s)).map((s) => ({ ...s, x: s.x + dx, y: s.y + dy }))
  }
  if (map.prep) {  // the deployment slots that fall outside are removed (later numbers close up), the rest move with the map
    let p: MapFile = { ...next, prep: map.prep }
    map.prep.slots.map((s, i) => (inside(s.x, s.y) ? -1 : i)).filter((i) => i >= 0).reverse().forEach((i) => { p = removeSlot(p, i) })
    next.prep = { ...p.prep!, slots: p.prep!.slots.map((s) => ({ x: s.x + dx, y: s.y + dy })) }
  }
  if (map.heights) {
    next.heights = Array.from({ length: newH }, (_, y) => Array.from({ length: newW }, (_, x) => map.heights![y - dy]?.[x - dx] ?? 0))
  }
  return next
}
