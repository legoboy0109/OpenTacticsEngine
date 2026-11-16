// Coordinates and contents of the square under the pointer (or the keyboard cursor).
import { terrainAt } from '../../../model/mapGrid'
import { sideOf } from '../../../model/mapSides'
import { gridOfGame } from '../../../model/gridSpec'
import { structureAt } from '../../../model/mapStructures'
import { unitAt } from '../../../model/mapUnits'
import type { MapFile, Structure, Terrain, Unit } from '../../../model/types'
import { useDoc } from '../../../store/context'
import { useCellWords } from '../../../ui/cellWords'
import { cellKey } from './geometry'
import { T } from './text'
import { useViewSlice, type MapViewState } from './viewState'

interface Props { vs: MapViewState; map: MapFile; terrain: Terrain[]; structureTypes: Structure[]; unitTypes: Unit[]; notice: string | null }

export function StatusLine({ vs, map, terrain, structureTypes, unitTypes, notice }: Props) {
  const words = useCellWords()
  const grid = gridOfGame(useDoc().game)
  const hover = useViewSlice(vs, (s) => cellKey(s.hover))
  const cursor = useViewSlice(vs, (s) => (s.cursorVisible ? cellKey(s.cursor) : null))
  const key = hover ?? cursor
  let text: string = words(T.statusNothing)
  if (key) {
    const [x, y] = key.split(',').map(Number)
    const parts = [T.statusSquare(x, y), terrain.find((t) => t.id === terrainAt(map, x, y))?.name ?? '?']
    const st = structureAt(map, { structureTypes, grid }, x, y)
    if (st) {
      parts.push(T.statusStructure(structureTypes.find((k) => k.id === st.type)?.name ?? st.type))
      if (st.side) parts.push(T.statusOwner(sideOf(map, st.side)?.name ?? st.side))
    }
    const u = unitAt(map, x, y)
    if (u) parts.push(T.statusUnit(unitTypes.find((t) => t.id === u.type)?.name ?? u.type ?? '', sideOf(map, u.side)?.name ?? u.side, u.name))
    text = parts.join(' · ')
  }
  return (
    <div className="map-status">
      <span className={`map-status-cell${key ? '' : ' idle'}`} data-testid="map-status">{text}</span>
      <span className="map-notice" role="status" aria-live="polite" data-testid="map-notice">{notice}</span>
    </div>
  )
}
