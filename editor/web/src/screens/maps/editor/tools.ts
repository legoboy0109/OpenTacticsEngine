// The four map tools and what one drag (a "stroke") does with each.

import { ctxOf, editMap } from '../../../model/mapCtx'
import { terrainAt, type Cell } from '../../../model/mapGrid'
import { fillFrom, lineCells, paintCells } from '../../../model/mapPaint'
import { applyHeights, clampHeight, type HeightMode } from '../../../model/mapHeights'
import { moveStructureBy, placeStructure, rectOf, structureIndexAt, placedOf } from '../../../model/mapStructures'
import { moveMapUnit, placeUnit, unitAt } from '../../../model/mapUnits'
import { STROKE_PREFIX } from '../../../store/history'
import type { DocumentStore } from '../../../store/documentStore'
import type { MapViewState } from './viewState'
import { T } from './text'

export type Tool = 'terrain' | 'units' | 'structures' | 'heights' | 'select'
export const TOOLS: Tool[] = ['terrain', 'units', 'structures', 'heights', 'select']
/** O stays the key of the old Owners tool, which the Structures tool replaced. */
export const TOOL_KEY: Record<Tool, string> = { terrain: 't', units: 'u', structures: 'o', heights: 'h', select: 's' }

export interface ToolState {
  tool: Tool
  terrainId: string
  fill: boolean
  unitType: string
  unitSide: string
  structureType: string
  /** Owner of newly placed structures; '' = nobody. */
  structureSide: string
  /** The Heights tool: what a click or drag does, and the number for "Set to". */
  heightMode: HeightMode
  heightValue: number
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
  if (t.tool === 'structures') return structureStroke(env, cell, t)
  if (t.tool === 'heights') return heightStroke(env, cell, t)
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

/** Raises, lowers or sets the squares the pointer passes over; each square changes once per drag and the drag is one undo step. */
function heightStroke(env: StrokeEnv, first: Cell, t: ToolState): Stroke {
  const k = `${STROKE_PREFIX}${++strokeCounter}`
  const action = t.heightMode === 'set' ? { kind: 'set' as const, value: clampHeight(t.heightValue) } : { kind: t.heightMode }
  const done = new Set<string>()
  const apply = (cells: Cell[]) => {
    const fresh = cells.filter((c) => !done.has(`${c.x},${c.y}`))
    fresh.forEach((c) => done.add(`${c.x},${c.y}`))
    if (fresh.length) env.store.edit((d) => editMap(d, env.mapId, (m) => applyHeights(m, fresh, action)), k)
  }
  let last = first
  apply([first])
  return { move(c) { apply(lineCells(last, c)); last = c }, end() {} }
}

/** Click an empty square: place the chosen structure with its top-left corner there. Click a placed one: select it;
 *  dragging it moves it (the pointer's offset inside the footprint is kept). */
function structureStroke(env: StrokeEnv, cell: Cell, t: ToolState): Stroke {
  const doc = env.store.getState().doc
  const ctx = ctxOf(doc)
  const map = currentMap(env)!
  if (!ctx) return NOOP
  const i = structureIndexAt(map, ctx, cell.x, cell.y)
  if (i >= 0) {
    const s = placedOf(map)[i]
    const size = rectOf(ctx.structureTypes.find((d) => d.id === s.type), s.x, s.y)
    env.view.set({ selected: { x: s.x, y: s.y }, selSize: { w: size.w, h: size.h } })
    return dragStructure(env, { x: s.x, y: s.y }, cell)
  }
  if (!t.structureType) { env.say(T.noStructureKinds); return NOOP }
  const spec = { type: t.structureType, side: t.structureSide, x: cell.x, y: cell.y }
  const r = placeStructure(map, ctx, spec)
  if (r.error) { env.say(r.error); return NOOP }
  env.store.edit((d) => editMap(d, env.mapId, (m, c) => placeStructure(m, c, spec).map))
  const size = rectOf(ctx.structureTypes.find((d) => d.id === t.structureType), cell.x, cell.y)
  env.view.set({ selected: cell, selSize: { w: size.w, h: size.h } })
  return NOOP
}

/** Drag a placed structure: its footprint follows the pointer (red where it cannot go); the move happens once, on release. */
function dragStructure(env: StrokeEnv, origin: Cell, grab: Cell): Stroke {
  const delta = (c: Cell) => ({ dx: c.x - grab.x, dy: c.y - grab.y })
  return {
    move(c) { env.view.set({ dragS: { origin, ...delta(c) } }) },
    end(c) {
      env.view.set({ dragS: null })
      if (!c) return
      const { dx, dy } = delta(c)
      if (dx === 0 && dy === 0) return
      const ctx = ctxOf(env.store.getState().doc)
      const map = currentMap(env)
      if (!ctx || !map) return
      const r = moveStructureBy(map, ctx, origin, dx, dy)
      if (r.error) { env.say(r.error); return }
      env.store.edit((d) => editMap(d, env.mapId, (m, x) => moveStructureBy(m, x, origin, dx, dy).map))
      env.view.set({ selected: { x: origin.x + dx, y: origin.y + dy }, selSize: env.view.selSize })
    },
  }
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
