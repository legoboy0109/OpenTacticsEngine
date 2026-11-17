// Checks that mirror what the game loader refuses about doors, chests, places and keys (engine/core/data/loader_places.cpp,
// loader_places_story.cpp, loader_items.cpp, loader_abilities.cpp), so a maker sees them before pressing Play, plus a few notes the loader
// does not give ("worth a look": a lock nothing can open, a key that opens nothing placed, a computer unit sent after a place the map does
// not have). The wording is plain; the loader's own messages still show in the Problems list after a save.
import { DOOR_HP_MAX, WAYS, blocksSomeone, fittingKeys, isKeyItem, keyFits, lockedByDefault, placeKindOf, sameFootprint, waysOf, type PlaceKind } from './places'
import { placeProblem } from './placeStory'
import { isStoryId } from './storyIds'
import { forEachCommand } from './storyWalk'
import { PLACE_COMMANDS, PLACE_EVENTS, commandKind, type CommandKind, type PlaceEvent } from './storyTypes'
import { abilityIsGiven } from './abilityChecks'
import { fxList } from './staffFx'
import type { Item, MapFile, MapStructure, MapUnit, ProjectDoc, Section, Structure } from './types'

export interface PlaceCheck { section: Section; text: string; mapId?: string; cell?: { x: number; y: number }; /** Loads fine but is probably not what was meant. */ note?: boolean }

const BLOCKS: PlaceKind[] = ['door', 'chest', 'visit']
const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v)
const dupes = (list: string[]): string[] => list.filter((x, i) => list.indexOf(x) !== i)
const q = (s: string) => `“${s}”`

// --- structure kinds -----------------------------------------------------------------------------------------------------------

function kindChecks(doc: ProjectDoc, s: Structure, say: (t: string, note?: boolean) => void): void {
  const present = BLOCKS.filter((k) => (s as Record<string, unknown>)[k] !== undefined)
  if (present.length > 1) say('it is more than one of a door, a chest and a place that can be visited. A structure is only one of them.')
  if (s.blocksSight !== undefined && typeof s.blocksSight !== 'boolean') say('“blocks sight” must be on or off.')
  for (const k of present) {
    const b = (s as Record<string, unknown>)[k]
    if (!isObj(b)) { say(`the ${k} settings are not in a form the game can read.`); continue }
    const kindRef = (key: string, what: string, required: boolean): void => {
      const v = b[key]
      if (v === undefined) { if (required) say(`a ${k} needs ${what}.`); return }
      const t = typeof v === 'string' ? doc.structures?.find((x) => x.id === v) : undefined
      if (!t) say(`${what} “${String(v)}” is not a structure kind of this game.`)
      else if (t.id === s.id) say(`${what} is the structure itself. It must become another kind.`)
      else if (!sameFootprint(s, t)) say(`${what} ${t.name} has a different size. The two looks must cover the same squares.`)
    }
    if (k === 'visit') {
      kindRef('becomes', 'what it becomes after a visit', false)
      kindRef('ruin', 'what it becomes when destroyed', false)
      if (b.repeat !== undefined && typeof b.repeat !== 'boolean') say('“can be visited again and again” must be on or off.')
      if (b.repeat === true && b.becomes !== undefined) say('a place that changes after a visit cannot also be visited again and again. Choose one.')
    } else {
      kindRef('opens', 'what it becomes when opened', true)
      if (b.locked !== undefined && typeof b.locked !== 'boolean') say('“starts locked” must be on or off.')
      doorChest(doc, s, k, b, say)
    }
    if (b.from !== undefined && b.from !== 'on' && b.from !== 'next' && b.from !== 'either') say('where a unit stands to use it must be on it, next to it, or either.')
  }
}

