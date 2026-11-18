// The computer opponent's behaviour of a unit (docs/AI.md): how it moves, when it starts, its group and a favourite target. A map unit, an `addUnit`
// command and a `setBehaviour` command all hold the same block. A block is a mode name ("hold") or an object; the editor always edits the object
// form and writes the shortest text that means the same (nothing for the default, a bare name when the mode is all there is).
import { defaultRoute, keepScripted, SCRIPTED_KEYS, scriptedProblems } from './behaviourScripted'
import { entryId } from './itemGrants'
import { namedUnits } from './storyIds'
import { placesOf } from './placeStory'
import { forEachCommand } from './storyWalk'
import type { Rect } from './storyTypes'
import type { MapFile, MapUnit, ProjectDoc, Unit } from './types'

export type BehaviourMode = 'advance' | 'hold' | 'guard' | 'guardArea' | 'seek' | 'patrol'
export const BEHAVIOUR_MODES: BehaviourMode[] = ['advance', 'hold', 'guard', 'guardArea', 'seek', 'patrol']
export type SeekKind = 'unit' | 'place' | 'square'
export type Prefer = 'weakest' | 'nearest' | { unit: string } | { type: string } | { tag: string }

export interface BehaviourObj {
  mode?: string; wake?: string; group?: string; fromTurn?: number; area?: Rect; within?: number
  unit?: string; place?: string; x?: number; y?: number; prefer?: Prefer
  /** The scripted-style keys (model/behaviourScripted.ts). */
  order?: number; weights?: string; caution?: string; retreat?: unknown; idleTurns?: number; route?: unknown; ignores?: string[]; measure?: string; heals?: string
  [k: string]: unknown
}
export type Behaviour = string | BehaviourObj

/** The loader's bounds (engine/core/data/behaviour.h). */
export const MAX_FROM_TURN = 99
export const MAX_WITHIN = 99
/** Past this round the battle has very probably ended: worth a look (not an error). */
export const SENSIBLE_FROM_TURN = 30

/** The block as an object, whatever form it was written in. A value that is not a block reads as an empty one. */
export function behaviourObj(b: unknown): BehaviourObj {
  if (typeof b === 'string') return { mode: b }
  return b !== null && typeof b === 'object' && !Array.isArray(b) ? { ...(b as BehaviourObj) } : {}
}

export const modeOf = (b: unknown): BehaviourMode => {
  const m = behaviourObj(b).mode
  return BEHAVIOUR_MODES.includes(m as BehaviourMode) ? (m as BehaviourMode) : 'advance'
}

export const seekKindOf = (o: BehaviourObj): SeekKind => (typeof o.unit === 'string' ? 'unit' : typeof o.place === 'string' ? 'place' : 'square')

/** The keys each mode keeps; everything else goes when the mode changes. `fromTurn` and `prefer` belong to every mode. */
const KEEP: Record<BehaviourMode, string[]> = {
  advance: [], hold: [], guard: ['wake', 'group'], guardArea: ['group', 'area', 'within'], seek: ['unit', 'place', 'x', 'y'], patrol: [],
}

/** The block written the short way: undefined for the default, a bare mode name when nothing else is set, else an object. */
export function tidyBehaviour(o: BehaviourObj): Behaviour | undefined {
  const mode = modeOf(o)
  const out: BehaviourObj = {}
  if (mode !== 'advance') out.mode = mode
  for (const k of KEEP[mode]) if (o[k] !== undefined && o[k] !== '' && !(k === 'wake' && o[k] === 'once')) out[k] = o[k]
  if (typeof o.fromTurn === 'number' && o.fromTurn > 1) out.fromTurn = o.fromTurn
  if (o.prefer !== undefined) out.prefer = o.prefer
  for (const k of SCRIPTED_KEYS) if (keepScripted(k, o[k], mode)) (out as Record<string, unknown>)[k] = o[k]
  // Keys the editor does not know stay (a hand-edited file keeps them; the loader says if they are wrong).
  for (const k of Object.keys(o)) if (!KNOWN.has(k)) out[k] = o[k]
  const keys = Object.keys(out)
  if (keys.length === 0) return undefined
  if (keys.length === 1 && out.mode !== undefined && (mode === 'hold' || mode === 'guard')) return mode
  return out
}
const KNOWN = new Set(['mode', 'wake', 'group', 'fromTurn', 'area', 'within', 'unit', 'place', 'x', 'y', 'prefer', ...SCRIPTED_KEYS])

