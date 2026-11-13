// The drawing surface. It redraws only on animation frames, and only visible squares.
import { useCallback, useEffect, useMemo, useRef } from 'react'
import { clockNow, useAnimating, useAnimationClock, useReducedMotion } from '../../../art/useArt'
import type { ArtScene, StyleView } from '../../../art/scene'
import type { MapFile, Terrain, Unit } from '../../../model/types'
import { drawMap, type Scene } from './drawMap'
import { useCanvasInput, type InputProps } from './useCanvasInput'
import { T } from './text'

interface Props extends Omit<InputProps, 'mapSize' | 'size'> {
  map: MapFile
  terrain: Terrain[]
  unitTypes: Unit[]
  flagged: Set<string>
  art: ArtScene | null
  style: StyleView
}

export function MapCanvas({ map, terrain, unitTypes, flagged, art, style, ...input }: Props) {
  const { vs } = input
  const wrap = useRef<HTMLDivElement>(null)
  const cv = useRef<HTMLCanvasElement>(null)
  const dpr = useRef(1)
  const raf = useRef(0)
  const reducedMotion = useReducedMotion()
  const reduced = useRef(reducedMotion)
  reduced.current = reducedMotion
  const mapW = map.grid.width, mapH = map.grid.height

  const scene = useMemo<Scene>(() => ({
    map,
    terrain: new Map(terrain.map((t) => [t.id, t])),
    types: new Map(unitTypes.map((u) => [u.id, u])),
    flagged, art, style,
  }), [map, terrain, unitTypes, flagged, art, style])
  const sceneRef = useRef(scene)
  sceneRef.current = scene

  const draw = useCallback(() => {
    raf.current = 0
    const c = cv.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    drawMap(g, sceneRef.current, { dpr: dpr.current, w: vs.size.w, h: vs.size.h, view: vs.view, hover: vs.hover, cursor: vs.cursorVisible ? vs.cursor : null, selected: vs.selected, ghost: vs.ghost, time: clockNow(reduced.current) })
    // Lets tests (and curious people) find a square on screen without guessing.
    c.dataset.view = `${vs.view.scale},${vs.view.x},${vs.view.y}`
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
      if (vs.autoFit) vs.fit(sceneRef.current.map.grid.width, sceneRef.current.map.grid.height)
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
  // A resized map is shown whole again.
  useEffect(() => { vs.fit(mapW, mapH) }, [vs, mapW, mapH])

  useCanvasInput(cv, { ...input, mapSize: { w: mapW, h: mapH }, size: () => vs.size })

  return (
    <div className="map-canvas-wrap" ref={wrap}>
      <canvas ref={cv} className="map-canvas" tabIndex={0} role="application" aria-label={T.canvasLabel} aria-roledescription="map grid" data-testid="map-canvas" />
    </div>
  )
}