function doorChest(doc: ProjectDoc, s: Structure, k: 'door' | 'chest', b: Record<string, unknown>, say: (t: string, note?: boolean) => void): void {
  const hp = b.hp
  if (k === 'door' && hp !== undefined && (typeof hp !== 'number' || !Number.isInteger(hp) || hp < 1 || hp > DOOR_HP_MAX)) say(`its hit points must be a whole number from 1 to ${DOOR_HP_MAX} (or switch “Breaking it down” off).`)
  if (k === 'door' && b.stats !== undefined) {
    if (hp === undefined) say('it has numbers for its damage but no hit points. Give it hit points or clear the numbers.')
    const known = new Set((doc.game?.stats ?? []).map((x) => x.id))
    for (const id of Object.keys(isObj(b.stats) ? b.stats : {})) if (!known.has(id)) say(`its damage numbers name ${q(id)}, which is not a stat of this game.`)
  }
  if (b.by !== undefined) {
    if (!Array.isArray(b.by)) say('what can open it must be a list of key, trait and break.')
    else {
      for (const w of b.by) if (!WAYS.includes(w as never) || (w === 'break' && k === 'chest')) say(`${q(String(w))} is not a way to open ${k === 'door' ? 'a door' : 'a chest'}.`)
      if ((b.by as unknown[]).includes('break') && hp === undefined) say('“Breaking it down” needs hit points.')
    }
  }
  if (k === 'door' && !blocksSomeone(s)) say('a door must stop at least one kind of unit. Under “Moving through it”, set “Cannot enter” for the kinds it should stop.')
  const keys = b.keys
  if (keys !== undefined) {
    if (!Array.isArray(keys)) { say('the list of keys is not in a form the game can read.'); return }
    if (!waysOf(s).includes('key')) say('it lists keys, but “A key” is not one of the ways to open it.')
    const ids = keys.map(String)
    for (const id of dupes(ids)) say(`the key ${q(id)} is listed twice.`)
    for (const id of ids) {
      const it = doc.items?.find((x) => x.id === id)
      if (!it) say(`the key ${q(id)} is not an item of this game.`)
      else if (!isKeyItem(it)) say(`${it.name} is listed as a key but opens nothing. Give it “It opens” on the Items screen.`)
      else if (!keyFits(it, s)) say(`${it.name} is listed as a key but does not open ${s.name} (on the Items screen, it must open any ${k} or this structure).`)
    }
  }
}

// --- what can open a locked door or chest ----------------------------------------------------------------------------------------

function unlockCovers(v: unknown, k: 'door' | 'chest'): boolean {
  if (v === true) return true
  if (Array.isArray(v)) return v.includes(k)
  if (isObj(v)) return v.kinds === undefined || (Array.isArray(v.kinds) && v.kinds.includes(k))
  return false
}
const abilityOpens = (doc: ProjectDoc, k: 'door' | 'chest'): boolean =>
  (doc.abilities ?? []).some((a) => isObj(a.effect) && 'unlock' in a.effect && unlockCovers(a.effect.unlock, k) && abilityIsGiven(doc, a.id))
const staffOpens = (doc: ProjectDoc, k: 'door' | 'chest'): boolean =>
  (doc.items ?? []).some((i) => i.kind === 'staff' && fxList(i).some((e) => isObj(e) && 'unlock' in e && unlockCovers(e.unlock, k)))

/** Can anything in the game open this kind (without counting events)? The ways its settings allow, and what exists. */
export function kindCanBeOpened(doc: ProjectDoc, s: Structure): boolean {
  const k = placeKindOf(s)
  if (k !== 'door' && k !== 'chest') return true
  const ways = waysOf(s)
  const keys = fittingKeys(doc, s).filter((i) => !(s[k]!.keys?.length) || s[k]!.keys!.includes(i.id))
  return (ways.includes('key') && keys.length > 0) || (ways.includes('trait') && abilityOpens(doc, k)) || ways.includes('break') || staffOpens(doc, k)
}

function eventsOpen(map: MapFile, id: string): boolean {
  let found = false
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { if (c.openDoor === id || c.unlockDoor === id || c.openChest === id) found = true })
  return found
}

// --- keys ------------------------------------------------------------------------------------------------------------------------

function keyChecks(doc: ProjectDoc, i: Item, say: (t: string, note?: boolean) => void): void {
  if (i.opens === undefined) return
  if (i.kind !== 'usable') { say('only a usable item can be a key.'); return }
  if (!Array.isArray(i.opens) || i.opens.length === 0) { say('“It opens” needs at least one of: any door, any chest, or a structure kind.'); return }
  for (const w of i.opens) {
    if (w === 'door' || w === 'chest') continue
    const t = doc.structures?.find((x) => x.id === w)
    if (!t) say(`it opens ${q(String(w))}, which is neither “any door”, “any chest” nor a structure kind of this game.`)
    else if (placeKindOf(t) !== 'door' && placeKindOf(t) !== 'chest') say(`it opens ${t.name}, which is not a door or a chest.`, true)
  }
  // Worth a look: a key for nothing that is placed anywhere.
  const kinds = (doc.structures ?? []).filter((s) => keyFits(i, s))
  const placed = Object.values(doc.maps).some((m) => m?.structures?.some((p) => kinds.some((s) => s.id === p.type)))
  if (!placed && i.opens.every((w) => w === 'door' || w === 'chest' || doc.structures?.some((x) => x.id === w))) say('it is a key, but no door or chest it opens is placed on any map yet.', true)
}

// --- placed structures -----------------------------------------------------------------------------------------------------------