export const isDefaultBehaviour = (b: unknown): boolean => tidyBehaviour(behaviourObj(b)) === undefined

export interface BehaviourContext {
  /** Story names of the units a unit may head for, and the placed structures that have a story name. */
  units: string[]
  places: string[]
  /** A square on the map (a default to head for). */
  width: number
  height: number
}

export function contextOf(doc: ProjectDoc, map: MapFile): BehaviourContext {
  return { units: namedUnits(doc, map).map((u) => u.id), places: placesOf(doc, map).map((p) => p.id), width: map.grid.width, height: map.grid.height }
}

/** Switches the mode, giving the new mode sensible starting values; `fromTurn` and `prefer` carry over. */
export function withMode(b: unknown, mode: BehaviourMode, ctx: BehaviourContext): BehaviourObj {
  const old = behaviourObj(b)
  const o: BehaviourObj = { mode }
  if (old.fromTurn !== undefined) o.fromTurn = old.fromTurn
  if (old.prefer !== undefined) o.prefer = old.prefer
  for (const k of SCRIPTED_KEYS) if (old[k] !== undefined && k !== 'route' && k !== 'measure') (o as Record<string, unknown>)[k] = old[k]  // they belong to every mode (route and measure to one each)
  if (mode === old.mode) return { ...old }
  if (mode === 'guard' || mode === 'guardArea') if (old.group !== undefined) o.group = old.group
  if (mode === 'guardArea') o.within = 3
  if (mode === 'patrol') o.route = defaultRoute(ctx.width, ctx.height)
  if (mode === 'seek') {
    if (ctx.places.length) o.place = ctx.places[0]
    else if (ctx.units.length) o.unit = ctx.units[0]
    else { o.x = Math.max(0, Math.floor(ctx.width / 2)); o.y = Math.max(0, Math.floor(ctx.height / 2)) }
  }
  return o
}

/** Switches what a seeker heads for. */
export function withSeekKind(b: unknown, kind: SeekKind, ctx: BehaviourContext): BehaviourObj {
  const o = behaviourObj(b)
  delete o.unit; delete o.place; delete o.x; delete o.y
  if (kind === 'unit') o.unit = ctx.units[0] ?? ''
  else if (kind === 'place') o.place = ctx.places[0] ?? ''
  else { o.x = Math.max(0, Math.floor(ctx.width / 2)); o.y = Math.max(0, Math.floor(ctx.height / 2)) }
  return o
}

/** The groups the map's units and its addUnit / setBehaviour commands name, with how many units each has (a setBehaviour block counts as one). */
export function groupCounts(map: MapFile): Map<string, number> {
  const out = new Map<string, number>()
  const add = (b: unknown) => { const g = behaviourObj(b).group; if (typeof g === 'string' && g) out.set(g, (out.get(g) ?? 0) + 1) }
  for (const u of map.units ?? []) add(u.behaviour)
  for (const e of map.events ?? []) forEachCommand(e.do, '', (c) => { if (c.addUnit && typeof c.addUnit === 'object') add(c.addUnit.behaviour) })
  return out
}

/** The group names of the map (for the suggestions of the group box). */
export const groupNames = (map: MapFile): string[] => [...groupCounts(map).keys()].sort()

/** The one-letter mark on the map canvas: H holds, G guards (a group or an area too), S seeks, T only starts late; null for the default. */
export function behaviourMark(b: unknown): string | null {
  const o = behaviourObj(b)
  switch (modeOf(o)) {
    case 'hold': return 'H'
    case 'guard': case 'guardArea': return 'G'
    case 'seek': return 'S'
    case 'patrol': return 'P'
    default: return typeof o.fromTurn === 'number' && o.fromTurn > 1 ? 'T' : null
  }
}

// --- units with a story name: renames and deletes ------------------------------------------------------------------------------

/** Where a behaviour block of this map's units names unit `id` (heads for it, prefers it). */
const namesUnit = (o: BehaviourObj, id: string): boolean => o.unit === id || (Array.isArray(o.ignores) && o.ignores.includes(id)) || (typeof o.prefer === 'object' && o.prefer !== null && (o.prefer as { unit?: string }).unit === id)

