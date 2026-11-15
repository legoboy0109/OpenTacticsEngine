// Pointer, touch, wheel and keyboard handling for the map canvas. It only knows
// about squares and strokes; what a stroke does is decided by tools.ts.
import { useEffect, useRef, type RefObject } from 'react'
import type { Geo } from '../../../model/mapGeo'
import type { Cell } from '../../../model/mapGrid'
import { cellAt, clampPan, reveal, zoomAt, type View } from './geometry'
import type { Stroke } from './tools'
import type { MapViewState } from './viewState'

export interface InputProps {
  vs: MapViewState
  /** Where cells are on the picture: squares and hexagons answer the same questions (model/mapGeo.ts). */
  geo: Geo
  begin: (cell: Cell, opts: { alt: boolean }) => Stroke
  /** Touch: a long press on the terrain tool picks the terrain (returns whether it did). */
  longPress: (cell: Cell) => boolean
  /** Enter / Space on the keyboard cursor. */
  act: (cell: Cell) => void
  remove: () => void
  escape: () => void
  /** Size of the canvas in CSS pixels. */
  size: () => { w: number; h: number }
}

type Pt = { x: number; y: number }
const TAP_SLOP = 6
const LONG_PRESS_MS = 500

export function useCanvasInput(ref: RefObject<HTMLCanvasElement | null>, props: InputProps): void {
  const latest = useRef(props)
  latest.current = props

  useEffect(() => {
    const cv = ref.current!
    const ptrs = new Map<number, Pt>()
    let mode: 'none' | 'stroke' | 'pan' | 'gesture' | 'pending' = 'none'
    let stroke: Stroke | null = null
    let lastCell: Cell | null = null
    let panFrom: Pt = { x: 0, y: 0 }
    let panView: View = latest.current.vs.view
    let pending: { cell: Cell; at: Pt; timer: number } | null = null
    let gesture = { dist: 1, mid: { x: 0, y: 0 }, view: latest.current.vs.view }
    let spaceDown = false
    let hovering = false

    const local = (e: PointerEvent): Pt => { const r = cv.getBoundingClientRect(); return { x: e.clientX - r.left, y: e.clientY - r.top } }
    const cellOf = (p: Pt) => { const { vs, geo } = latest.current; return cellAt(vs.view, p.x, p.y, geo) }
    const setView = (v: View) => {
      const { vs, geo, size } = latest.current
      const s = size()
      vs.set({ view: clampPan(v, s.w, s.h, geo.uw, geo.uh), autoFit: false })
    }
    const cancelPending = () => { if (pending) { clearTimeout(pending.timer); pending = null } }
    const endStroke = (at: Cell | null) => { stroke?.end(at); stroke = null }
    const startGesture = () => {
      const [a, b] = [...ptrs.values()]
      gesture = { dist: Math.max(1, Math.hypot(a.x - b.x, a.y - b.y)), mid: { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }, view: latest.current.vs.view }
    }

    const down = (e: PointerEvent) => {
      const { vs, begin, longPress } = latest.current
      cv.focus({ preventScroll: true })
      try { cv.setPointerCapture(e.pointerId) } catch { /* pointer already gone */ }
      const p = local(e)
      ptrs.set(e.pointerId, p)
      if (ptrs.size >= 2) {
        cancelPending(); endStroke(lastCell); mode = 'gesture'; startGesture(); return
      }
      if (e.button === 1 || (e.button === 0 && spaceDown)) {
        e.preventDefault(); mode = 'pan'; panFrom = p; panView = vs.view; return
      }
      if (e.button !== 0) return
      vs.set({ cursorVisible: false })
      const cell = cellOf(p)
      if (!cell) return
      if (e.pointerType === 'mouse') {
        stroke = begin(cell, { alt: e.altKey }); lastCell = cell; mode = 'stroke'
        return
      }
      // Touch/pen: wait to see whether this is a tap, a drag, a long press or the start of a two-finger gesture.
      mode = 'pending'
      pending = { cell, at: p, timer: window.setTimeout(() => {
        if (pending && longPress(pending.cell)) { mode = 'none'; pending = null }
      }, LONG_PRESS_MS) }
    }

    const move = (e: PointerEvent) => {
      const { vs, begin } = latest.current
      const p = local(e)
      if (ptrs.has(e.pointerId)) ptrs.set(e.pointerId, p)
      if (e.pointerType === 'mouse') {
        const c = cellOf(p)
        if (c?.x !== vs.hover?.x || c?.y !== vs.hover?.y) vs.set({ hover: c })
      }
      if (mode === 'gesture' && ptrs.size >= 2) {
        const [a, b] = [...ptrs.values()]
        const mid = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }
        const zoomed = zoomAt(gesture.view, Math.hypot(a.x - b.x, a.y - b.y) / gesture.dist, gesture.mid.x, gesture.mid.y)
        setView({ ...zoomed, x: zoomed.x + mid.x - gesture.mid.x, y: zoomed.y + mid.y - gesture.mid.y })
      } else if (mode === 'pan') {
        setView({ ...panView, x: panView.x + p.x - panFrom.x, y: panView.y + p.y - panFrom.y })
      } else if (mode === 'pending' && pending) {
        if (Math.hypot(p.x - pending.at.x, p.y - pending.at.y) < TAP_SLOP) return
        const first = pending.cell
        cancelPending()
        stroke = begin(first, { alt: false }); lastCell = first; mode = 'stroke'
        const c = cellOf(p)
        if (c) { stroke.move(c); lastCell = c }
      } else if (mode === 'stroke') {
        const c = cellOf(p)
        if (c && (c.x !== lastCell?.x || c.y !== lastCell?.y)) { stroke?.move(c); lastCell = c }
      }
    }

    const up = (e: PointerEvent) => {
      const { begin } = latest.current
      const p = local(e)
      ptrs.delete(e.pointerId)
      if (mode === 'pending' && pending) {
        const cell = pending.cell
        cancelPending()
        begin(cell, { alt: false }).end(cell)
        mode = 'none'
      } else if (mode === 'stroke') {
        endStroke(cellOf(p) ?? lastCell); mode = 'none'
      } else if (mode === 'pan' || (mode === 'gesture' && ptrs.size < 2)) {
        mode = 'none'
      }
      if (ptrs.size === 0) mode = 'none'
    }

    const cancel = (e: PointerEvent) => { cancelPending(); if (mode === 'stroke') endStroke(null); ptrs.delete(e.pointerId); mode = 'none' }
    const leave = () => { hovering = false; if (latest.current.vs.hover) latest.current.vs.set({ hover: null }) }
    const enter = () => { hovering = true }

    const wheel = (e: WheelEvent) => {
      e.preventDefault()
      const r = cv.getBoundingClientRect()
      const k = Math.exp(-e.deltaY * (e.deltaMode === 1 ? 0.05 : 0.0015))
      setView(zoomAt(latest.current.vs.view, k, e.clientX - r.left, e.clientY - r.top))
    }

    const keydown = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey) return
      const { vs, geo, act, remove, escape, size } = latest.current
      const arrows: Record<string, [number, number]> = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }
      const d = arrows[e.key]
      if (d) {
        e.preventDefault()
        const from = vs.cursor ?? vs.selected ?? { x: 0, y: 0 }
        const step = e.shiftKey ? 5 : 1
        const c = { x: Math.min(geo.w - 1, Math.max(0, from.x + d[0] * step)), y: Math.min(geo.h - 1, Math.max(0, from.y + d[1] * step)) }
        const s = size()
        vs.set({ cursor: c, cursorVisible: true, view: vs.autoFit ? vs.view : reveal(vs.view, c, s.w, s.h, geo) })
      } else if (e.key === 'Enter' || (e.key === ' ' && !hovering)) {
        e.preventDefault()
        const c = vs.cursor
        if (c) act(c)
      } else if (e.key === ' ') {
        e.preventDefault(); spaceDown = true
      } else if (e.key === 'Delete' || e.key === 'Backspace') {
        e.preventDefault(); remove()
      } else if (e.key === 'Escape') {
        escape()
      }
    }
    const keyup = (e: KeyboardEvent) => { if (e.key === ' ') spaceDown = false }
    const focus = () => {
      const { vs } = latest.current
      if (cv.matches(':focus-visible')) vs.set({ cursor: vs.cursor ?? vs.selected ?? { x: 0, y: 0 }, cursorVisible: true })
    }
    const blur = () => { spaceDown = false }

    cv.addEventListener('pointerdown', down)
    cv.addEventListener('pointermove', move)
    cv.addEventListener('pointerup', up)
    cv.addEventListener('pointercancel', cancel)
    cv.addEventListener('pointerleave', leave)
    cv.addEventListener('pointerenter', enter)
    cv.addEventListener('wheel', wheel, { passive: false })
    cv.addEventListener('keydown', keydown)
    cv.addEventListener('keyup', keyup)
    cv.addEventListener('focus', focus)
    cv.addEventListener('blur', blur)
    return () => {
      cancelPending()
      for (const [t, f] of [['pointerdown', down], ['pointermove', move], ['pointerup', up], ['pointercancel', cancel], ['pointerleave', leave], ['pointerenter', enter], ['wheel', wheel], ['keydown', keydown], ['keyup', keyup], ['focus', focus], ['blur', blur]] as const) {
        cv.removeEventListener(t, f as EventListener)
      }
    }
  }, [ref])
}
