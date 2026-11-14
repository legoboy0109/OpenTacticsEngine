// Fast-changing editor state that must not re-render React on every mouse move:
// the view (zoom/pan), hover, keyboard cursor, selection and the drag ghost.
// The canvas listens directly; small components subscribe to one slice.
import { useSyncExternalStore } from 'react'
import type { Cell } from '../../../model/mapGrid'
import { fitView, zoomAt, type View } from './geometry'

type Fields = Pick<MapViewState, 'view' | 'autoFit' | 'hover' | 'cursor' | 'cursorVisible' | 'selected' | 'selSize' | 'ghost' | 'placing' | 'dragS' | 'size'>

export class MapViewState {
  view: View = { scale: 16, x: 0, y: 0 }
  /** Canvas size in CSS pixels. */
  size = { w: 0, h: 0 }
  /** Until the user zooms or pans, the map keeps fitting the window. */
  autoFit = true
  hover: Cell | null = null
  cursor: Cell | null = null
  cursorVisible = false
  selected: Cell | null = null
  /** Footprint of the selected structure (null = the selection is one square). */
  selSize: { w: number; h: number } | null = null
  ghost: { from: Cell; to: Cell } | null = null
  /** The Structures tool is active with this kind and owner ('' = nobody): a footprint follows the pointer. */
  placing: { type: string; side: string } | null = null
  /** A placed structure (by its top-left square) is being dragged by (dx, dy) squares. */
  dragS: { origin: Cell; dx: number; dy: number } | null = null
  private listeners = new Set<() => void>()
  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  set(patch: Partial<Fields>): void {
    // Selecting another square forgets the old structure footprint unless the caller gives one.
    if ('selected' in patch && !('selSize' in patch)) this.selSize = null
    Object.assign(this, patch)
    this.listeners.forEach((l) => l())
  }
  /** Zoom around the middle of the canvas (the +/- buttons and keys). */
  zoomBy(k: number): void {
    this.set({ view: zoomAt(this.view, k, this.size.w / 2, this.size.h / 2), autoFit: false })
  }
  fit(mapW: number, mapH: number): void {
    if (this.size.w > 0 && this.size.h > 0) this.set({ view: fitView(this.size.w, this.size.h, mapW, mapH), autoFit: true })
  }
}

/** Re-renders only when `pick`'s (primitive) result changes. */
export function useViewSlice<T extends string | number | boolean | null>(s: MapViewState, pick: (s: MapViewState) => T): T {
  return useSyncExternalStore(s.subscribe, () => pick(s))
}