function placedChecks(doc: ProjectDoc, mapId: string, map: MapFile, out: PlaceCheck[]): void {
  const ids = new Map<string, number>()
  for (const p of map.structures ?? []) if (typeof p.id === 'string' && p.id !== '') ids.set(p.id, (ids.get(p.id) ?? 0) + 1)
  const sideIds = new Set((map.sides ?? []).map((x) => x.id))
  const unitIds = new Set([...(map.units ?? []).map((u) => u.id).filter((x): x is string => !!x), ...(doc.characters ?? []).map((c) => c.id)])
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { if (c.addUnit?.id) unitIds.add(c.addUnit.id) })
  for (const p of map.structures ?? []) {
    const def = doc.structures?.find((x) => x.id === p.type)
    const k = placeKindOf(def)
    const at = { x: p.x, y: p.y }
    const where = `${def?.name ?? p.type} at ${p.x + 1}, ${p.y + 1}`
    const say = (text: string, note?: boolean) => out.push({ section: 'maps', mapId, cell: at, note, text: `${where}: ${text}` })
    if (p.id !== undefined) {
      if (typeof p.id !== 'string' || !isStoryId(p.id)) say('its story name may use only letters, digits, _ and - (up to 64).')
      else if ((ids.get(p.id) ?? 0) > 1) say(`the story name ${q(p.id)} is used by more than one structure of the map.`)
    }
    if (p.contents !== undefined) {
      if (k !== 'chest') say('it has contents, but only a chest holds things.')
      else contentsChecks(doc, p, say)
    } else if (k === 'chest') say('a chest needs contents: add what is inside (the game will not load without).')
    if (p.locked !== undefined && k !== 'door' && k !== 'chest') say('“locked” only belongs to a door or a chest.')
    if (p.visitSides !== undefined || p.visitUnits !== undefined) {
      if (k !== 'visit') say('it lists who may visit, but it is not a place that can be visited.')
      for (const s of p.visitSides ?? []) if (!sideIds.has(s)) say(`it lets side ${q(s)} visit, which is not a side of this map.`)
      for (const u of p.visitUnits ?? []) if (!unitIds.has(u)) say(`it lets ${q(u)} visit, which is neither a named unit of this map nor a character.`)
    }
    if (def && (k === 'door' || k === 'chest')) {
      const locked = p.locked !== undefined ? p.locked : lockedByDefault(def)
      if (locked && !kindCanBeOpened(doc, def) && !(typeof p.id === 'string' && eventsOpen(map, p.id)))
        say(`it is locked, but nothing in the game can open it (no key, unlock ability or staff that fits, no way to break it, and no event that opens it).`, true)
    }
  }
}

function contentsChecks(doc: ProjectDoc, p: MapStructure, say: (t: string, note?: boolean) => void): void {
  const list = p.contents
  if (!Array.isArray(list)) { say('its contents are not in a form the game can read.'); return }
  if (list.length === 0) say('a chest with nothing in it (the game will not load it). Add something, or remove the chest.')
  if (list.length > 8) say(`it holds ${list.length} things, but a chest holds at most 8.`)
  list.forEach((e, n) => {
    const at = `thing ${n + 1}`
    if (typeof e === 'string' || (isObj(e) && 'item' in e)) {
      const id = typeof e === 'string' ? e : String((e as { item: unknown }).item)
      const def = doc.items?.find((x) => x.id === id)
      if (!def) { say(`${at} is ${q(id)}, which is not an item of this game.`); return }
      const uses = isObj(e) ? (e as { uses?: unknown }).uses : undefined
      if (uses !== undefined) {
        if (def.uses === undefined) say(`${at}: ${def.name} has unlimited uses, so “uses left” cannot be set.`)
        else if (typeof uses !== 'number' || !Number.isInteger(uses) || uses < 1 || uses > def.uses) say(`${at}: ${def.name} has ${def.uses} uses, so “uses left” must be from 1 to ${def.uses}.`)
      }
    } else if (isObj(e) && 'resource' in e) {
      const id = String(e.resource)
      if (!doc.game?.resources?.some((r) => r.id === id)) say(`${at} is ${q(id)}, which is not a resource of this game.`)
      const n2 = e.amount
      if (typeof n2 !== 'number' || !Number.isInteger(n2) || n2 < 1) say(`${at}: the amount must be a whole number of 1 or more.`)
    } else say(`${at} is not an item or an amount of a resource.`)
  })
}

// --- a map's events and computer units ------------------------------------------------------------------------------------------

const PLACE_CMD_SET = new Set<CommandKind>(PLACE_COMMANDS)

