// Who owns a building at the start of a battle.
import type { Cell } from './mapGrid'
import type { MapFile, Terrain } from './types'

export const ownerAt = (map: MapFile, x: number, y: number): string | undefined =>
  map.owners?.find((o) => o.x === x && o.y === y)?.side

/** Sets (or, with null, clears) the owner of one or more squares. */
export function setOwners(map: MapFile, cells: Cell[], side: string | null): MapFile {
  const at = new Set(cells.map((c) => `${c.x},${c.y}`))
  const old = map.owners ?? []
  if (side !== null && !map.sides?.some((s) => s.id === side)) return map
  const kept = old.filter((o) => !at.has(`${o.x},${o.y}`))
  const added = side === null ? [] : cells
    .filter((c, i) => c.x >= 0 && c.y >= 0 && c.x < map.grid.width && c.y < map.grid.height && cells.findIndex((d) => d.x === c.x && d.y === c.y) === i)
    .map((c) => {
      const prev = old.find((o) => o.x === c.x && o.y === c.y)
      return prev ? { ...prev, side } : { x: c.x, y: c.y, side }
    })
  const unchanged = kept.length + added.length === old.length && added.every((a) => old.some((o) => o.x === a.x && o.y === a.y && o.side === a.side))
  return unchanged ? map : { ...map, owners: [...kept, ...added] }
}

/** True when owning this terrain changes anything in a battle (capturable, earns money, builds units, or is a headquarters). */
export const ownershipMatters = (t: Terrain | undefined): boolean =>
  !!t && (!!t.capture || !!t.produces?.length || !!(t.income && Object.values(t.income).some((v) => v)) || !!t.tags?.includes('hq'))
