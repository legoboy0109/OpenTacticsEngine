// Unit badges: data/graphics.json "badges" (docs/GRAPHICS.md "Unit badges"): a small picture drawn on the map sprite of the units that carry a tag (a boss
// marker). Pure edits on the document, and the checks that mirror engine/core/data/loader_badges.cpp in plain words. Defaults are never written.
import { findSheet, sheetsOf, withGraphics } from './graphics'
import { isStoryId } from './storyIds'
import type { Badge, CellRef, ProjectDoc } from './types'
import { patchObj } from './util'

export const BADGE_ANCHORS = ['topLeft', 'topRight', 'bottomLeft', 'bottomRight', 'center'] as const
export type BadgeAnchor = typeof BADGE_ANCHORS[number]
export const BADGE_SIDE_WORDS = ['all', 'computer', 'player'] as const
export const BADGE_OFFSET_MAX = 256

export const badgeList = (doc: ProjectDoc): Badge[] => doc.graphics?.badges ?? []
const setBadges = (doc: ProjectDoc, list: Badge[]): ProjectDoc => withGraphics(doc, (g) => patchObj(g, { badges: list.length ? list : undefined }))

/** Every tag a unit type or a character of the game has (what a badge can ask for). */
export const badgeTags = (doc: ProjectDoc): string[] =>
  [...new Set([...(doc.units ?? []).flatMap((u) => u.tags ?? []), ...(doc.characters ?? []).flatMap((c) => (c as { tags?: string[] }).tags ?? [])])].sort()

/** A new badge: the first image, whole, for the tag `boss`, on the computer's units, in the bottom right corner. Needs an image. */
export function addBadge(doc: ProjectDoc): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet) return doc
  const taken = new Set(badgeList(doc).map((b) => b.id))
  let id = 'badge'
  for (let n = 2; taken.has(id); n++) id = `badge${n}`
  return setBadges(doc, [...badgeList(doc), { id, sheet, tag: 'boss', sides: 'computer', anchor: 'bottomRight', offset: [0, 0] }])
}

/** Changes the badge at `index`. A default (all sides, bottom right, no offset) is written as no key, an unset cell too. */
export function patchBadge(doc: ProjectDoc, index: number, patch: Partial<Badge> & { cell?: CellRef | undefined }): ProjectDoc {
  const list = badgeList(doc)
  const cur = list[index]
  if (!cur) return doc
  const next = patchObj(cur, patch)
  if (next.sides === 'all') delete next.sides
  if (next.anchor === 'bottomRight') delete next.anchor
  if (next.offset && next.offset[0] === 0 && next.offset[1] === 0) delete next.offset
  return setBadges(doc, list.map((b, i) => (i === index ? next : b)))
}

export function moveBadge(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  const list = [...badgeList(doc)]
  if (from === to || from < 0 || to < 0 || from >= list.length || to >= list.length) return doc
  const [it] = list.splice(from, 1)
  list.splice(to, 0, it)
  return setBadges(doc, list)
}

export const removeBadge = (doc: ProjectDoc, index: number): ProjectDoc => setBadges(doc, badgeList(doc).filter((_, i) => i !== index))

/** Badges that draw from the image (they go when the image is removed). */
export const badgesUsingSheet = (doc: ProjectDoc, sheetId: string): string[] => badgeList(doc).filter((b) => b.sheet === sheetId).map((b) => b.id)

const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

/** What the game would refuse in the badges (the loader's readBadges), in plain words. The cell's place on the sheet is checked by the game (the editor does not know the image's size). */
export function badgeProblems(doc: ProjectDoc): string[] {
  const out: string[] = []
  const list = doc.graphics?.badges
  if (list === undefined) return out
  if (!Array.isArray(list)) return ['The badges must be a list.']
  const tags = new Set(badgeTags(doc))
  const seen = new Set<string>()
  list.forEach((b, i) => {
    const at = `Badge ${i + 1}${typeof b?.id === 'string' && b.id ? ` “${b.id}”` : ''}`
    if (b === null || typeof b !== 'object') { out.push(`${at} must be an object.`); return }
    if (typeof b.id !== 'string' || !isStoryId(b.id)) out.push(`${at}: the name may use only letters, digits, _ and - (and not be empty).`)
    else if (seen.has(b.id)) out.push(`${at}: another badge has the same name.`)
    seen.add(b.id)
    if (typeof b.sheet !== 'string' || !findSheet(doc, b.sheet)) out.push(`${at}: pick one of the game’s images.`)
    if (b.cell !== undefined && (!Array.isArray(b.cell) || b.cell.length !== 2 || !whole(b.cell[0]) || !whole(b.cell[1]) || b.cell[0] < 0 || b.cell[1] < 0)) out.push(`${at}: the cell must be a column and a row, whole numbers from 0.`)
    if (typeof b.tag !== 'string' || b.tag === '') out.push(`${at}: type the tag the units wear it for.`)
    else if (!tags.has(b.tag)) out.push(`${at}: no unit type and no character has the tag “${b.tag}” (give one a tag on the Units or Characters screen).`)
    const s = b.sides
    if (s !== undefined && !(typeof s === 'string' && (BADGE_SIDE_WORDS as readonly string[]).includes(s)) && !(Array.isArray(s) && s.length > 0 && s.every((x) => typeof x === 'string' && x !== ''))) out.push(`${at}: “whose units” must be all, the computer’s, the player’s or a list of side names.`)
    if (b.anchor !== undefined && !(BADGE_ANCHORS as readonly string[]).includes(b.anchor)) out.push(`${at}: the corner must be one of the four corners or the middle.`)
    const o = b.offset
    if (o !== undefined && !(Array.isArray(o) && o.length === 2 && whole(o[0]) && whole(o[1]) && Math.abs(o[0]) <= BADGE_OFFSET_MAX && Math.abs(o[1]) <= BADGE_OFFSET_MAX)) out.push(`${at}: the offset must be two whole numbers from -${BADGE_OFFSET_MAX} to ${BADGE_OFFSET_MAX}.`)
  })
  return out
}
