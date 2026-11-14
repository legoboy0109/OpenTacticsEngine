// The sides (players) of one map.
import type { MapCtx } from './mapCtx'
import { makeId } from './ids'
import { patchObj, setNum } from './util'
import type { MapFile, MapSide } from './types'

const COLORS = ['#3d85c6', '#cc0000', '#6aa84f', '#e0a800', '#8e5fc0', '#e67e22', '#2aa7a7', '#c2578a']
const NAMES = ['Blue', 'Red', 'Green', 'Yellow', 'Purple', 'Orange', 'Teal', 'Pink']

export const MIN_SIDES = 2
export const sideOf = (map: MapFile, id: string): MapSide | undefined => map.sides?.find((s) => s.id === id)
export const sideColor = (map: MapFile, id: string): string => sideOf(map, id)?.color || '#888888'

/** A new computer-played side with a name and colour not used yet. */
export function addSide(map: MapFile): MapFile {
  const sides = map.sides ?? []
  const used = new Set(sides.map((s) => s.name))
  const taken = new Set(sides.map((s) => s.id))
  const name = NAMES.find((n) => !used.has(n) && !taken.has(n.toLowerCase())) ?? `Side ${sides.length + 1}`
  const color = COLORS.find((c) => !sides.some((s) => s.color?.toLowerCase() === c)) ?? COLORS[sides.length % COLORS.length]
  const id = makeId(name, sides.map((s) => s.id), { fallback: 'side' })
  return { ...map, sides: [...sides, { id, name, color, controller: 'ai' }] }
}

export const canRemoveSide = (map: MapFile): boolean => (map.sides?.length ?? 0) > MIN_SIDES

/** What removing a side takes with it: its units go; its structures stay and become nobody's. */
export function sideRemovalLoss(map: MapFile, id: string): { units: number; structures: number } {
  return { units: (map.units ?? []).filter((u) => u.side === id).length, structures: (map.structures ?? []).filter((o) => o.side === id).length }
}

export function removeSide(map: MapFile, id: string): MapFile {
  if (!canRemoveSide(map) || !sideOf(map, id)) return map
  const next: MapFile = { ...map, sides: map.sides!.filter((s) => s.id !== id) }
  if (map.units) next.units = map.units.filter((u) => u.side !== id)
  if (map.structures?.some((o) => o.side === id)) next.structures = map.structures.map((o) => (o.side === id ? patchObj(o, { side: undefined }) : o))
  return next
}

type SidePatch = { name?: string; color?: string; controller?: 'human' | 'ai'; team?: string }

/** The side's id never changes (units and owners point at it). An empty team removes the key (the side is then its own team). */
export function updateSide(map: MapFile, id: string, patch: SidePatch): MapFile {
  if (!sideOf(map, id)) return map
  const clean: Record<string, unknown> = { ...patch }
  if ('team' in patch && !patch.team?.trim()) clean.team = undefined
  return { ...map, sides: map.sides!.map((s) => (s.id === id ? patchObj(s, clean) : s)) }
}

/** Starting amount of one resource; only resources the game defines are accepted. */
export function setSideResource(map: MapFile, c: MapCtx, id: string, resource: string, value: number | undefined): MapFile {
  if (!c.resources.includes(resource) || !sideOf(map, id)) return map
  return { ...map, sides: map.sides!.map((s) => (s.id === id ? patchObj(s, { resources: setNum(s.resources, resource, value) }) : s)) }
}

export const setMapName = (map: MapFile, name: string): MapFile => (map.name === name ? map : { ...map, name })
