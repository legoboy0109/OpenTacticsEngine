// Pointer gestures on the timeline:
//   - mouse / pen: drag empty space to move the picture; drag a box (or a chapter's name) to move that chapter;
//   - touch: one finger moves the picture (a tap on a box still opens it), two fingers pinch to zoom.
// A press only becomes a drag after it has moved a few pixels, so a click is still a click. Moving a chapter by touch is done with
// the buttons above the timeline (there is no drag for fingers: they are busy moving the picture).
import { useCallback, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react'
import { clampK, type View } from './usePanZoom'

const MOVE_THRESHOLD = 6

interface Props {
  viewportRef: React.RefObject<HTMLDivElement | null>
  viewRef: { current: View }
  set: (v: View) => void
  /** A chapter was dropped at this world point. */
  onDrop: (chapterId: string, wx: number, wy: number) => void
}

export interface DragState { chapterId: string; wx: number; wy: number }

type Gesture =
  | { kind: 'maybe'; id: number; x: number; y: number; chapter: string | null; view: View }
  | { kind: 'pan'; id: number; x: number; y: number; view: View }
  | { kind: 'drag'; id: number; chapter: string }
  | { kind: 'pinch'; d0: number; mid0: { x: number; y: number }; view: View }

export function useGestures({ viewportRef, viewRef, set, onDrop }: Props) {
  const pointers = useRef(new Map<number, { x: number; y: number }>())
  const gesture = useRef<Gesture | null>(null)
  const [drag, setDrag] = useState<DragState | null>(null)

  const local = (e: { clientX: number; clientY: number }) => {
    const r = viewportRef.current!.getBoundingClientRect()
    return { x: e.clientX - r.left, y: e.clientY - r.top }
  }
  const world = (p: { x: number; y: number }) => ({ wx: (p.x - viewRef.current.x) / viewRef.current.k, wy: (p.y - viewRef.current.y) / viewRef.current.k })

  const down = useCallback((e: ReactPointerEvent<HTMLDivElement>) => {
    if (e.pointerType === 'mouse' && e.button !== 0) return
    if ((e.target as HTMLElement).closest('.tl-plus, .tl-badge, .tl-expand')) return
    const p = local(e)
    pointers.current.set(e.pointerId, p)
    if (pointers.current.size === 2) {
      const [a, b] = [...pointers.current.values()]
      gesture.current = { kind: 'pinch', d0: Math.hypot(a.x - b.x, a.y - b.y) || 1, mid0: { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }, view: { ...viewRef.current } }
      try { viewportRef.current!.setPointerCapture(e.pointerId) } catch { /* the pointer is already gone */ }
      return
    }
    if (pointers.current.size > 2) return
    const chapter = e.pointerType === 'touch' ? null : (e.target as HTMLElement).closest<HTMLElement>('[data-drag-chapter]')?.dataset.dragChapter ?? null
    gesture.current = { kind: 'maybe', id: e.pointerId, x: p.x, y: p.y, chapter, view: { ...viewRef.current } }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const move = useCallback((e: ReactPointerEvent<HTMLDivElement>) => {
    if (!pointers.current.has(e.pointerId)) return
    const p = local(e)
    pointers.current.set(e.pointerId, p)
    const g = gesture.current
    if (!g) return
    if (g.kind === 'pinch') {
      if (pointers.current.size < 2) return
      const [a, b] = [...pointers.current.values()]
      const mid = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }
      const k = clampK(g.view.k * (Math.hypot(a.x - b.x, a.y - b.y) / g.d0))
      const wx = (g.mid0.x - g.view.x) / g.view.k, wy = (g.mid0.y - g.view.y) / g.view.k
      set({ k, x: mid.x - wx * k, y: mid.y - wy * k })
      return
    }
    if (g.kind === 'maybe' && e.pointerId === g.id) {
      if (Math.hypot(p.x - g.x, p.y - g.y) < MOVE_THRESHOLD) return
      try { viewportRef.current!.setPointerCapture(e.pointerId) } catch { /* gone */ }
      gesture.current = g.chapter ? { kind: 'drag', id: g.id, chapter: g.chapter } : { kind: 'pan', id: g.id, x: g.x, y: g.y, view: g.view }
      if (g.chapter) setDrag({ chapterId: g.chapter, ...world(p) })
      return
    }
    if (g.kind === 'pan' && e.pointerId === g.id) set({ ...g.view, x: g.view.x + p.x - g.x, y: g.view.y + p.y - g.y })
    else if (g.kind === 'drag' && e.pointerId === g.id) setDrag({ chapterId: g.chapter, ...world(p) })
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [set])

  const up = useCallback((e: ReactPointerEvent<HTMLDivElement>, cancelled = false) => {
    const g = gesture.current
    const p = pointers.current.get(e.pointerId)
    pointers.current.delete(e.pointerId)
    try { viewportRef.current?.releasePointerCapture(e.pointerId) } catch { /* not held */ }
    if (!g) return
    if (g.kind === 'drag' && e.pointerId === g.id) {
      setDrag(null)
      gesture.current = null
      if (!cancelled && p) { const w = world(p); onDrop(g.chapter, w.wx, w.wy) }
    } else if (g.kind === 'pinch') {
      // One finger left: keep moving the picture with it.
      const rest = [...pointers.current.entries()][0]
      gesture.current = rest ? { kind: 'pan', id: rest[0], x: rest[1].x, y: rest[1].y, view: { ...viewRef.current } } : null
    } else if ((g.kind === 'maybe' || g.kind === 'pan') && e.pointerId === g.id) gesture.current = null
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [onDrop])

  return { drag, handlers: { onPointerDown: down, onPointerMove: move, onPointerUp: up, onPointerCancel: (e: ReactPointerEvent<HTMLDivElement>) => up(e, true) } }
}
