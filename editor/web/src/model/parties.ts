// Campaign parties (docs/EVENTS.md "Parties"): editing the list in data/campaign.json and the static walk the checks use.
// A party is its own roster, storage and funds under one story; scenes switch between them (`switchParty`) and join them
// (`mergeParties`). The first party in the list is the one the campaign starts with. The list is written only while it has a party
// in it (no key = one roster for the whole story, as before). A party's id is made once and never changes; renaming changes its name.
import { withCampaign } from './doc'
import { makeId } from './ids'
import { sceneCommands } from './campaignScene'
import { walkCampaign } from './campaignWalk'
import { isHumanSide } from './campaignRoster'
import type { Campaign, Chapter, Party } from './campaignTypes'
import { chapterName } from './campaignTypes'
import { storyIdOf } from './characterUnits'
import { commandKind, type Command } from './storyTypes'
import { DROP, forEachCommand } from './storyWalk'
import { patchObj, moveItem } from './util'
import type { MapFile, ProjectDoc } from './types'

export const partiesOf = (c: Campaign | null | undefined): Party[] => (Array.isArray(c?.parties) ? c!.parties! : [])

/** The party's name as the player sees it (its id while it has none). */
export const partyName = (c: Campaign | null | undefined, id: string): string => {
  const p = partiesOf(c).find((x) => x.id === id)
  return p ? (p.name?.trim() ? p.name : p.id) : id
}

export const newPartyId = (doc: ProjectDoc, name: string): string => makeId(name, partiesOf(doc.campaign).map((p) => p.id), { fallback: 'party' })

const setParties = (doc: ProjectDoc, list: Party[]): ProjectDoc => withCampaign(doc, (c) => patchObj(c, { parties: list.length ? list : undefined }))

/** Adds a party at the end (the first one added is the starting party, and the story's own party). */
export function addParty(doc: ProjectDoc, name: string, id: string = newPartyId(doc, name)): ProjectDoc {
  return setParties(doc, [...partiesOf(doc.campaign), { id, name }])
}

/** Renames (a name only; an empty name is written as no key, so the id shows). */
export function renameParty(doc: ProjectDoc, id: string, name: string): ProjectDoc {
  return setParties(doc, partiesOf(doc.campaign).map((p) => (p.id === id ? patchObj(p, { name: name === '' ? undefined : name }) : p)))
}

export function moveParty(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  const list = partiesOf(doc.campaign)
  return from === to || from < 0 || to < 0 || from >= list.length || to >= list.length ? doc : setParties(doc, moveItem(list, from, to))
}

/** Deletes the party. Every scene command and chapter that named it is cleaned up in the same step: a switch to it goes, a merge loses
 *  it (a merge into it, or one with nothing left to merge, goes), a chapter that started it no longer does. */
export function deleteParty(doc: ProjectDoc, id: string): ProjectDoc {
  return withCampaign(doc, (c) => {
    if (!partiesOf(c).some((p) => p.id === id)) return c
    const rest = partiesOf(c).filter((p) => p.id !== id)
    const next = walkCampaign({ ...c, parties: rest }, { ref: (r) => (r.kind === 'party' && r.id === id ? DROP : r.id) })
    return patchObj(next, { parties: rest.length ? rest : undefined })
  })
}

// ---- a static walk for the checks: the chapters in list order, each chapter's scenes top to bottom ----

export type PartyEvent =
  | { kind: 'switch'; party: string; chapter: Chapter; where: string }
  | { kind: 'merge'; from: string[]; into: string; chapter: Chapter; where: string }
  | { kind: 'battle'; chapter: Chapter }

/** Every party command in the order a straight play through the list would meet them (both branches of an if, every answer counted:
 *  the checks cannot know which is taken). A chapter's own `party` counts as a switch at its start; each chapter's battle is an event of its own. */
