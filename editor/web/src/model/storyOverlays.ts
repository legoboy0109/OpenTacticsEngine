// The squares an open event (or the objectives) is about, so the map can draw them while the person edits: the
// trigger's area, and the squares of commands that add, move or look at something. Pure; drawing is in drawStory.ts.
import { rectOf, triggerKind, type Objectives, type StoryEvent } from './storyTypes'
import { forEachCommand } from './storyWalk'

export type OverlayTone = 'area' | 'add' | 'move' | 'view' | 'win' | 'lose' | 'slot'
export interface Overlay { x: number; y: number; w: number; h: number; label: string; tone: OverlayTone }

const num = (n: unknown) => (typeof n === 'number' && Number.isFinite(n) ? n : 0)

export function objectiveOverlays(o: Objectives | undefined): Overlay[] {
  const out: Overlay[] = []
  for (const k of ['win', 'lose'] as const)
    for (const r of o?.[k] ?? []) if (r.reach) out.push({ ...rectOf(r.reach), label: k === 'win' ? 'Win area' : 'Lose area', tone: k })
  return out
}

export function eventOverlays(e: StoryEvent | undefined): Overlay[] {
  if (!e) return []
  const out: Overlay[] = []
  if (triggerKind(e.when ?? {}) === 'reach' && e.when.reach) out.push({ ...rectOf(e.when.reach), label: 'Area', tone: 'area' })
  forEachCommand(e.do, '', (c) => {
    if (c.addUnit) out.push({ x: num(c.addUnit.x), y: num(c.addUnit.y), w: 1, h: 1, label: 'Added', tone: 'add' })
    else if (c.moveUnit !== undefined) out.push({ x: num(c.x), y: num(c.y), w: 1, h: 1, label: 'Move to', tone: 'move' })
    else if (c.focus && typeof c.focus === 'object') out.push({ x: num(c.focus.x), y: num(c.focus.y), w: 1, h: 1, label: 'View', tone: 'view' })
    else if (c.objective) out.push(...objectiveOverlays(c.objective))
  })
  return out
}

/** The rectangle between two squares (either corner first), at least 1 x 1. */
export function rectBetween(a: { x: number; y: number }, b: { x: number; y: number }): { x: number; y: number; w: number; h: number } {
  return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), w: Math.abs(a.x - b.x) + 1, h: Math.abs(a.y - b.y) + 1 }
}
