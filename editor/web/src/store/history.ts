// Undo/redo as a snapshot stack. Snapshots share structure (edits are pure
// functions that only copy what changed), so 200 of them are cheap.

export const HISTORY_CAP = 200
/** Edits with the same key within this many ms count as one undo step (typing). */
export const COALESCE_MS = 1500

export interface History<T> {
  past: T[]
  present: T
  future: T[]
  lastKey: string | null
  lastTime: number
}

export const initHistory = <T,>(present: T): History<T> => ({ past: [], present, future: [], lastKey: null, lastTime: 0 })
export const canUndo = (h: History<unknown>): boolean => h.past.length > 0
export const canRedo = (h: History<unknown>): boolean => h.future.length > 0

export function pushEdit<T>(h: History<T>, next: T, key: string | undefined, now: number): History<T> {
  if (next === h.present) return h
  if (key !== undefined && key === h.lastKey && now - h.lastTime < COALESCE_MS && h.past.length > 0) {
    return { ...h, present: next, future: [], lastTime: now }
  }
  return {
    past: [...h.past, h.present].slice(-HISTORY_CAP),
    present: next,
    future: [],
    lastKey: key ?? null,
    lastTime: now,
  }
}

export function undo<T>(h: History<T>): History<T> {
  if (!canUndo(h)) return h
  return {
    past: h.past.slice(0, -1),
    present: h.past[h.past.length - 1],
    future: [h.present, ...h.future],
    lastKey: null,
    lastTime: 0,
  }
}

export function redo<T>(h: History<T>): History<T> {
  if (!canRedo(h)) return h
  return {
    past: [...h.past, h.present],
    present: h.future[0],
    future: h.future.slice(1),
    lastKey: null,
    lastTime: 0,
  }
}
