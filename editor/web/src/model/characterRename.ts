// Renaming a character's id (Characters screen, Advanced > Internal name > Rename). A character's id is also the story id of
// its unit and the key of its portrait, so the new id has to follow into EVERY place that names it, in ONE edit (one Undo step).
//
// Where the old id can sit (built from the loaders in engine/core/data; tests/unit/characterRename.test.ts builds a game that
// uses every one of them and checks none is left):
//   data/characters.json   the entry's `id`; every character's quotes (`against`, a line's `speaker` and `portrait`)
//   data/supports.json     a pair's `a` / `b`; a pair's conversation lines (`speaker`, `portrait`)
//   data/weapons.json, items.json   `wieldedBy` (weapons and staves)
//   data/graphics.json     `portraits` (general and per style) keyed by the id; `battle.assign` rules with `character`
//   maps/*.json            units with `character`; `prep` required / forbidden / fixed; structures' `visitUnits`; units' behaviour
//                          (`seek.unit`, `prefer.unit`); events and objectives (every filter, trigger, condition and command that names
//                          a unit: speaker, by, talk / to, defeated, fight / with, sees, alive / dead, moveUnit, removeUnit, changeSide,
//                          promote, giveItem / takeItem, readings of `level` / `hp` / `maxHp` / `of`, setBehaviour units and targets,
//                          addUnit `character` and its behaviour, support pairs, openChest `by`, focus, a line's `portrait`)
//   data/campaign.json     the same commands and conditions inside scenes
// What cannot follow: Lua scripts and formulas that spell the id in quotes (`formulaMentions` lists them so the maker can look),
// and saves (a save names units by id: see docs/SAVES.md and docs/SRPG.md "Renaming a character").
import { renameBehaviourUnit } from './behaviour'
import { walkCampaign } from './campaignWalk'
import { renameCharacterLocks } from './locks'
import { followCharacterInPromotions } from './promotionLevels'
import { renameVisitUnit } from './mapPlaces'
import { ID_RULE, idProblem, isStoryId, takenUnitIds } from './storyIds'
import { mapCommands, walkStory, type Ref, type Walk } from './storyWalk'
import type { Character, SupportPair } from './srpgTypes'
import type { Command } from './storyTypes'
import type { MapPrep } from './prepTypes'
import type { MapFile, ProjectDoc } from './types'
import { patchObj } from './util'

const mapEntries = (doc: ProjectDoc): Array<[string, MapFile]> => Object.entries(doc.maps).filter((e): e is [string, MapFile] => e[1] !== null)
const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id

/** Every portrait picture entry (general and per style) that is keyed by `id`. */
const hasPortrait = (doc: ProjectDoc, id: string): boolean =>
  !!doc.graphics && (id in (doc.graphics.portraits ?? {}) || Object.values(doc.graphics.styles ?? {}).some((st) => id in ((st as { portraits?: object }).portraits ?? {})))

/** Why `to` cannot be the new id of character `from` (null when it can), in plain words. */
export function characterIdProblem(doc: ProjectDoc, from: string, to: string): string | null {
  if (to === from) return null
  if (to === '') return 'Type the new internal name.'
  if (to.length > 64) return 'At most 64 characters.'
  if (!isStoryId(to)) return ID_RULE
  const other = (doc.characters ?? []).find((c) => c.id === to)
  if (other) return `The character “${other.name}” already has this internal name.`
  // A character's id is the story name of its unit: no unit of any map (placed with its own story name, or added by an event) may have it.
  for (const [mid, m] of mapEntries(doc)) {
    const taken = takenUnitIds(m).filter((id) => id !== from)
    if (idProblem(to, taken) !== null) return `A unit on the map “${mapName(doc, mid)}” already has the story name “${to}”. A character’s internal name is also the story name of its unit, so pick another.`
  }
  // Its portrait follows it; another portrait cannot be in the way.
  if (hasPortrait(doc, from) && hasPortrait(doc, to)) return `There is already a portrait called “${to}” (on the Art screen). Rename or remove it first, or pick another name.`
  return null
}

/** The same list with each entry's id renamed; the very same array when nothing changed. */
const swap = (list: string[] | undefined, from: string, to: string): string[] | undefined =>
  list?.includes(from) ? list.map((x) => (x === from ? to : x)) : list

