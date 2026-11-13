// The four map tools and what one drag (a "stroke") does with each.
import { ctxOf, editMap } from '../../../model/mapCtx'
import { terrainAt, type Cell } from '../../../model/mapGrid'
import { ownershipMatters, setOwners } from '../../../model/mapOwners'
import { fillFrom, lineCells, paintCells } from '../../../model/mapPaint'
import { moveMapUnit, placeUnit, unitAt } from '../../../model/mapUnits'
import { STROKE_PREFIX } from '../../../store/history'
import type { DocumentStore } from '../../../store/documentStore'
import type { MapViewState } from './viewState'
import { T } from './text'

export type Tool = 'terrain' | 'units' | 'owners' | 'select'
export const TOOLS: Tool[] = ['terrain', 'units', 'owners', 'select']
export const TOOL_KEY: Record<Tool, string> = { terrain: 't', units: 'u', owners: 'o', select: 's' }

export interface ToolState {
  tool: Tool
  terrainId: string
  fill: boolean
  unitType: string
  unitSide: string
  /** '' = neutral (no owner). */
  ownerSide: string
}

/** One drag. `end` gets the square the pointer was released on (null if outside the map). */
export interface Stroke { move(c: Cell): void; end(c: Cell | null): void }
const NOOP: Stroke = { move() {}, end() {} }

export interface StrokeEnv {
  store: DocumentStore
  mapId: string
  view: MapViewState
  tools: () => ToolState
  pickTerrain: (id: string) => void
  say: (msg: string | null) => void
}

let strokeCounter = 0
const currentMap = (env: StrokeEnv) => env.store.getState().doc.maps[env.mapId] ?? null

/** Starts the active tool at `cell`; null when there is nothing to do. */
export function beginStroke(env: StrokeEnv, cell: Cell, opts: { alt?: boolean } = {}): Stroke {
  const t = env.tools()
  const map = currentMap(env)
  if (!map) return NOOP
  env.say(null)
  if (t.tool === 'terrain') {
    if (opts.alt) {
      const id = terrainAt(map, cell.x, cell.y)
      if (id) env.pickTerrain(id)
      return NOOP
    }
    if (t.fill) {
      env.store.edit((d) => editMap(d, env.mapId, (m, c) => fillFrom(m, c.terrain, cell.x, cell.y, t.terrainId)))
      return NOOP
    }
    return paintStroke(cell, (cells, k) => env.store.edit((d) => editMap(d, env.mapId, (m, c) => paintCells(m, c.terrain, cells, t.terrainId)), k))
  }
  if (t.tool === 'owners') return ownerStroke(env, cell, t)
  env.view.set({ selected: cell })
  const unit = unitAt(map, cell.x, cell.y)
  if (t.tool === 'select') return NOOP
  return unit ? dragUnit(env, cell) : placeAt(env, cell, t)
}

/** Paints along the pointer's path; every square of one drag shares one undo step (one coalescing key). */
function paintStroke(first: Cell, apply: (cells: Cell[], key: string) => void): Stroke {
  const k = `${STROKE_PREFIX}${++strokeCounter}`
  let last = first
  apply([first], k)
  return { move(c) { apply(lineCells(last, c), k); last = c }, end() {} }
}

function ownerStroke(env: StrokeEnv, first: Cell, t: ToolState): Stroke {
  const map = currentMap(env)!
  const ctx = ctxOf(env.store.getState().doc)
  const def = ctx?.terrain.find((x) => x.id === terrainAt(map, first.x, first.y))
  if (t.ownerSide && !ownershipMatters(def)) env.say(T.ownerNoEffect(def?.name ?? 'This terrain'))
  return paintStroke(first, (cells, k) => env.store.edit((d) => editMap(d, env.mapId, (m) => setOwners(m, cells, t.ownerSide || null)), k))
}

function placeAt(env: StrokeEnv, cell: Cell, t: ToolState): Stroke {
  const doc = env.store.getState().doc
  const ctx = ctxOf(doc)
  const map = currentMap(env)!
  if (!ctx) return NOOP
  const r = placeUnit(map, ctx, { type: t.unitType, side: t.unitSide, x: cell.x, y: cell.y })
  if (r.error) { env.say(r.error); return NOOP }
  env.store.edit((d) => editMap(d, env.mapId, (m, c) => placeUnit(m, c, { type: t.unitType, side: t.unitSide, x: cell.x, y: cell.y }).map))
  return NOOP
}

/** Drag a placed unit: a ghost follows the pointer, the move happens once on release. */
function dragUnit(env: StrokeEnv, from: Cell): Stroke {
  return {
    move(c) { env.view.set({ ghost: { from, to: c } }) },
    end(c) {
      env.view.set({ ghost: null })
      if (!c || (c.x === from.x && c.y === from.y)) return
      const ctx = ctxOf(env.store.getState().doc)
      const map = currentMap(env)
      if (!ctx || !map) return
      const r = moveMapUnit(map, ctx, from, c)
      if (r.error) { env.say(r.error); return }
      env.store.edit((d) => editMap(d, env.mapId, (m, x) => moveMapUnit(m, x, from, c).map))
      env.view.set({ selected: c })
    },
  }
}
