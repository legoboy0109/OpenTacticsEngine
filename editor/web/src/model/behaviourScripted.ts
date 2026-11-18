// The keys of a behaviour block that make a computer player act like a scripted one (docs/AI.md "Scripted-style computer players"): order, weights, caution,
// retreat, idleTurns, route (mode patrol), ignores and measure (mode guardArea). behaviour.ts keeps the block together; this file knows these keys: which
// ones are written, what the loader would refuse (the checks mirror engine/core/data/behaviour_scripted.cpp and loader_behaviour.cpp), and the starting values.
import { aiRules } from './aiRules'
import { namedUnits } from './storyIds'
import type { MapFile, ProjectDoc } from './types'

export const MAX_ORDER = 100000
export const MAX_IDLE_TURNS = 99
export const MAX_ROUTE = 32
export const MIN_ROUTE = 2
export const MAX_IGNORES = 32

export interface Retreat { below?: number; until?: number; to?: { terrain?: string[] }; useItems?: boolean; [k: string]: unknown }
export interface RouteSquare { x: number; y: number }
export type ScriptedObj = Record<string, unknown>

/** The keys, in the order they are written. */
export const SCRIPTED_KEYS = ['order', 'weights', 'caution', 'retreat', 'idleTurns', 'route', 'ignores', 'measure'] as const
export const isScriptedKey = (k: string): boolean => (SCRIPTED_KEYS as readonly string[]).includes(k)

const isInt = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

/** Is this key written for this mode and value? (A default is never written; `route` is for patrol and `measure` for guardArea only.) */
export function keepScripted(k: string, v: unknown, mode: string): boolean {
  switch (k) {
    case 'order': return v !== undefined && v !== 0
    case 'weights': return typeof v === 'string' && v !== ''
    case 'caution': return v === 'none'
    case 'retreat': return v !== undefined && v !== null
    case 'idleTurns': return typeof v === 'number' && v > 0
    case 'route': return mode === 'patrol' && Array.isArray(v)
    case 'ignores': return Array.isArray(v) && v.length > 0
    case 'measure': return mode === 'guardArea' && v === 'moveCost'
    default: return false
  }
}

/** A starting route for a patrol: two squares in the middle row, a third of the map from each side. */
export function defaultRoute(width: number, height: number): RouteSquare[] {
  const y = Math.max(0, Math.floor(height / 2))
  const a = Math.min(width - 1, Math.floor(width / 3))
  let b = Math.min(width - 1, Math.max(a, Math.floor((2 * width) / 3)))
  let by = y
  if (b === a) by = y === 0 ? Math.min(height - 1, 1) : y - 1
  return [{ x: a, y }, { x: b, y: by }]
}

/** The starting retreat block: the limits of the docs' example, no terrain (just away from the enemy). */
export const defaultRetreat = (): Retreat => ({ below: 30, until: 80 })

/** Names of the weight sets the game has. */
export const weightNames = (doc: ProjectDoc): string[] => Object.keys(aiRules(doc).weights)

