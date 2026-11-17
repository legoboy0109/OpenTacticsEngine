// What the map drawing code is given (shared by the square and the hexagon drawing).
import type { ArtScene, StyleView } from '../../../art/scene'
import type { GridSpec } from '../../../model/gridSpec'
import type { Geo } from '../../../model/mapGeo'
import type { Cell } from '../../../model/mapGrid'
import type { Overlay } from '../../../model/storyOverlays'
import type { Rect } from '../../../model/storyTypes'
import type { MapFile, Structure, Terrain, Unit } from '../../../model/types'
import type { StructureGhost } from './drawStructures'
import type { View } from './geometry'

export interface Scene {
  map: MapFile
  terrain: Map<string, Terrain>
  types: Map<string, Unit>
  /** The game's structure kinds. */
  structures: Map<string, Structure>
  /** "x,y" of units standing where they cannot be. */
  flagged: Set<string>
  /** Pictures (null = the game has none: everything keeps the placeholder look). */
  art?: ArtScene | null
  /** The visual style being previewed: background, terrain colours, letters on/off. */
  style?: StyleView
  /** Squares whose height is not 0 get a tint and their number (the Heights tool). */
  showHeights?: boolean
  /** The game's grid (missing = squares) and where cells are on the picture (model/mapGeo.ts; missing = squares). */
  grid?: GridSpec
  geo?: Geo
}

export interface DrawState {
  dpr: number
  w: number
  h: number
  view: View
  hover: Cell | null
  cursor: Cell | null
  selected: Cell | null
  ghost: { from: Cell; to: Cell } | null
  /** A structure about to be placed or being dragged (red where it cannot go). */
  structureGhost?: StructureGhost | null
  /** The selected structure's footprint (the selected square otherwise); squares only. */
  selSize?: { w: number; h: number } | null
  /** Hexagon maps: every hexagon of the selected structure (the selected hexagon otherwise). */
  selCells?: Cell[] | null
  /** Running time in ms for animated pictures (0 under reduced motion). */
  time?: number
  /** Areas / squares of the open event or the objectives, and the rectangle being picked. */
  overlays?: Overlay[]
  /** The preparation's deployment squares, numbered (model/prep.ts slotOverlays); drawn under the event areas. */
  slots?: Overlay[]
  pickRect?: Rect | null
  /** Squares with 0 are dimmed (what a side does not see). */
  fogSeen?: Uint8Array | null
}
