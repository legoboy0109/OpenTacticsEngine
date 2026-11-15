// Picking a square or an area on the map for a story form. While a form is waiting (vs.picking), the map's own tools
// are off: the next press is the start of the answer, a drag grows the rectangle (drawn dashed), release gives it.
import type { Cell } from '../../../../model/mapGrid'
import { rectBetween } from '../../../../model/storyOverlays'
import type { Stroke } from '../tools'
import type { MapViewState, PickRequest } from '../viewState'

/** Ends a pick without an answer. */
export function stopPicking(vs: MapViewState): void {
  if (vs.picking || vs.pickRect) vs.set({ picking: null, pickRect: null })
}

export function pickStroke(vs: MapViewState, req: PickRequest, first: Cell): Stroke {
  let last = first
  if (req.mode === 'area') vs.set({ pickRect: rectBetween(first, first) })
  return {
    move(c) {
      last = c
      if (req.mode === 'area') vs.set({ pickRect: rectBetween(first, c) })
    },
    end(c) {
      const at = c ?? last
      vs.set({ picking: null, pickRect: null })
      req.done(req.mode === 'area' ? rectBetween(first, at) : { x: at.x, y: at.y, w: 1, h: 1 })
    },
  }
}