function renamePrep(prep: MapPrep, from: string, to: string): MapPrep {
  const required = prep.required?.map((r) => (typeof r === 'string' ? (r === from ? to : r) : r.character === from ? { ...r, character: to } : r))
  const next: MapPrep = { ...prep }
  if (required && required.some((r, i) => r !== prep.required![i])) next.required = required
  const forbidden = swap(prep.forbidden, from, to)
  if (forbidden !== prep.forbidden) next.forbidden = forbidden
  const fixed = swap(prep.fixed, from, to)
  if (fixed !== prep.fixed) next.fixed = fixed
  return next.required === prep.required && next.forbidden === prep.forbidden && next.fixed === prep.fixed ? prep : next
}

/** One map: the placed character, the lists of its preparation screen, its visits, the units that head for it, its events and objectives. */
function renameInMap(map: MapFile, from: string, to: string, walk: Walk): MapFile {
  let out = map
  if (out.units?.some((u) => u.character === from)) out = { ...out, units: out.units.map((u) => (u.character === from ? { ...u, character: to } : u)) }
  if (out.prep) { const prep = renamePrep(out.prep, from, to); if (prep !== out.prep) out = { ...out, prep } }
  out = renameVisitUnit(out, from, to)
  out = renameBehaviourUnit(out, from, to)
  return out.events || out.objectives ? walkStory(out, walk) : out
}

const renameQuotes = (c: Character, walk: Walk, from: string, to: string): Character => {
  const q = c.quotes
  if (!q) return c
  const lines = (list: Command[] | undefined) => (list ? mapCommands(list, walk, `${c.name}’s quote`) : list)
  const battle = q.battle?.map((b) => {
    const l = lines(b.lines)!
    return l === b.lines && b.against !== from ? b : { ...b, ...(b.against === from ? { against: to } : {}), lines: l }
  })
  const falls = lines(q.defeat?.falls), retreats = lines(q.defeat?.retreats)
  const changed = (battle && battle.some((b, i) => b !== q.battle![i])) || falls !== q.defeat?.falls || retreats !== q.defeat?.retreats
  if (!changed) return c
  return { ...c, quotes: { ...q, ...(battle ? { battle } : {}), ...(q.defeat ? { defeat: { ...q.defeat, ...(falls ? { falls } : {}), ...(retreats ? { retreats } : {}) } } : {}) } }
}

const renamePair = (p: SupportPair, walk: Walk, from: string, to: string): SupportPair => {
  const levels = p.levels.map((lv) => {
    if (!lv.talk) return lv
    const talk = mapCommands(lv.talk, walk, 'a conversation')
    return talk === lv.talk ? lv : { ...lv, talk }
  })
  const changed = levels.some((lv, i) => lv !== p.levels[i])
  if (!changed && p.a !== from && p.b !== from) return p
  return { ...p, a: p.a === from ? to : p.a, b: p.b === from ? to : p.b, levels: changed ? levels : p.levels }
}

/** A portrait table with the key `from` renamed in place (the order is kept); the same object when it has no such key. */
function renamePortraitKey<T>(table: Record<string, T> | undefined, from: string, to: string): Record<string, T> | undefined {
  if (!table || !(from in table)) return table
  return Object.fromEntries(Object.entries(table).map(([k, v]) => [k === from ? to : k, v]))
}

function renameInGraphics(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  const g = doc.graphics
  if (!g) return doc
  let out = g
  const portraits = renamePortraitKey(g.portraits, from, to)
  if (portraits !== g.portraits) out = { ...out, portraits }
  // A character's own picture (graphics `characters`, general and per style) is keyed by the id too.
  const looks = renamePortraitKey(g.characters, from, to)
  if (looks !== g.characters) out = { ...out, characters: looks }
  if (g.styles) {
    let styles = g.styles
    for (const [sid, st] of Object.entries(g.styles)) {
      const p = renamePortraitKey((st as { portraits?: Record<string, unknown> }).portraits, from, to)
      if (p !== (st as { portraits?: unknown }).portraits) styles = { ...styles, [sid]: { ...st, portraits: p } as typeof st }
      const cl = renamePortraitKey(st.characters, from, to)
      if (cl !== st.characters) styles = { ...styles, [sid]: { ...(styles[sid] ?? st), characters: cl } as typeof st }
    }
    if (styles !== g.styles) out = { ...out, styles }
  }
  const assign = g.battle?.assign
  if (assign?.some((a) => a.character === from)) out = { ...out, battle: { ...g.battle!, assign: assign.map((a) => (a.character === from ? { ...a, character: to } : a)) } }
  return out === g ? doc : { ...doc, graphics: out }
}