/** A unit's story name changed: the blocks that head for it or prefer it follow. */
export function renameBehaviourUnit(map: MapFile, from: string, to: string): MapFile {
  const fix = (b: unknown): unknown => {
    const o = behaviourObj(b)
    if (!namesUnit(o, from)) return b
    if (o.unit === from) o.unit = to
    if (Array.isArray(o.ignores)) o.ignores = o.ignores.map((i) => (i === from ? to : i))
    if (typeof o.prefer === 'object' && o.prefer !== null && (o.prefer as { unit?: string }).unit === from) o.prefer = { unit: to }
    return o
  }
  let changed = false
  const units = (map.units ?? []).map((u) => { const nb = fix(u.behaviour); if (nb === u.behaviour) return u; changed = true; return { ...u, behaviour: nb as Behaviour } })
  return changed ? { ...map, units } : map
}

/** A unit is deleted: a unit that headed for it goes back to advancing; a preference for it is dropped. */
export function dropBehaviourUnit(map: MapFile, id: string): MapFile {
  const fix = (b: unknown): Behaviour | undefined => {
    const o = behaviourObj(b)
    if (o.unit === id) { delete o.unit; o.mode = 'advance' }
    if (Array.isArray(o.ignores)) o.ignores = o.ignores.filter((i) => i !== id)
    if (typeof o.prefer === 'object' && o.prefer !== null && (o.prefer as { unit?: string }).unit === id) delete o.prefer
    return tidyBehaviour(o)
  }
  let changed = false
  const units = (map.units ?? []).map((u) => {
    if (u.behaviour === undefined || !namesUnit(behaviourObj(u.behaviour), id)) return u
    changed = true
    const { behaviour: _old, ...rest } = u
    const nb = fix(u.behaviour)
    return (nb === undefined ? rest : { ...rest, behaviour: nb }) as MapUnit
  })
  return changed ? { ...map, units } : map
}

/** A structure's story name changed: seekers of it follow. */
export function renameBehaviourPlace(map: MapFile, from: string, to: string): MapFile {
  let changed = false
  const units = (map.units ?? []).map((u) => {
    const o = behaviourObj(u.behaviour)
    if (o.place !== from) return u
    changed = true
    return { ...u, behaviour: { ...o, place: to } as Behaviour }
  })
  return changed ? { ...map, units } : map
}

// --- checks -------------------------------------------------------------------------------------------------------------------------

export interface BehaviourCheck { mapId: string; text: string; cell?: { x: number; y: number }; note?: boolean }

/** Can this unit do anything at all from where it stands (a weapon, a staff or an ability)? */
function canAct(doc: ProjectDoc, u: MapUnit): boolean {
  const type: Unit | undefined = doc.units?.find((t) => t.id === (u.character !== undefined ? doc.characters?.find((c) => c.id === u.character)?.type ?? u.type : u.type))
  if (!type) return true // unknown type: another check says so
  if (type.weapons?.length || type.abilities?.length) return true
  const person = u.character === undefined ? undefined : doc.characters?.find((c) => c.id === u.character)
  const items = u.items ?? person?.items ?? type.items ?? []
  return items.some((e) => { const it = doc.items?.find((i) => i.id === entryId(e)); return it !== undefined && (it.kind === 'weapon' || it.kind === 'staff') })
}

