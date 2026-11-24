// Guests (loader: map units and story "addUnit"): "guest": true marks a unit on a side a person plays that the computer moves. It does not
// keep its side in the battle and never joins the party. A guest may have a "behaviour" and a "difficulty" (a human-side unit may not
// otherwise). The story command { "guestJoins": <unit filter> } turns the matching guests into the player's ordinary units.
import { forEachCommand } from './storyWalk'
import { sectionsOf } from './storyShowMap'
import { patchObj } from './util'
import type { Command } from './storyTypes'
import type { MapFile, MapUnit, ProjectDoc } from './types'

export const isGuest = (u: Pick<MapUnit, 'guest'>): boolean => u.guest === true

/** Does the computer move this unit (so a behaviour and a difficulty are fine)? A unit of a computer side, or a guest on a human side. */
export const computerMoves = (map: MapFile, u: Pick<MapUnit, 'side' | 'guest'>): boolean =>
  map.sides?.find((s) => s.id === u.side)?.controller === 'ai' || isGuest(u)

/** Turns "guest" on or off (off removes the key). */
export const setGuest = <T extends object>(u: T, on: boolean): T => patchObj(u, { guest: on ? true : undefined })

/** The game's "rules": { "actionPoints" } switches guests off (the engine refuses a guest in such a game). */
const gameHasActionPoints = (doc: ProjectDoc): boolean => (doc.game?.rules as Record<string, unknown> | undefined)?.actionPoints !== undefined

export interface GuestCheck { text: string; cell?: { x: number; y: number } }

/** What the loader would refuse about guests on one map: plain sentences that start with where the unit is. `cell` marks a placed unit. */
export function guestChecks(doc: ProjectDoc, map: MapFile): GuestCheck[] {
  const out: GuestCheck[] = []
  const check = (u: MapUnit, where: string, cell?: { x: number; y: number }) => {
    if (u.guest === undefined) return
    if (typeof u.guest !== 'boolean') { out.push({ text: `${where}: “guest” must be on or off.`, cell }); return }
    if (!u.guest) return
    const side = map.sides?.find((s) => s.id === u.side)
    if (side?.controller === 'ai') out.push({ text: `${where}: a guest is a unit on a side a person plays, but the side “${side.name || side.id}” is the computer’s, so its units are the computer’s already. Untick Guest, or put the unit on a side a person plays.`, cell })
    if (gameHasActionPoints(doc)) out.push({ text: `${where}: guests are not available in a game with action points. Untick Guest.`, cell })
  }
  for (const u of map.units ?? []) check(u, `The unit at ${u.x + 1}, ${u.y + 1}`, { x: u.x, y: u.y })
  for (const e of map.events ?? [])
    forEachCommand(e.do, `Event “${e.id}”`, (c, w) => { if (c.addUnit && !sectionsOf(null, e.do).of.has(c)) check(c.addUnit, `${w}, the unit it adds`) })
  return out
}

/** "guestJoins" takes only the unit filter (a command's "if" is not a key of its own). */
export function guestJoinsChecks(c: Command): string[] {
  if (c.guestJoins === undefined) return []
  if (typeof c.guestJoins !== 'object' || c.guestJoins === null) return ['pick which guests join the party.']
  const extra = Object.keys(c).filter((k) => k !== 'guestJoins' && k !== 'if')
  return extra.length ? [`“a guest joins the party” takes only the unit filter; remove ${extra.map((k) => `“${k}”`).join(', ')}.`] : []
}