export function partyEvents(c: Campaign): PartyEvent[] {
  const out: PartyEvent[] = []
  const scan = (ch: Chapter, list: Command[], title: string) => {
    forEachCommand(list, `chapter “${chapterName(ch)}”, “${title}”`, (cmd: Command, where: string) => {
      const kind = commandKind(cmd)
      if (kind === 'switchParty' && typeof cmd.switchParty === 'string') out.push({ kind: 'switch', party: cmd.switchParty, chapter: ch, where })
      else if (kind === 'mergeParties' && Array.isArray(cmd.mergeParties) && typeof cmd.into === 'string')
        out.push({ kind: 'merge', from: cmd.mergeParties.filter((x): x is string => typeof x === 'string'), into: cmd.into, chapter: ch, where })
    })
  }
  for (const ch of Array.isArray(c.chapters) ? c.chapters : []) {
    if (typeof ch.party === 'string' && ch.party) out.push({ kind: 'switch', party: ch.party, chapter: ch, where: `chapter “${chapterName(ch)}”` })
    scan(ch, sceneCommands(ch.before), 'Before the battle')
    out.push({ kind: 'battle', chapter: ch })
    scan(ch, sceneCommands(ch.after), 'After the battle')
  }
  return out
}

const humanStoryIds = (m: MapFile | null | undefined): string[] => {
  if (!m) return []
  const human = new Set((m.sides ?? []).filter(isHumanSide).map((s) => s.id))
  const out: string[] = []
  for (const u of m.units ?? []) {
    const id = storyIdOf(u)
    if (id !== undefined && human.has(u.side) && !out.includes(id)) out.push(id)
  }
  return out
}

/** Notes about parties the loader accepts but a maker probably did not mean (hints, not errors). */
export function partyHints(doc: ProjectDoc): string[] {
  const c = doc.campaign
  const parties = partiesOf(c)
  if (!c || parties.length === 0) return []
  const events = partyEvents(c)
  const hints: string[] = []
  const name = (id: string) => `“${partyName(c, id)}”`
  const exists = (id: string) => parties.some((p) => p.id === id)
  let active = parties[0].id
  const used = new Set([active])  // parties that have been the active one by this point
  const merged = new Set<string>()
  const switchedTo = new Set<string>()
  const mergedAway = new Set<string>()
  // Who has been placed on the human sides of the chapters each party played so far (to say a map names someone only another party had).
  const seen = new Map<string, Set<string>>()
  const seenOf = (id: string) => { if (!seen.has(id)) seen.set(id, new Set()); return seen.get(id)! }
  for (const e of events) {
    if (e.kind === 'switch') {
      if (!exists(e.party)) continue
      if (merged.has(e.party)) hints.push(`${e.where}: switches to ${name(e.party)}, which was merged into another party earlier, so nothing happens.`)
      else { active = e.party; used.add(e.party); if (e.party !== parties[0].id) switchedTo.add(e.party) }
    } else if (e.kind === 'merge') {
      if (!exists(e.into)) continue
      if (merged.has(e.into)) { hints.push(`${e.where}: merges into ${name(e.into)}, which was merged away earlier, so nothing happens.`); continue }
      used.add(e.into)
      active = e.into
      for (const from of e.from) {
        if (!exists(from) || from === e.into) continue
        if (merged.has(from)) hints.push(`${e.where}: ${name(from)} was merged earlier already.`)
        else if (!used.has(from)) hints.push(`${e.where}: merges ${name(from)}, but nothing has switched to it before this, so it is empty.`)
        merged.add(from)
        mergedAway.add(from)
        for (const id of seenOf(from)) seenOf(e.into).add(id)
        seen.delete(from)
      }
    } else {
      const here = merged.has(active) ? parties[0].id : active
      const mapId = e.chapter.map
      const have = seenOf(here)
      for (const id of humanStoryIds(mapId ? doc.maps[mapId] : null)) {
        if (!have.has(id)) {
          const other = [...seen.entries()].find(([p, ids]) => p !== here && ids.has(id))
          if (other) hints.push(`Chapter “${chapterName(e.chapter)}”: its map puts ${id} on the player’s side, but so far only ${name(other[0])} has them, not ${name(here)}. They start fresh from the map’s own description.`)
        }
        have.add(id)
      }
    }
  }
  for (const p of parties.slice(1)) {
    if (switchedTo.has(p.id) && !mergedAway.has(p.id) && active !== p.id)
      hints.push(`Party ${name(p.id)} is switched to but never merged into another party, and the story does not end with it, so its units are left behind.`)
  }
  return hints
}
