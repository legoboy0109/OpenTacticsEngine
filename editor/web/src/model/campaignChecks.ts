// Checks that mirror what the game loader refuses about data/campaign.json (engine/core/data/loader_campaign*.cpp), so a
// maker sees them before pressing Play, plus a few hints the loader accepts but a maker probably did not mean. Names are
// checked by the same walk as clean-up and rename (campaignWalk.ts), so the knowledge of where ids sit is written once.
// The wording is plain; the loader's own message still shows in the Problems list after a save.
import { unreachedChapters } from './campaignFlow'
import { allSideIds, definedStoryIds } from './campaignRoster'
import { chapterName, type Campaign, type Chapter } from './campaignTypes'
import { SCENE_TITLE, walkCampaign } from './campaignWalk'
import { isStoryId } from './storyIds'
import { commandKind, type Command } from './storyTypes'
import { forEachCommand, type Ref } from './storyWalk'
import type { ProjectDoc } from './types'

/** `error`: the loader refuses the file. `hint`: it loads, but check that this is what you meant. `topic` says which
 *  part of the game the message is about, so other screens (Rules: switches, variables, resources) can show theirs. */
export interface CampaignCheck {
  level: 'error' | 'hint'
  topic: 'general' | 'chapter' | 'map' | 'switch' | 'variable' | 'resource' | 'side' | 'unit'
  text: string
  chapterId?: string
}

const BIG = 1_000_000_000
const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

function refProblem(doc: ProjectDoc, c: Campaign, r: Ref): { topic: CampaignCheck['topic']; level: CampaignCheck['level']; text: string } | null {
  const known = (list: Array<{ id: string }> | undefined) => !!list?.some((x) => x.id === r.id)
  const pick = (what: string, topic: CampaignCheck['topic']) => (r.id === '' ? { topic, level: 'error' as const, text: `pick ${what}.` } : null)
  switch (r.kind) {
    case 'map': return pick('a map', 'map') ?? (known(doc.game?.maps) ? null : { topic: 'map', level: 'error', text: `“${r.id}” is not a map of this game.` })
    case 'chapter': return pick('a chapter', 'chapter') ?? (c.chapters.some((x) => x.id === r.id) ? null : { topic: 'chapter', level: 'error', text: `“${r.id}” is not a chapter of this campaign.` })
    case 'switch': return pick('a switch', 'switch') ?? (known(doc.game?.switches) ? null : { topic: 'switch', level: 'error', text: `“${r.id}” is not a switch of this game.` })
    case 'variable': return pick('a variable', 'variable') ?? (known(doc.game?.variables) ? null : { topic: 'variable', level: 'error', text: `“${r.id}” is not a variable of this game.` })
    case 'resource': return pick('a resource', 'resource') ?? (known(doc.game?.resources) ? null : { topic: 'resource', level: 'error', text: `“${r.id}” is not a resource of this game.` })
    case 'side':
      if (r.id === '') return { topic: 'side', level: 'error', text: 'pick a side.' }
      if (!isStoryId(r.id)) return { topic: 'side', level: 'error', text: 'a side id may use only letters, digits, _ and - (up to 64).' }
      return allSideIds(doc).has(r.id) ? null : { topic: 'side', level: 'hint', text: `no map has a side “${r.id}”, so this gift goes to nobody.` }
    case 'unit':
      if (r.id === '') return { topic: 'unit', level: 'error', text: 'pick a unit.' }
      if (!isStoryId(r.id)) return { topic: 'unit', level: 'error', text: 'a unit’s story name may use only letters, digits, _ and - (up to 64).' }
      return definedStoryIds(doc).has(r.id) ? null : { topic: 'unit', level: 'hint', text: `no map has a unit with the story name “${r.id}” (give a unit that story name in the map editor).` }
    default: return null
  }
}

function commandChecks(c: Command, where: string, say: (level: CampaignCheck['level'], topic: CampaignCheck['topic'], where: string, text: string) => void, allowGoto: boolean): void {
  switch (commandKind(c)) {
    case 'say':
      if (!String(c.say ?? '').trim()) say('error', 'general', where, 'the text to say is empty.')
      break
    case 'giveResource':
      if (!Object.keys(c.giveResource ?? {}).length) say('error', 'resource', where, 'choose a resource to give.')
      else if (Object.values(c.giveResource!).some((n) => Math.abs(n) > BIG)) say('error', 'resource', where, 'an amount is unreasonably large.')
      break
    case 'setVariable':
    case 'addVariable':
      if (Math.abs(Number(c.to ?? c.by ?? 0)) > BIG) say('error', 'variable', where, 'the number is unreasonably large.')
      break
    case 'goto':
      if (!allowGoto) say('error', 'chapter', where, '“Go to a chapter” only works in an “After the battle” scene.')
      break
    case 'if':
      if (!c.if?.length) say('hint', 'general', where, 'this “If” has no conditions, so it always happens.')
      break
    case 'setSwitch':
      break
    default:
      // Only reachable from a hand-edited file: the Campaign screen offers nothing else.
      say('error', 'general', where, 'a scene cannot use this command.')
  }
}

/** Everything wrong with the campaign; empty when all is well (or there is none). */
export function campaignChecks(doc: ProjectDoc): CampaignCheck[] {
  const c = doc.campaign
  if (!c) return []
  const out: CampaignCheck[] = []
  const say = (level: CampaignCheck['level'], topic: CampaignCheck['topic'], where: string, text: string, chapterId?: string) =>
    out.push({ level, topic, text: where ? `${cap(where)}: ${text}` : cap(text), chapterId })

  if (!Array.isArray(c.chapters) || c.chapters.length === 0) say('error', 'general', '', 'a campaign needs at least one chapter.')
  const seen = new Set<string>()
  const chapters: Chapter[] = Array.isArray(c.chapters) ? c.chapters : []
  chapters.forEach((ch) => {
    const where = `chapter “${chapterName(ch)}”`
    if (!isStoryId(ch.id ?? '')) say('error', 'chapter', where, 'the internal name may use only letters, digits, _ and - (up to 64).', ch.id)
    if (seen.has(ch.id)) say('error', 'chapter', where, 'another chapter has the same internal name.', ch.id)
    seen.add(ch.id)
    for (const key of ['before', 'after'] as const) {
      forEachCommand(ch[key], `${where}, “${SCENE_TITLE[key]}”`, (cmd, w) => commandChecks(cmd, w, (l, t, ww, tx) => say(l, t, ww, tx, ch.id), key === 'after'))
    }
  })

  // Do the names the campaign points at exist? (one walk for every kind of name)
  const idOf = new Map<string, string>()
  chapters.forEach((ch) => idOf.set(chapterName(ch), ch.id))
  walkCampaign(c, {
    ref: (r, where) => {
      const problem = refProblem(doc, c, r)
      if (problem) {
        const m = /^chapter “(.*?)”/.exec(where)
        say(problem.level, problem.topic, where, problem.text, m ? idOf.get(m[1]) : undefined)
      }
      return r.id
    },
  })

  // Hints: a chapter no chapter leads to (the first one is where the story starts).
  for (const id of unreachedChapters(c)) {
    const ch = chapters.find((x) => x.id === id)!
    say('hint', 'chapter', `chapter “${chapterName(ch)}”`, 'no chapter leads to it, so it is never played (set another chapter’s “Next chapter” to it, or use “Go to a chapter”).', id)
  }
  return out
}
