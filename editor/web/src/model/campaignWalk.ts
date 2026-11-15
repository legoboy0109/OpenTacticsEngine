// The same kind of walk as storyWalk.ts, over data/campaign.json: every place a campaign NAMES something (maps, chapters,
// switches, variables, resources, sides, roster units). One traversal serves four jobs, exactly as for a map's events:
//   - rename (the callback returns a new id: a chapter or a unit's story name changed),
//   - clean up after a delete (DROP: a chapter's map is cleared, `next` / a free-battle entry / a command / one
//     resource of a gift goes; the chapter itself stays, with its writing),
//   - "used by" (`dropped` gets a sentence for each),
//   - checks (the callback gets `where` and says whether the name exists).
// The scene commands are walked by storyWalk's own `mapCommands`, so a command is understood in one place only.
// Returns the very same object when nothing changed.
import { chapterName, type Campaign, type Chapter, type SceneKey } from './campaignTypes'
import { DROP, mapCommands, put, type Walk } from './storyWalk'

export const SCENE_TITLE: Record<SceneKey, string> = { before: 'Before the battle', after: 'After the battle' }

function walkChapter(ch: Chapter, w: Walk): Chapter {
  const label = `chapter “${chapterName(ch)}”`
  let out = ch
  if (typeof ch.map === 'string') {
    const r = w.ref({ kind: 'map', id: ch.map }, label)
    if (r === DROP) {
      w.dropped?.({ what: 'chapterMap', text: chapterName(ch), where: label })
      out = put(out, 'map', '')
    } else out = put(out, 'map', r)
  }
  if (typeof ch.next === 'string' && ch.next !== '') {
    const r = w.ref({ kind: 'chapter', id: ch.next }, label)
    if (r === DROP) {
      w.dropped?.({ what: 'link', text: chapterName(ch), where: label })
      out = put(out, 'next', undefined)
    } else out = put(out, 'next', r)
  }
  for (const key of ['before', 'after'] as const) {
    const list = ch[key]
    if (!Array.isArray(list)) continue
    const next = mapCommands(list, w, `${label}, “${SCENE_TITLE[key]}”`)
    if (next !== list) out = put(out, key, next)
  }
  return out
}

/** Undefined values are written as "no key": `put(o, k, undefined)` leaves the key in with undefined, so strip it. */
const stripUndefined = (c: Chapter): Chapter => {
  if (!Object.values(c).includes(undefined)) return c
  return Object.fromEntries(Object.entries(c).filter(([, v]) => v !== undefined)) as Chapter
}

export function walkCampaign(c: Campaign, w: Walk): Campaign {
  let changed = false
  const chapters = c.chapters.map((ch) => {
    const next = stripUndefined(walkChapter(ch, w))
    if (next !== ch) changed = true
    return next
  })
  let out: Campaign = changed ? { ...c, chapters } : c
  if (Array.isArray(c.freeBattles)) {
    const kept = c.freeBattles.filter((id) => {
      const r = typeof id === 'string' ? w.ref({ kind: 'map', id }, 'the free battles') : id
      if (r === DROP) w.dropped?.({ what: 'freeBattle', text: id, where: 'the free battles' })
      return r !== DROP
    })
    if (kept.length !== c.freeBattles.length) out = { ...out, freeBattles: kept }
  }
  return out
}