/** What the loader would refuse (or what is probably not meant) about the scripted keys of one block. `say(text, note)`: a note is only worth a look. */
export function scriptedProblems(o: ScriptedObj, mode: string, doc: ProjectDoc, map: MapFile, say: (text: string, note?: boolean) => void): void {
  if (o.order !== undefined) {
    if (!isInt(o.order) || Math.abs(o.order) > MAX_ORDER) say(`“acts in this order” must be a whole number from -${MAX_ORDER} to ${MAX_ORDER}.`)
    else if (o.order !== 0 && aiRules(doc).turnOrder !== 'order') say('it has an acting order, but the game’s computer player plays its best action first (Rules, Computer player), so the number does nothing yet.', true)
  }
  if (o.weights !== undefined) {
    if (typeof o.weights !== 'string' || o.weights === '') say('pick the set of attack weights it uses.')
    else if (!weightNames(doc).includes(o.weights)) say(`it uses the attack weights “${o.weights}”, which the game does not have (Rules, Computer player).`)
  }
  if (o.caution !== undefined && o.caution !== 'normal' && o.caution !== 'none') say('“caution” must be normal or none.')
  if (o.idleTurns !== undefined && (!isInt(o.idleTurns) || o.idleTurns < 1 || o.idleTurns > MAX_IDLE_TURNS)) say(`“idle turns” must be a whole number from 1 to ${MAX_IDLE_TURNS}.`)
  if (o.retreat !== undefined) retreatProblems(o.retreat, mode, doc, say)
  if (o.route !== undefined && mode !== 'patrol') say('a route goes with “walks a route” only.')
  if (mode === 'patrol') routeProblems(o.route, map, say)
  if (o.ignores !== undefined) {
    if (!Array.isArray(o.ignores) || o.ignores.length > MAX_IGNORES) say(`“never chases” must be a list of at most ${MAX_IGNORES} units.`)
    else {
      const known = new Set(namedUnits(doc, map).map((u) => u.id))
      for (const id of o.ignores) {
        if (typeof id !== 'string' || id === '') say('“never chases” must be a list of unit names.')
        else if (!known.has(id) && !doc.characters?.some((c) => c.id === id)) say(`it never chases “${id}”, which is not a named unit of this map.`)
      }
    }
  }
  if (o.measure !== undefined) {
    if (mode !== 'guardArea' || o.within === undefined) say('“measured by” goes with “waits until someone comes near” and a distance.')
    else if (o.measure !== 'steps' && o.measure !== 'moveCost') say('“measured by” must be squares or walking cost.')
  }
}

function retreatProblems(r: unknown, mode: string, doc: ProjectDoc, say: (text: string, note?: boolean) => void): void {
  if (typeof r !== 'object' || r === null || Array.isArray(r)) { say('“retreats to heal” must have a limit to fall back at and one to come back at.'); return }
  const o = r as Retreat
  const below = o.below
  const until = o.until ?? 100
  if (!isInt(below) || below < 1 || below > 99) say('the retreat limit (“falls back below”) must be a whole number from 1 to 99 percent.')
  if (o.until !== undefined && (!isInt(o.until) || o.until < 2 || o.until > 100)) say('“comes back at” must be a whole number from 2 to 100 percent.')
  else if (isInt(below) && isInt(until) && until <= below) say(`“comes back at” (${until}) must be more than “falls back below” (${below}).`)
  if (o.useItems !== undefined && typeof o.useItems !== 'boolean') say('“drinks a healing item first” must be on or off.')
  if (o.to !== undefined) {
    const terrain = typeof o.to === 'object' && o.to !== null ? o.to.terrain : undefined
    if (!Array.isArray(terrain) || terrain.length === 0) say('“falls back to” needs at least one kind of terrain, or leave it off to fall back away from the enemy.')
    else for (const t of terrain) if (typeof t !== 'string' || !doc.terrain?.some((x) => x.id === t)) say(`it falls back to the terrain “${String(t)}”, which this game does not have.`)
  }
  if (mode === 'hold') say('it holds its square, so a retreat can only make it drink a healing item.', true)
}

function routeProblems(route: unknown, map: MapFile, say: (text: string, note?: boolean) => void): void {
  if (!Array.isArray(route) || route.length < MIN_ROUTE || route.length > MAX_ROUTE) { say(`a route needs ${MIN_ROUTE} to ${MAX_ROUTE} squares.`); return }
  const sq = route as Array<Partial<RouteSquare>>
  sq.forEach((s, i) => {
    if (!isInt(s?.x) || !isInt(s?.y) || s.x < 0 || s.y < 0) say(`route square ${i + 1} needs a column and a row.`)
    else if (s.x >= map.grid.width || s.y >= map.grid.height) say(`route square ${i + 1} (${s.x + 1}, ${s.y + 1}) is outside the map.`)
  })
  for (let i = 0; i < sq.length; i++) {
    const a = sq[i]
    const b = sq[(i + 1) % sq.length]
    if (a?.x === b?.x && a?.y === b?.y) { say(`the route has the same square twice in a row (${(a?.x ?? 0) + 1}, ${(a?.y ?? 0) + 1}; the last square is followed by the first).`); break }
  }
}
