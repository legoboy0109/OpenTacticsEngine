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
import { isSceneObject, sceneCommands, sceneMusic, withSceneBackdrop, withSceneCommands, withSceneMusic, type Scene } from './campaignScene'
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
  if (typeof ch.party === 'string' && ch.party !== '') {
    const r = w.ref({ kind: 'party', id: ch.party }, label)
    if (r === DROP) {
      w.dropped?.({ what: 'setting', text: 'it no longer starts a party', where: label })
      out = put(out, 'party', undefined)
    } else out = put(out, 'party', r)
  }
  for (const key of ['before', 'after'] as const) {
    const scene = ch[key]
    if (scene === undefined || (!Array.isArray(scene) && !isSceneObject(scene))) continue
    const where = `${label}, “${SCENE_TITLE[key]}”`
    let next: Scene | undefined = scene
    // The music of the object form is a name of a track, like a `playMusic` command at the start of the scene.
    const music = sceneMusic(scene)
    if (music !== undefined) {
      const r = w.ref({ kind: 'music', id: music }, where)
      if (r === DROP) {
        w.dropped?.({ what: 'setting', text: 'its music is taken off', where })
        next = withSceneMusic(next, undefined)
      } else if (r !== music) next = withSceneMusic(next, r)
    }
    // Its backdrop names a background (null = the plain colour: not a name).
    if (isSceneObject(next) && typeof next.backdrop === 'string') {
      const r = w.ref({ kind: 'background', id: next.backdrop }, where)
      if (r === DROP) {
        w.dropped?.({ what: 'setting', text: 'its backdrop is taken off (it then starts on the campaign’s)', where })
        next = withSceneBackdrop(next, undefined)
      } else if (r !== next.backdrop) next = withSceneBackdrop(next, r)
    }
    const list = sceneCommands(next)
    const mapped = mapCommands(list, w, where)
    if (mapped !== list) next = isSceneObject(next) ? withSceneCommands(next, mapped) : mapped
    if (next !== scene) out = put(out, key, next)
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
  if (typeof c.backdrop === 'string') {  // the campaign's own backdrop names a background too
    const r = w.ref({ kind: 'background', id: c.backdrop }, 'the campaign’s backdrop')
    if (r === DROP) {
      w.dropped?.({ what: 'setting', text: 'the campaign’s backdrop is taken off', where: 'the campaign’s backdrop' })
      const { backdrop: _gone, ...rest } = out
      out = rest as Campaign
    } else if (r !== c.backdrop) out = { ...out, backdrop: r }
  }
  const fb = c.freeBattles
  const list = Array.isArray(fb) ? fb : fb && typeof fb === 'object' && Array.isArray(fb.maps) ? fb.maps : null
  if (list) {
    const kept = list.filter((id) => {
      const r = typeof id === 'string' ? w.ref({ kind: 'map', id }, 'the free battles') : id
      if (r === DROP) w.dropped?.({ what: 'freeBattle', text: id, where: 'the free battles' })
      return r !== DROP
    })
    if (kept.length !== list.length) out = { ...out, freeBattles: Array.isArray(fb) ? kept : { ...(fb as object), maps: kept } }
  }
  return out
}
