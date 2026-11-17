// The drawing surface. It redraws only on animation frames, and only visible squares.
import { useCallback, useEffect, useMemo, useRef } from 'react'
import { clockNow, useAnimating, useAnimationClock, useReducedMotion } from '../../../art/useArt'
import type { ArtScene, StyleView } from '../../../art/scene'
import { ctxOfParts } from '../../../model/mapCtx'
import { fitProblem, indexByOrigin, placedOf, structureIndexAt } from '../../../model/mapStructures'
import type { MapFile, Structure, Terrain, Unit } from '../../../model/types'
import type { StructureGhost } from './drawStructures'
import { drawMap, type Scene } from './drawMap'
import { useCanvasInput, type InputProps } from './useCanvasInput'
import { T } from './text'

interface Props extends Omit<InputProps, 'size'> {
  map: MapFile
  terrain: Terrain[]
  unitTypes: Unit[]
  structureTypes: Structure[]
  flagged: Set<string>
  art: ArtScene | null
  style: StyleView
  /** The Heights tool is active: squares not at ground level show their number. */
  showHeights: boolean
}

export function MapCanvas({ map, terrain, unitTypes, structureTypes, flagged, art, style, showHeights, ...input }: Props) {
  const { vs, geo } = input
  const wrap = useRef<HTMLDivElement>(null)
  const cv = useRef<HTMLCanvasElement>(null)
  const dpr = useRef(1)
  const raf = useRef(0)
  const reducedMotion = useReducedMotion()
  const reduced = useRef(reducedMotion)
  reduced.current = reducedMotion
  const unitsW = geo.uw, unitsH = geo.uh

  const scene = useMemo<Scene>(() => ({
    map,
    terrain: new Map(terrain.map((t) => [t.id, t])),
    types: new Map(unitTypes.map((u) => [u.id, u])),
    structures: new Map(structureTypes.map((s) => [s.id, s])),
    flagged, art, style, showHeights, grid: geo.grid, geo,
  }), [map, terrain, unitTypes, structureTypes, flagged, art, style, showHeights, geo])
  const sceneRef = useRef(scene)
  sceneRef.current = scene

  // Where a structure would go (placing with the pointer or keyboard cursor, or dragging one), and whether it fits.
  const ctx = useMemo(() => ctxOfParts(terrain, structureTypes, unitTypes, geo.grid), [terrain, structureTypes, unitTypes, geo.grid])
  const ghostOf = useCallback((): StructureGhost | null => {
    const m = sceneRef.current.map
    if (vs.dragS) {
      const i = indexByOrigin(m, vs.dragS.origin.x, vs.dragS.origin.y)
      const s = placedOf(m)[i]
      if (!s) return null
      const x = s.x + vs.dragS.dx, y = s.y + vs.dragS.dy
      return { type: s.type, x, y, side: s.side, ok: fitProblem(m, ctx, s.type, x, y, i) === null }
    }
    const at = vs.placing ? vs.hover ?? (vs.cursorVisible ? vs.cursor : null) : null
    if (!vs.placing || !at || structureIndexAt(m, ctx, at.x, at.y) >= 0) return null
    return { type: vs.placing.type, x: at.x, y: at.y, side: vs.placing.side || undefined, ok: fitProblem(m, ctx, vs.placing.type, at.x, at.y) === null }
  }, [vs, ctx])
  const ghostRef = useRef(ghostOf)
  ghostRef.current = ghostOf

  const draw = useCallback(() => {
    raf.current = 0
    const c = cv.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    const sg = ghostRef.current()
    drawMap(g, sceneRef.current, { dpr: dpr.current, w: vs.size.w, h: vs.size.h, view: vs.view, hover: vs.hover, cursor: vs.cursorVisible ? vs.cursor : null, selected: vs.selected, ghost: vs.ghost, structureGhost: sg, selSize: vs.selSize, selCells: vs.selCells, time: clockNow(reduced.current), overlays: vs.overlays, slots: vs.changeOutline.length ? [...vs.slots, ...vs.changeOutline] : vs.slots, pickRect: vs.pickRect, fogSeen: vs.fogSeen })
    // What the structure ghost shows (for tests and curious people): "kind,x,y,ok|bad", empty when there is none.
    c.dataset.ghost = sg ? `${sg.type},${sg.x},${sg.y},${sg.ok ? 'ok' : 'bad'}` : ''
    // Lets tests (and curious people) find a square on screen without guessing.
    c.dataset.view = `${vs.view.scale},${vs.view.x},${vs.view.y}`
    // What the "Show what a side sees" aid shows, for tests and curious people: one digit per square, row by row (1 = seen, 0 = dimmed).
    c.dataset.fog = vs.fogSeen ? Array.from(vs.fogSeen).join('') : ''
    // The squares of the open event / objectives ("tone:x,y,w,h;..."), for tests and curious people.
    c.dataset.areas = vs.overlays.map((o) => `${o.tone}:${o.x},${o.y},${o.w},${o.h}`).join(';')
    // The preparation's deployment squares in slot order ("x,y;x,y"), for tests and curious people.
    c.dataset.slots = vs.slots.map((o) => `${o.x},${o.y}`).join(';')
    // The Changes tab: the squares of the selected change ("x,y;x,y") and whether the canvas shows the map before or after it.
    c.dataset.changeCells = vs.changeOutline.map((o) => `${o.x},${o.y}`).join(';')
    c.dataset.changeView = vs.changeSel ? vs.changeView : ''
  }, [vs])
  const schedule = useCallback(() => { if (!raf.current) raf.current = requestAnimationFrame(draw) }, [draw])

  // Size the backing store to the element x devicePixelRatio so lines stay one device pixel wide.
  useEffect(() => {
    const el = wrap.current!
    const measure = () => {
      const r = el.getBoundingClientRect()
      dpr.current = window.devicePixelRatio || 1
      const c = cv.current!
      c.width = Math.max(1, Math.round(r.width * dpr.current)); c.height = Math.max(1, Math.round(r.height * dpr.current))
      vs.set({ size: { w: r.width, h: r.height } })
      if (vs.autoFit) vs.fit(sceneRef.current.geo!.uw, sceneRef.current.geo!.uh)
      schedule()
    }
    const ro = new ResizeObserver(measure)
    ro.observe(el)
    measure()
    return () => { ro.disconnect(); if (raf.current) cancelAnimationFrame(raf.current) }
  }, [vs, schedule])

  useEffect(() => vs.subscribe(schedule), [vs, schedule])
  // Animated pictures: redraw ~20x a second, only while the tab is in front and motion is allowed.
  useAnimationClock(useAnimating(!!art?.animated), schedule)
  useEffect(schedule, [scene, schedule])
  // Motion switched off (or on) while the map is open: draw once more, so an animated picture goes back to its first
  // frame instead of staying on whichever frame the clock stopped at.
  useEffect(schedule, [reducedMotion, schedule])
  // A resized map is shown whole again.
  useEffect(() => { vs.fit(unitsW, unitsH) }, [vs, unitsW, unitsH])

  useCanvasInput(cv, { ...input, size: () => vs.size })

  return (
    <div className="map-canvas-wrap" ref={wrap}>
      <canvas ref={cv} className="map-canvas" tabIndex={0} role="application" aria-label={T.canvasLabel} aria-roledescription="map grid" data-testid="map-canvas" />
    </div>
  )
}
