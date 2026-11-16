// Weapon ranks and stat caps (docs/SRPG.md section 7): the edits behind the Rules sections and the rank / cap pickers of units,
// characters, weapons and placed units. Pure functions on the document. What deleting or renaming a rank, a group or a stat does to
// the things that name it is in `rankRefs.ts`.
import { carriedWeapons } from './itemRules'
import { cleanAllCaps, cleanAllRanks, rankRefsOnRemove, renameRankRefs } from './rankRefs'
import type { Caps, Rank, RankGrowth, RankMap, RankValue, Ranks } from './srpgTypes'
import type { NumMap, ProjectDoc } from './types'
import { makeId } from './ids'
import { patchObj } from './util'

export const MAX_RANKS = 12
export const MAX_RANK_XP = 9999
export const MAX_USE_XP = 99
export const MAX_CAP = 99999
/** What a weapon without its own `rankXp` earns per use. */
export const DEFAULT_USE_XP = 1

/** The owner's default ladder (2026-11-16): E to S. The experience numbers are placeholders until the owner picks them. */
export const DEFAULT_LADDER: Rank[] = [
  { id: 'e', name: 'E', xp: 0 }, { id: 'd', name: 'D', xp: 30 }, { id: 'c', name: 'C', xp: 70 },
  { id: 'b', name: 'B', xp: 120 }, { id: 'a', name: 'A', xp: 180 }, { id: 's', name: 'S', xp: 250 },
]

// --- reading ---------------------------------------------------------------------------------------------------------

export const ladderOf = (doc: ProjectDoc): Rank[] => doc.ranks?.ranks ?? []
/** The feature is on in the editor: the file exists (even with an empty ladder; the engine treats that as off). */
export const ranksOn = (doc: ProjectDoc): boolean => doc.ranks !== null
/** True once there is a ladder to pick from (weapons, classes and characters only show their rank pickers then). */
export const hasLadder = (doc: ProjectDoc): boolean => ladderOf(doc).length > 0
export const growthOf = (doc: ProjectDoc): RankGrowth => (doc.ranks?.growth === 'use' ? 'use' : 'fixed')
export const rankName = (doc: ProjectDoc, id: string): string => ladderOf(doc).find((r) => r.id === id)?.name ?? id
/** Weapon ranks need weapons the units carry (the loader refuses a ladder otherwise). */
export const canSwitchOnRanks = (doc: ProjectDoc): boolean => carriedWeapons(doc)

/** Every group some weapon belongs to, in the order of the weapons (the engine's order). */
export const groupsOf = (doc: ProjectDoc): string[] => [...new Set((doc.weapons ?? []).flatMap((w) => (w.group ? [w.group] : [])))]

/** The game-wide cap of a stat, or undefined (no cap). */
export const gameCap = (doc: ProjectDoc, stat: string): number | undefined => doc.caps?.caps?.[stat]
/** The cap a unit of this class has before a character's adjustment: the class's own, else the game's. */
export const classCap = (doc: ProjectDoc, type: string | undefined, stat: string): number | undefined =>
  doc.units?.find((u) => u.id === type)?.caps?.[stat] ?? gameCap(doc, stat)
/** class or game cap + adjustment, never below 0; undefined = no cap (a stat with no cap anywhere stays uncapped). */
export const resultingCap = (base: number | undefined, adjust: number | undefined): number | undefined =>
  base === undefined ? undefined : Math.max(0, base + (adjust ?? 0))

// --- the feature ------------------------------------------------------------------------------------------------------

/** Switches the weapon ranks on with nothing in them (the empty file is written, so the switch stays on). */
export const switchOnRanks = (doc: ProjectDoc): ProjectDoc => (doc.ranks !== null || !canSwitchOnRanks(doc) ? doc : { ...doc, ranks: { growth: 'fixed', ranks: [] } })
export const switchOnCaps = (doc: ProjectDoc): ProjectDoc => (doc.caps !== null ? doc : { ...doc, caps: { caps: {} } })

/** Switches the weapon ranks off: the file goes, and so does everything that depends on a ladder (the caller asked first). */
export const switchOffRanks = (doc: ProjectDoc): ProjectDoc => (doc.ranks === null ? doc : { ...cleanAllRanks(doc), ranks: null })
/** Switches the stat caps off: the file goes, with every class cap and character adjustment. */
export const switchOffCaps = (doc: ProjectDoc): ProjectDoc => (doc.caps === null ? doc : { ...cleanAllCaps(doc), caps: null })

/** How many things a feature holds, for the "are you sure" question. */
export const ranksCount = (doc: ProjectDoc): number => ladderOf(doc).length
export const capsCount = (doc: ProjectDoc): number => Object.keys(doc.caps?.caps ?? {}).length
  + (doc.units ?? []).filter((u) => u.caps).length + (doc.characters ?? []).filter((c) => c.capAdjust).length

/** Edits the ranks file (a no-op while the feature is off). */
const withRanks = (doc: ProjectDoc, fn: (r: Ranks) => Ranks): ProjectDoc => {
  if (!doc.ranks) return doc
  const next = fn(doc.ranks)
  return next === doc.ranks ? doc : { ...doc, ranks: next }
}
const withCaps = (doc: ProjectDoc, fn: (c: Caps) => Caps): ProjectDoc => {
  if (!doc.caps) return doc
  const next = fn(doc.caps)
  return next === doc.caps ? doc : { ...doc, caps: next }
}

export const setGrowth = (doc: ProjectDoc, growth: RankGrowth): ProjectDoc => withRanks(doc, (r) => (r.growth === growth ? r : { ...r, growth }))

