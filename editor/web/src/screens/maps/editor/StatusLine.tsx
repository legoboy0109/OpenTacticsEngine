// Coordinates and contents of the square under the pointer (or the keyboard cursor).
import { terrainAt } from '../../../model/mapGrid'
import { ownerAt } from '../../../model/mapOwners'
import { sideOf } from '../../../model/mapSides'
import { unitAt } from '../../../model/mapUnits'
import type { MapFile, Terrain, Unit } from '../../../model/types'
import { cellKey } from './geometry'
import { T } from './text'
import { useViewSlice, type MapViewState } from './viewState'

interface Props { vs: MapViewState; map: MapFile; terrain: Terrain[]; unitTypes: Unit[]; notice: string | null }

export function StatusLine({ vs, map, terrain, unitTypes, notice }: Props) {
  const hover = useViewSlice(vs, (s) => cellKey(s.hover))
  const cursor = useViewSlice(vs, (s) => (s.cursorVisible ? cellKey(s.cursor) : null))
  const key = hover ?? cursor
  let text: string = T.statusNothing
  if (key) {
    const [x, y] = key.split(',').map(Number)
    const parts = [T.statusSquare(x, y), terrain.find((t) => t.id === terrainAt(map, x, y))?.name ?? '?']
    const owner = ownerAt(map, x, y)
    if (owner) parts.push(T.statusOwner(sideOf(map, owner)?.name ?? owner))
    const u = unitAt(map, x, y)
    if (u) parts.push(T.statusUnit(unitTypes.find((t) => t.id === u.type)?.name ?? u.type, sideOf(map, u.side)?.name ?? u.side, u.name))
    text = parts.join(' · ')
  }
  return (
    <div className="map-status">
      <span className="map-status-cell" data-testid="map-status">{text}</span>
      <span className="map-notice" role="status" aria-live="polite" data-testid="map-notice">{notice}</span>
    </div>
  )
}
