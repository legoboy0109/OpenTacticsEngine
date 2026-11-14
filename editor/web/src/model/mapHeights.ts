// Heights of a map's squares (docs/GAME_FORMAT.md "heights"): whole numbers, one per square, 0 = ground level. The editor
// keeps them within HEIGHT_MIN..HEIGHT_MAX. A map whose squares are all 0 has no "heights" key at all.
import type { Cell } from './mapGrid'
import type { MapFile } from './types'

export const HEIGHT_MIN = -8
export const HEIGHT_MAX = 32

export type HeightAction = { kind: 'raise' } | { kind: 'lower' } | { kind: 'set'; value: number }
export type HeightMode = HeightAction['kind']

export const clampHeight = (n: number): number => Math.min(HEIGHT_MAX, Math.max(HEIGHT_MIN, Math.round(Number.isFinite(n) ? n : 0)))

/** The height of a square (0 where the map has none). */
export function heightAt(map: MapFile, x: number, y: number): number {
  const v = map.heights?.[y]?.[x]
  return typeof v === 'number' && Number.isInteger(v) ? v : 0
}

const next = (cur: number, a: HeightAction): number => clampHeight(a.kind === 'raise' ? cur + 1 : a.kind === 'lower' ? cur - 1 : a.value)

/** Applies the action to every given square once (a square listed twice changes once). Returns the same map when nothing
 *  changed, so a stroke over squares already at the limit adds no undo step. */
export function applyHeights(map: MapFile, cells: Cell[], action: HeightAction): MapFile {
  const w = map.grid.width, h = map.grid.height
  const rows = Array.from({ length: h }, (_, y) => Array.from({ length: w }, (_, x) => heightAt(map, x, y)))
  const seen = new Set<number>()
  let changed = false
  for (const { x, y } of cells) {
    if (x < 0 || y < 0 || x >= w || y >= h || seen.has(y * w + x)) continue
    seen.add(y * w + x)
    const v = next(rows[y][x], action)
    if (v !== rows[y][x]) { rows[y][x] = v; changed = true }
  }
  if (!changed) return map
  const out: MapFile = { ...map }
  if (rows.some((r) => r.some((v) => v !== 0))) out.heights = rows
  else delete out.heights
  return out
}