// --- the ladder -----------------------------------------------------------------------------------------------------

/** Puts the default ladder in, when there is none yet (a ladder is never replaced silently). */
export const fillDefaultLadder = (doc: ProjectDoc): ProjectDoc => withRanks(doc, (r) => (r.ranks.length ? r : { ...r, ranks: DEFAULT_LADDER.map((x) => ({ ...x })) }))

/** Keeps the ladder valid: the first rank needs 0, each one more than the one before (later ones move up to make room). */
export function tidyLadder(ranks: Rank[]): Rank[] {
  let prev = -1
  let changed = false
  const out = ranks.map((r, i) => {
    const xp = i === 0 ? 0 : Math.max(r.xp, prev + 1)
    prev = xp
    if (xp === r.xp) return r
    changed = true
    return { ...r, xp }
  })
  return changed ? out : ranks
}

/** A new rank at the end: a plain name, and a step of 50 more than the last. */
export function addRank(doc: ProjectDoc, name = 'New rank'): ProjectDoc {
  return withRanks(doc, (r) => {
    if (r.ranks.length >= MAX_RANKS) return r
    const id = makeId(name, r.ranks.map((x) => x.id), { fallback: 'rank' })
    const last = r.ranks[r.ranks.length - 1]
    const xp = last ? Math.min(last.xp + 50, MAX_RANK_XP) : 0
    return { ...r, ranks: [...r.ranks, { id, name, xp }] }
  })
}

/** Removes a rank and everything that named it (weapons that needed it need nothing; starting ranks fall away). */
export function removeRank(doc: ProjectDoc, id: string): ProjectDoc {
  const cleaned = rankRefsOnRemove(doc, id)
  return withRanks(cleaned, (r) => ({ ...r, ranks: tidyLadder(r.ranks.filter((x) => x.id !== id)) }))
}

/** Moves a rank up or down. The experience numbers stay where they are (they belong to the steps of the ladder), so the
 *  ladder keeps rising whatever the order. */
export function moveRank(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withRanks(doc, (r) => {
    if (from === to || from < 0 || to < 0 || from >= r.ranks.length || to >= r.ranks.length) return r
    const order = r.ranks.slice()
    const [m] = order.splice(from, 1)
    order.splice(to, 0, m)
    return { ...r, ranks: order.map((x, i) => ({ ...x, xp: r.ranks[i].xp })) }
  })
}

/** A rank's id; weapons, classes, characters and placed units that named the old id follow. Refused (same document) for an id
 *  that is empty, taken, or has characters the loader does not allow. */
export function setRankId(doc: ProjectDoc, from: string, to: string): ProjectDoc {
  if (from === to || !RANK_ID.test(to) || ladderOf(doc).some((r) => r.id === to)) return doc
  const moved = withRanks(doc, (r) => ({ ...r, ranks: r.ranks.map((x) => (x.id === from ? { ...x, id: to } : x)) }))
  return renameRankRefs(moved, from, to)
}
/** What the loader accepts as a rank id: lowercase letters, digits, "_" and "-". */
export const RANK_ID = /^[a-z0-9_-]+$/

export const renameRank = (doc: ProjectDoc, id: string, name: string): ProjectDoc =>
  withRanks(doc, (r) => ({ ...r, ranks: r.ranks.map((x) => (x.id === id ? { ...x, name } : x)) }))

/** The experience a rank needs; the ones above it move up when needed. The first rank always stays at 0. */
export function setRankXp(doc: ProjectDoc, id: string, xp: number): ProjectDoc {
  return withRanks(doc, (r) => ({ ...r, ranks: tidyLadder(r.ranks.map((x) => (x.id === id ? { ...x, xp: Math.min(Math.max(0, Math.round(xp)), MAX_RANK_XP) } : x))) }))
}

// --- weapons, classes, characters, placed units ---------------------------------------------------------------------

/** The weapon needs this rank (undefined = any); the experience per use is only kept when it is not the default. */
export function setWeaponRank(doc: ProjectDoc, weapon: string, rank: string | undefined): ProjectDoc {
  return { ...doc, weapons: doc.weapons && doc.weapons.map((w) => (w.id === weapon ? patchObj(w, { rank }) : w)) }
}
export function setWeaponRankXp(doc: ProjectDoc, weapon: string, xp: number | undefined): ProjectDoc {
  return { ...doc, weapons: doc.weapons && doc.weapons.map((w) => (w.id === weapon ? patchObj(w, { rankXp: xp === DEFAULT_USE_XP ? undefined : xp }) : w)) }
}

/** One group's starting value in a ranks map; undefined removes it, and an emptied map is dropped (undefined). */
export function setRankIn(map: RankMap | undefined, group: string, value: RankValue | undefined): RankMap | undefined {
  const next = { ...(map ?? {}) }
  if (value === undefined) delete next[group]; else next[group] = value
  return Object.keys(next).length ? next : undefined
}

/** One stat's number in a caps / cap-adjust map; undefined removes it. */
export function setCapIn(map: NumMap | undefined, stat: string, value: number | undefined): NumMap | undefined {
  const next = { ...(map ?? {}) }
  if (value === undefined) delete next[stat]; else next[stat] = value
  return Object.keys(next).length ? next : undefined
}

/** The game-wide cap of a stat (undefined = no cap). */
export function setGameCap(doc: ProjectDoc, stat: string, value: number | undefined): ProjectDoc {
  return withCaps(doc, (c) => ({ ...c, caps: setCapIn(c.caps, stat, value) ?? {} }))
}