function eventChecks(doc: ProjectDoc, mapId: string, map: MapFile, out: PlaceCheck[]): void {
  const say = (where: string, text: string) => out.push({ section: 'maps', mapId, text: `Event ${where}: ${text}` })
  for (const e of map.events ?? []) {
    const t = e.when ?? {}
    if (t.place !== undefined) {
      const is = PLACE_EVENTS.includes(t.is as PlaceEvent) ? (t.is as PlaceEvent) : undefined
      if (!is) say(`${q(e.id)}, “When”`, 'say what happens to it: opened, visited or destroyed.')
      else { const p = placeProblem(doc, map, String(t.place), { is }); if (p) say(`${q(e.id)}, “When”`, p) }
    }
    forEachCommand(e.do, `${q(e.id)}`, (c, where) => {
      const kind = commandKind(c)
      if (!kind || !PLACE_CMD_SET.has(kind)) return
      const p = placeProblem(doc, map, String((c as Record<string, unknown>)[kind] ?? ''), { command: kind })
      if (p) say(where, p)
    })
  }
}

const GOALS: Array<{ key: 'chests' | 'villages' | 'doors'; kinds: PlaceKind; words: string }> = [
  { key: 'chests', kinds: 'chest', words: 'chest' }, { key: 'villages', kinds: 'visit', words: 'place that can be visited' }, { key: 'doors', kinds: 'door', words: 'door' },
]

function goalChecks(doc: ProjectDoc, mapId: string, map: MapFile, out: PlaceCheck[]): void {
  const present = new Set((map.structures ?? []).map((p) => placeKindOf(doc.structures?.find((x) => x.id === p.type))))
  const sides = new Map((map.sides ?? []).map((s) => [s.id, s]))
  // Placed units, then the units "Add a unit" commands bring in (the same rules; a command has no square on the map to point at).
  const units: Array<{ u: MapUnit; where: string; cell?: { x: number; y: number } }> = (map.units ?? []).map((u) => ({ u, where: `The unit at ${u.x + 1}, ${u.y + 1}`, cell: { x: u.x, y: u.y } }))
  for (const e of map.events ?? []) forEachCommand(e.do, `${q(e.id)}`, (c, w) => { if (c.addUnit) units.push({ u: c.addUnit, where: `Event ${w}, the unit it adds` }) })
  for (const { u, where, cell } of units) {
    if (u.goesFor === undefined) continue
    const say = (text: string, note?: boolean) => out.push({ section: 'maps', mapId, cell, note, text: `${where}: ${text}` })
    if (!Array.isArray(u.goesFor) || u.goesFor.length === 0) { say('“goes for” needs at least one of chests, villages, doors, steal (or remove it).'); continue }
    const side = sides.get(u.side)
    if (side && side.controller !== 'ai') say('“goes for” tells the computer what to do, but this unit is on a side a person plays.')
    for (const w of u.goesFor) {
      const g = GOALS.find((x) => x.key === w)
      if (!g && w !== 'steal') say(`${q(String(w))} is not something a unit can go for.`)
      else if (g && !present.has(g.kinds)) say(`it goes for ${g.key}, but this map has no ${g.words}.`, true)
    }
  }
}

// --- the lot ----------------------------------------------------------------------------------------------------------------------

export function placeChecks(doc: ProjectDoc): PlaceCheck[] {
  const out: PlaceCheck[] = []
  for (const s of doc.structures ?? []) {
    kindChecks(doc, s, (text, note) => out.push({ section: 'structures', text: `${s.name}: ${text}`, note }))
    const k = placeKindOf(s)
    if ((k === 'door' || k === 'chest') && lockedByDefault(s) && !kindCanBeOpened(doc, s) && !anyEventOpens(doc, s))
      out.push({ section: 'structures', note: true, text: `${s.name}: it starts locked, but nothing in the game can open it (no key, unlock ability or staff that fits, no way to break it down, no event). Use the “It opens” part on a key item, or tick another way.` })
  }
  for (const i of doc.items ?? []) keyChecks(doc, i, (text, note) => out.push({ section: 'items', text: `${i.name}: ${text}`, note }))
  const actions = doc.game?.rules?.places?.actions
  if (actions !== undefined && actions !== 'menu' && actions !== 'auto') out.push({ section: 'rules', text: 'Doors, chests and places: “how they are used” must be Menu or Automatic.' })
  for (const [mapId, m] of Object.entries(doc.maps)) {
    if (!m) continue
    placedChecks(doc, mapId, m, out)
    eventChecks(doc, mapId, m, out)
    goalChecks(doc, mapId, m, out)
  }
  return out
}

/** Does any map event open or unlock a place of this kind (so a locked one is not hopeless)? */
function anyEventOpens(doc: ProjectDoc, s: Structure): boolean {
  return Object.values(doc.maps).some((m) => !!m?.structures?.some((p) => p.type === s.id && typeof p.id === 'string' && eventsOpen(m, p.id)))
}