/** Plain problems of one block. `refs` says what exists on the map; `where` starts every sentence. */
export function blockProblems(b: unknown, doc: ProjectDoc, map: MapFile, say: (text: string, note?: boolean) => void, mine: { human?: boolean } = {}): void {
  const o = behaviourObj(b)
  const mode = modeOf(o)
  if (typeof b !== 'string' && (typeof b !== 'object' || b === null || Array.isArray(b))) { say('the behaviour is not a mode name or a block.'); return }
  if (o.mode !== undefined && !BEHAVIOUR_MODES.includes(o.mode as BehaviourMode)) say(`“${String(o.mode)}” is not a behaviour (advance, hold, guard, guardArea, seek or patrol).`)
  if (mine.human) say('a behaviour tells the computer what to do, but this unit is on a side a person plays.')
  if (o.fromTurn !== undefined) {
    if (!Number.isInteger(o.fromTurn) || o.fromTurn < 1 || o.fromTurn > MAX_FROM_TURN) say(`“does nothing before turn” must be a whole number from 1 to ${MAX_FROM_TURN}.`)
    else if (o.fromTurn > SENSIBLE_FROM_TURN) say(`it does nothing before turn ${o.fromTurn}; the battle will most likely be over by then.`, true)
  }
  if (mode === 'seek') {
    const targets = (o.unit !== undefined ? 1 : 0) + (o.place !== undefined ? 1 : 0) + (o.x !== undefined || o.y !== undefined ? 1 : 0)
    if (targets !== 1) say('“heads for” needs exactly one target: a unit, a place or a square.')
    if (typeof o.unit === 'string') {
      if (o.unit === '') say('pick the unit it heads for.')
      else if (!namedUnits(doc, map).some((u) => u.id === o.unit) && !doc.characters?.some((c) => c.id === o.unit)) say(`it heads for “${o.unit}”, which is not a named unit of this map.`)
    }
    if (typeof o.place === 'string') {
      if (o.place === '') say('pick the place it heads for.')
      else if (!placesOf(doc, map).some((p) => p.id === o.place)) say(`it heads for “${o.place}”, which is not a structure with that story name on this map.`)
    }
    if (typeof o.x === 'number' && typeof o.y === 'number' && (o.x < 0 || o.y < 0 || o.x >= map.grid.width || o.y >= map.grid.height)) say(`it heads for the square ${o.x + 1}, ${o.y + 1}, which is outside the map.`)
  }
  if (mode === 'guardArea') {
    const a = o.area
    if (a === undefined && o.within === undefined) say('“waits for someone in an area” needs an area and / or a distance.')
    if (a !== undefined && (a.x >= map.grid.width || a.y >= map.grid.height || a.x < 0 || a.y < 0)) say('its area starts outside the map.')
    if (o.within !== undefined && (!Number.isInteger(o.within) || o.within < 1 || o.within > MAX_WITHIN)) say(`the distance must be a whole number from 1 to ${MAX_WITHIN}.`)
  }
  scriptedProblems(o, mode, doc, map, say)
  const p = o.prefer
  if (typeof p === 'object' && p !== null && 'unit' in p && typeof p.unit === 'string' && p.unit !== '' && !namedUnits(doc, map).some((u) => u.id === p.unit) && !doc.characters?.some((c) => c.id === p.unit))
    say(`it prefers “${p.unit}”, which is not a named unit of this map.`)
  if (typeof p === 'object' && p !== null && 'type' in p && typeof p.type === 'string' && !doc.units?.some((t) => t.id === p.type)) say(`it prefers the unit type “${p.type}”, which this game does not have.`)
}

/** A command's sentence: what the units are told to do ("stay where they are, from turn 3"). */
export function behaviourPhrase(b: unknown): string {
  const o = behaviourObj(b)
  const mode = modeOf(o)
  let text: string
  switch (mode) {
    case 'hold': text = 'stay where they are'; break
    case 'guard': text = o.wake === 'react' ? 'only hit what comes within reach' : 'wait until someone comes into range'; break
    case 'guardArea': text = 'wait until someone comes near'; break
    case 'patrol': text = 'walk their route'; break
    case 'seek': text = typeof o.unit === 'string' ? `head for ${o.unit || 'a unit'}` : typeof o.place === 'string' ? `head for ${o.place || 'a place'}` : `head for the square ${(o.x ?? 0) + 1}, ${(o.y ?? 0) + 1}`; break
    default: text = 'advance and attack'
  }
  const g = typeof o.group === 'string' && o.group ? ` (group ${o.group})` : ''
  const t = typeof o.fromTurn === 'number' && o.fromTurn > 1 ? `, from turn ${o.fromTurn}` : ''
  return `${text}${g}${t}`
}

/** Every problem with the behaviours of the map's units, plus a group of one and a unit that holds with nothing to do. */
export function behaviourChecks(doc: ProjectDoc, mapId: string, map: MapFile): BehaviourCheck[] {
  const out: BehaviourCheck[] = []
  const sides = new Map((map.sides ?? []).map((s) => [s.id, s]))
  const counts = groupCounts(map)
  for (const u of map.units ?? []) {
    if (u.behaviour === undefined) continue
    const where = `The unit at ${u.x + 1}, ${u.y + 1}`
    const cell = { x: u.x, y: u.y }
    const say = (text: string, note?: boolean) => out.push({ mapId, cell, note, text: `${where}: ${text}` })
    const side = sides.get(u.side)
    blockProblems(u.behaviour, doc, map, say, { human: side !== undefined && side.controller !== 'ai' })
    const o = behaviourObj(u.behaviour)
    if (typeof o.group === 'string' && o.group && counts.get(o.group) === 1) say(`it is the only unit in the group “${o.group}”, so waking it wakes nobody else.`, true)
    if (modeOf(o) === 'hold' && !canAct(doc, u)) say('it holds its square but has no weapon, staff or ability, so it can never do anything.', true)
  }
  return out
}
