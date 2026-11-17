// Edits the timeline makes, all on the document like every other Campaign edit (so Undo, "Unsaved changes", the checks and the
// clean-up after deletes keep working): add a chapter at a place, move one, add or remove a scene.
import { moveChapter, newChapterId, patchChapter } from './campaign'
import { withCampaign } from './doc'
import type { Chapter, SceneKey } from './campaignTypes'
import type { Rect } from './timelineTypes'
import type { ProjectDoc } from './types'

/** A new chapter at list position `index` (0 = first, length = last), on the map of the chapter before it (else after it). */
export function insertChapter(doc: ProjectDoc, name: string, index: number, id: string = newChapterId(doc, name)): ProjectDoc {
  return withCampaign(doc, (c) => {
    const at = Math.max(0, Math.min(index, c.chapters.length))
    const map = c.chapters[at - 1]?.map || c.chapters[at]?.map || doc.game?.maps[0]?.id || ''
    const chapter: Chapter = { id, name, map }
    return { ...c, chapters: [...c.chapters.slice(0, at), chapter, ...c.chapters.slice(at)] }
  })
}

/** Moves a chapter to list position `to`. */
export function moveChapterTo(doc: ProjectDoc, id: string, to: number): ProjectDoc {
  const from = doc.campaign?.chapters.findIndex((c) => c.id === id) ?? -1
  return from < 0 || from === to ? doc : moveChapter(doc, from, to)
}

/** An empty scene. It is saved as `[]` and goes away again by itself when it is removed. */
export const addScene = (doc: ProjectDoc, chapterId: string, which: SceneKey): ProjectDoc => {
  const ch = doc.campaign?.chapters.find((c) => c.id === chapterId)
  return !ch || ch[which] !== undefined ? doc : patchChapter(doc, chapterId, { [which]: [] })
}

export const removeScene = (doc: ProjectDoc, chapterId: string, which: SceneKey): ProjectDoc => {
  const ch = doc.campaign?.chapters.find((c) => c.id === chapterId)
  return !ch || ch[which] === undefined ? doc : patchChapter(doc, chapterId, { [which]: undefined })
}

/** Where a dragged chapter would land: the list position it ends up at when dropped at world point (x, y). The nearest chapter
 *  decides; the left half of it means "before it", the right half "after it". Dropping on itself changes nothing. */
export function dropIndex(strips: Array<{ chapterId: string; rect: Rect }>, order: string[], dragged: string, x: number, y: number): number {
  const from = order.indexOf(dragged)
  let best = -1, bestD = Infinity
  strips.forEach((s, i) => {
    const dx = Math.max(s.rect.x - x, 0, x - (s.rect.x + s.rect.w)), dy = Math.max(s.rect.y - y, 0, y - (s.rect.y + s.rect.h))
    const d = Math.hypot(dx, dy)
    if (d < bestD) { bestD = d; best = i }
  })
  if (best < 0 || from < 0) return from
  const s = strips[best]
  if (s.chapterId === dragged) return from
  let at = order.indexOf(s.chapterId) + (x > s.rect.x + s.rect.w / 2 ? 1 : 0)
  if (from < at) at -= 1
  return at
}
