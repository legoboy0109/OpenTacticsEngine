// Scene backdrops (docs/EVENTS.md "Scene backdrops"): data/campaign.json "backdrop" (a background id every chapter scene starts on) and a
// chapter scene's own { "backdrop": id | null, "music", "commands" } (null = the plain colour for that scene). A scene is the plain list
// until it needs the object form (campaignScene.ts); like the music, the backdrop is written only while it is set.
import { withCampaign } from './doc'
import { backgroundIds } from './backgrounds'
import { isSceneObject, withSceneBackdrop, type SceneBackdrop } from './campaignScene'
import type { SceneKey } from './campaignTypes'
import { patchChapter } from './campaign'
import type { ProjectDoc } from './types'
import { patchObj } from './util'

/** The campaign's own backdrop (undefined = none). */
export const campaignBackdrop = (doc: ProjectDoc): string | undefined => (typeof doc.campaign?.backdrop === 'string' ? doc.campaign.backdrop : undefined)

export function setCampaignBackdrop(doc: ProjectDoc, id: string | undefined): ProjectDoc {
  return withCampaign(doc, (c) => (c.backdrop === id ? c : patchObj(c, { backdrop: id })))
}

export function setSceneBackdrop(doc: ProjectDoc, chapterId: string, which: SceneKey, backdrop: SceneBackdrop): ProjectDoc {
  const cur = doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which]
  const next = withSceneBackdrop(cur, backdrop)
  return next === cur ? doc : patchChapter(doc, chapterId, { [which]: next })
}

/** What the loader refuses about backdrops (loader_campaign.cpp, loader_campaign_scene.cpp chapterScene), in plain words; `chapterId` for the chapter's own. */
export function backdropProblems(doc: ProjectDoc): Array<{ text: string; chapterId?: string }> {
  const c = doc.campaign
  if (!c) return []
  const out: Array<{ text: string; chapterId?: string }> = []
  const known = backgroundIds(doc)
  const unknown = (id: string) => `the background “${id}” is not one of this game’s (Art, Backgrounds)${known.length ? '' : ': there are none yet'}.`
  if (c.backdrop !== undefined) {
    if (typeof c.backdrop !== 'string' || !known.includes(c.backdrop)) out.push({ text: `The campaign’s backdrop: ${typeof c.backdrop === 'string' ? unknown(c.backdrop) : 'pick a background by its name.'}` })
  }
  for (const ch of Array.isArray(c.chapters) ? c.chapters : []) {
    for (const key of ['before', 'after'] as const) {
      const s = ch[key]
      if (!isSceneObject(s) || !('backdrop' in s) || s.backdrop === null) continue
      if (typeof s.backdrop !== 'string' || !known.includes(s.backdrop))
        out.push({ text: `Chapter “${ch.name?.trim() || ch.id}”, “${key === 'before' ? 'Before the battle' : 'After the battle'}”: ${typeof s.backdrop === 'string' ? unknown(s.backdrop) : 'the backdrop is a background’s name, or none.'}`, chapterId: ch.id })
    }
  }
  return out
}