/** Renames character `from` to `to` everywhere, in one edit. `error` says why it was refused (the document is then unchanged). */
export function renameCharacter(doc: ProjectDoc, from: string, to: string): { doc: ProjectDoc; error: string | null } {
  if (from === to || !doc.characters?.some((c) => c.id === from)) return { doc, error: null }
  const error = characterIdProblem(doc, from, to)
  if (error) return { doc, error }
  // The portrait id a line names follows only when the portrait itself is renamed (otherwise the line asks for a picture that never existed).
  const portrait = hasPortrait(doc, from)
  const walk: Walk = {
    ref: (r: Ref) => ((r.kind === 'unit' || r.kind === 'character' || (portrait && r.kind === 'portrait')) && r.id === from ? to : r.id),
  }
  let next: ProjectDoc = { ...doc }
  next.characters = doc.characters.map((c) => renameQuotes(c.id === from ? patchObj(c, { id: to }) : c, walk, from, to))
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(doc.maps)) maps[id] = m && renameInMap(m, from, to, walk)
  next.maps = maps
  if (doc.campaign) next.campaign = walkCampaign(doc.campaign, walk)
  if (doc.supports) next.supports = { ...doc.supports, pairs: doc.supports.pairs.map((p) => renamePair(p, walk, from, to)) }
  next = followCharacterInPromotions(renameInGraphics(renameCharacterLocks(next, from, to), from, to), from, to)
  return { doc: next, error: null }
}

/** Places that spell the id in quotes inside a formula or a Lua script: the rename cannot reach them, so the maker is told where to look. */
export function formulaMentions(doc: ProjectDoc, id: string): string[] {
  const quoted = (s: string) => s.includes(`"${id}"`) || s.includes(`'${id}'`)
  const texts = (v: unknown, out: string[] = []): string[] => {
    if (typeof v === 'string') out.push(v)
    else if (Array.isArray(v)) v.forEach((x) => texts(x, out))
    else if (v && typeof v === 'object') Object.values(v).forEach((x) => texts(x, out))
    return out
  }
  const lists: Array<[string, Array<{ name?: string; id: string }> | null | undefined]> = [
    ['Skill', doc.skills], ['Ability', doc.abilities], ['Condition', doc.conditions], ['Weapon', doc.weapons], ['Item', doc.items], ['Unit type', doc.units],
  ]
  const out: string[] = []
  for (const [what, list] of lists)
    for (const e of list ?? []) if (texts(e).some(quoted)) out.push(`${what} “${e.name ?? e.id}” has a formula that spells “${id}” in quotes`)
  if (doc.game?.formulas && texts(doc.game.formulas).some(quoted)) out.push(`The rules’ formulas spell “${id}” in quotes`)
  for (const [file, text] of Object.entries(doc.scripts ?? {})) if (quoted(text)) out.push(`The script ${file} spells “${id}” in quotes`)
  return out
}

/** What the rename will change, as plain lines for the dialog (nothing is listed for a kind of place that does not name it). */
export function renameEffects(doc: ProjectDoc, id: string): string[] {
  const out: string[] = []
  const probe = `${id}__rename`
  const after = renameCharacter(doc, id, probe).doc  // a throwaway name: only what differs is counted
  // The placed unit itself is listed on its own; everything else of a map is compared with the `character` key left out.
  const rest = (m: MapFile | null | undefined) => JSON.stringify(m && { ...m, units: m.units?.map((u) => (u.character === id || u.character === probe ? { ...u, character: '' } : u)) })
  for (const [mid, m] of mapEntries(doc)) {
    if (m.units?.some((u) => u.character === id)) out.push(`The map “${mapName(doc, mid)}” places it`)
    if (rest(after.maps[mid]) !== rest(m)) out.push(`The map “${mapName(doc, mid)}”: its events, objectives, preparation lists, visits and behaviour that name it follow`)
  }
  const changed = (a: unknown, b: unknown) => JSON.stringify(a) !== JSON.stringify(b)
  if (changed(after.campaign, doc.campaign)) out.push('The campaign: scenes that name it follow')
  if (changed(after.supports, doc.supports)) out.push('Supports: its pairs and their conversations follow')
  if (changed(after.characters?.map((c) => c.quotes), doc.characters?.map((c) => c.quotes))) out.push('Characters: quotes against it and lines it speaks follow')
  if (changed([after.weapons, after.items], [doc.weapons, doc.items])) out.push('Weapons and staves locked to it follow')
  if (changed(after.graphics, doc.graphics)) out.push('Art: its portrait and its battle animation rules follow')
  return out
}
