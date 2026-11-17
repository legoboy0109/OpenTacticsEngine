// The timeline's view: where the picture is (x, y) and how big (k). It is moved straight on the element while a gesture runs and
// handed to React at most once a frame, so dragging does not redraw the boxes on every pointer event. The latest view is also
// written to the timeline's memory so coming back from an editor restores it.
import { useCallback, useEffect, useRef, useState } from 'react'
import type { Rect } from '../../../model/timelineTypes'
import type { TimelineMemory } from './memory'

export interface View { x: number; y: number; k: number }
export const MIN_K = 0.1
export const MAX_K = 2
export const clampK = (k: number): number => Math.min(MAX_K, Math.max(MIN_K, k))

export interface Size { w: number; h: number }

/** The view that shows the whole picture (never zoomed in past 100 %), centred. */
export function fitView(size: Size, content: { width: number; height: number }, pad = 12): View {
  const k = clampK(Math.min((size.w - 2 * pad) / content.width, (size.h - 2 * pad) / content.height, 1))
  return { k, x: (size.w - content.width * k) / 2, y: Math.max(pad, (size.h - content.height * k) / 2) }
}

/** The view moved just enough that `r` (world pixels) is inside, with some air around it. */
export function revealRect(v: View, size: Size, r: Rect, air = 40): View {
  let { x, y } = v
  const left = x + r.x * v.k, right = x + (r.x + r.w) * v.k, top = y + r.y * v.k, bottom = y + (r.y + r.h) * v.k
  if (right - left + 2 * air > size.w) x = air - r.x * v.k
  else if (left < air) x += air - left
  else if (right > size.w - air) x -= right - (size.w - air)
  if (bottom - top + 2 * air > size.h) y = air - r.y * v.k
  else if (top < air) y += air - top
  else if (bottom > size.h - air) y -= bottom - (size.h - air)
  return { ...v, x, y }
}

export function usePanZoom(mem: TimelineMemory) {
  const [view, setView] = useState<View>({ x: mem.x, y: mem.y, k: mem.k })
  const ref = useRef(view)
  const worldRef = useRef<HTMLDivElement>(null)
  const raf = useRef(0)
  const [size, setSize] = useState<Size>({ w: 0, h: 0 })
  const viewportRef = useRef<HTMLDivElement>(null)

  const set = useCallback((v: View) => {
    ref.current = v
    mem.x = v.x; mem.y = v.y; mem.k = v.k
    if (worldRef.current) worldRef.current.style.transform = `translate(${v.x}px, ${v.y}px) scale(${v.k})`
    if (!raf.current) raf.current = requestAnimationFrame(() => { raf.current = 0; setView(ref.current) })
  }, [mem])

  useEffect(() => () => { if (raf.current) cancelAnimationFrame(raf.current) }, [])

  // The viewport's size, kept up to date.
  useEffect(() => {
    const el = viewportRef.current
    if (!el) return
    const read = () => setSize((s) => (s.w === el.clientWidth && s.h === el.clientHeight ? s : { w: el.clientWidth, h: el.clientHeight }))
    read()
    if (typeof ResizeObserver === 'undefined') return
    const ro = new ResizeObserver(read)
    ro.observe(el)
    return () => ro.disconnect()
  }, [])

  /** Zoom by `factor` around the point (px, py) of the viewport (its middle by default). */
  const zoomBy = useCallback((factor: number, px?: number, py?: number) => {
    const v = ref.current
    const k = clampK(v.k * factor)
    const cx = px ?? size.w / 2, cy = py ?? size.h / 2
    set({ k, x: cx - ((cx - v.x) / v.k) * k, y: cy - ((cy - v.y) / v.k) * k })
  }, [set, size.w, size.h])

  // The wheel is a native listener: a React one cannot stop the page from scrolling.
  useEffect(() => {
    const el = viewportRef.current
    if (!el) return
    const onWheel = (e: WheelEvent) => {
      e.preventDefault()
      const r = el.getBoundingClientRect()
      if (e.ctrlKey || e.metaKey) zoomBy(Math.exp(-e.deltaY * (e.deltaMode === 1 ? 0.05 : 0.01)), e.clientX - r.left, e.clientY - r.top)
      else {
        const unit = e.deltaMode === 1 ? 16 : 1
        const v = ref.current
        const dx = e.deltaX !== 0 ? e.deltaX : e.shiftKey ? e.deltaY : 0
        const dy = e.shiftKey && e.deltaX === 0 ? 0 : e.deltaY
        set({ ...v, x: v.x - dx * unit, y: v.y - dy * unit })
      }
    }
    el.addEventListener('wheel', onWheel, { passive: false })
    return () => el.removeEventListener('wheel', onWheel)
  }, [set, zoomBy])

  return { view, ref, worldRef, viewportRef, size, set, zoomBy }
}
