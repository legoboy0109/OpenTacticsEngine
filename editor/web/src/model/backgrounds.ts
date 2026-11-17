// Story scene backgrounds: data/graphics.json "backgrounds": { "<id>": { "sheet": "<image id>", "cell": [x, y] } } (no "cell" = the
// whole image). The id is what a `background` story command names (docs/GRAPHICS.md "Backgrounds"). Deleting or renaming one
// is followed by every command that names it, in the same undo step (the same machinery as music: audioRefs.ts).
import { walkCampaign } from './campaignWalk'
import { sheetsOf, withGraphics } from './graphics'
import type { Usage } from './references'
import { scrubDoc, storyUsages } from './storyRefs'
import { isStoryId } from './storyIds'
import { DROP, walkStory, type Ref, type Walk } from './storyWalk'
import type { CellRef, ProjectDoc, SceneBackground } from './types'
import { patchObj } from './util'

export const backgroundList = (doc: ProjectDoc): Array<{ id: string; background: SceneBackground }> =>
  Object.entries(doc.graphics?.backgrounds ?? {}).map(([id, background]) => ({ id, background }))

export const backgroundIds = (doc: ProjectDoc): string[] => Object.keys(doc.graphics?.backgrounds ?? {})

const setBackgrounds = (doc: ProjectDoc, map: Record<string, SceneBackground>): ProjectDoc =>
  withGraphics(doc, (g) => patchObj(g, { backgrounds: Object.keys(map).length ? map : undefined }))

/** Why `id` cannot be a background id (null when it can). `except` = the background being renamed. */
export function backgroundIdProblem(doc: ProjectDoc, id: string, except?: string): string | null {
  if (id === '') return 'Type a name.'
  if (!isStoryId(id)) return 'Use letters, digits, _ and - only (no spaces).'
  if (id !== except && doc.graphics?.backgrounds && id in doc.graphics.backgrounds) return 'There is already a background with this name.'
  return null
}

/** A new background starts as the first image, whole. */
export function addBackground(doc: ProjectDoc, id: string): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet || backgroundIdProblem(doc, id)) return doc
  return setBackgrounds(doc, { ...(doc.graphics?.backgrounds ?? {}), [id]: { sheet } })
}

/** The picture: a whole image (`cell` null) or one piece of it. */
export function setBackgroundPicture(doc: ProjectDoc, id: string, sheet: string, cell: CellRef | null): ProjectDoc {
  const cur = doc.graphics?.backgrounds?.[id]
  if (!cur) return doc
  return setBackgrounds(doc, { ...doc.graphics!.backgrounds!, [id]: patchObj(cur, { sheet, cell: cell ?? undefined }) })
}

export function moveBackground(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  const entries = Object.entries(doc.graphics?.backgrounds ?? {})
  if (from === to || from < 0 || to < 0 || from >= entries.length || to >= entries.length) return doc
  const [it] = entries.splice(from, 1)
  entries.splice(to, 0, it)
  return setBackgrounds(doc, Object.fromEntries(entries))
}

/** Backgrounds that draw from the image (they go when the image is removed). */
export const backgroundsUsingSheet = (doc: ProjectDoc, sheetId: string): string[] =>
  backgroundList(doc).filter((b) => b.background.sheet === sheetId).map((b) => b.id)

// ---- the commands that name a background ----

const hit = (gone: Ref[], r: Ref): boolean => gone.some((g) => g.kind === r.kind && g.id === r.id)

/** Backgrounds that `before` had and `after` no longer has. */
export function goneBackgrounds(before: ProjectDoc, after: ProjectDoc): Ref[] {
  const b = before.graphics?.backgrounds
  if (!b || b === after.graphics?.backgrounds) return []
  return Object.keys(b).filter((id) => !(id in (after.graphics?.backgrounds ?? {}))).map((id): Ref => ({ kind: 'background', id }))
}

/** Takes out of the game every command that names a background this edit deleted (a rename is not a delete: the store runs this
 *  after every edit, and a renamed background is followed by `renameBackground` in the same edit, so it never looks gone). The same
 *  object when nothing was lost. */
export function reconcileBackgrounds(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (before === after) return after
  const gone = goneBackgrounds(before, after)
  if (!gone.length) return after
  let out = scrubDoc(after, gone)
  const drop: Walk = { ref: (r) => (hit(gone, r) ? DROP : r.id) }
  if (out.campaign) { const c = walkCampaign(out.campaign, drop); if (c !== out.campaign) out = { ...out, campaign: c } }
  return out
}

/** Renames in place (the order of the list is kept) and every command that names it follows. */
export function renameBackground(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const all = doc.graphics?.backgrounds
  if (!all || !(from in all) || from === to || backgroundIdProblem(doc, to, from)) return doc
  let out = setBackgrounds(doc, Object.fromEntries(Object.entries(all).map(([k, v]) => [k === from ? to : k, v])))
  const follow: Walk = { ref: (r) => (r.kind === 'background' && r.id === from ? to : r.id) }
  let maps = out.maps
  for (const [id, m] of Object.entries(out.maps)) {
    if (!m || (!m.events && !m.objectives)) continue
    const next = walkStory(m, follow)
    if (next !== m) maps = { ...maps, [id]: next }
  }
  if (maps !== out.maps) out = { ...out, maps }
  if (out.campaign) { const c = walkCampaign(out.campaign, follow); if (c !== out.campaign) out = { ...out, campaign: c } }
  return out
}

/** Removes the background; the store then takes out the commands that named it (reconcileBackgrounds). */
export function removeBackground(doc: ProjectDoc, id: string): ProjectDoc {
  const all = doc.graphics?.backgrounds
  if (!all || !(id in all)) return doc
  const { [id]: _gone, ...rest } = all
  return setBackgrounds(doc, rest)
}

/** Lines for the delete confirmation: what is taken out when this background goes. */
export const backgroundUsageLines = (doc: ProjectDoc, ids: string[]): Usage[] =>
  storyUsages(doc, ids.map((id): Ref => ({ kind: 'background', id })))

