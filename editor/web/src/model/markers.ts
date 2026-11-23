// Map markers: data/graphics.json "markers" (docs/GRAPHICS.md "Map markers"): a small picture an event puts on the squares of its reach area while it can still run.
// Pure edits on the document and the checks that mirror engine/core/data/loader_badges.cpp (readMarkers) and loader_events.cpp ("marker").
import { BADGE_ANCHORS, BADGE_OFFSET_MAX } from './badges'
import { findSheet, sheetsOf, withGraphics } from './graphics'
import { isStoryId } from './storyIds'
import { triggerKind } from './storyTypes'
import type { Marker, ProjectDoc } from './types'
import { patchObj } from './util'

export const markerList = (doc: ProjectDoc): Marker[] => doc.graphics?.markers ?? []
const setMarkers = (doc: ProjectDoc, list: Marker[]): ProjectDoc => withGraphics(doc, (g) => patchObj(g, { markers: list.length ? list : undefined }))

/** A new marker: the first image, whole, centred on the square. Needs an image. */
export function addMarker(doc: ProjectDoc): ProjectDoc {
  const sheet = sheetsOf(doc)[0]?.id
  if (!sheet) return doc
  const taken = new Set(markerList(doc).map((m) => m.id))
  let id = 'interest'
  for (let n = 2; taken.has(id); n++) id = `interest${n}`
  return setMarkers(doc, [...markerList(doc), { id, sheet }])
}

/** Changes the marker at `index`; the defaults (centre, no offset) are written as no key. */
export function patchMarker(doc: ProjectDoc, index: number, patch: Partial<Marker> & { cell?: [number, number] | undefined }): ProjectDoc {
  const list = markerList(doc)
  if (!list[index]) return doc
  const next = patchObj(list[index], patch)
  if (next.anchor === 'center') delete next.anchor
  if (next.offset && next.offset[0] === 0 && next.offset[1] === 0) delete next.offset
  return setMarkers(doc, list.map((m, i) => (i === index ? next : m)))
}

export const removeMarker = (doc: ProjectDoc, index: number): ProjectDoc => setMarkers(doc, markerList(doc).filter((_, i) => i !== index))
export const markersUsingSheet = (doc: ProjectDoc, sheetId: string): string[] => markerList(doc).filter((m) => m.sheet === sheetId).map((m) => m.id)

const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

/** What the game would refuse in the markers list, in plain words. */
export function markerProblems(doc: ProjectDoc): string[] {
  const list = doc.graphics?.markers
  if (list === undefined) return []
  if (!Array.isArray(list)) return ['The markers must be a list.']
  const out: string[] = []
  const seen = new Set<string>()
  list.forEach((m, i) => {
    const at = `Marker ${i + 1}${typeof m?.id === 'string' && m.id ? ` “${m.id}”` : ''}`
    if (typeof m?.id !== 'string' || !isStoryId(m.id)) out.push(`${at}: the name may use only letters, digits, _ and - (and not be empty).`)
    else if (seen.has(m.id)) out.push(`${at}: another marker has the same name.`)
    if (typeof m?.id === 'string') seen.add(m.id)
    if (typeof m?.sheet !== 'string' || !findSheet(doc, m.sheet)) out.push(`${at}: pick one of the game’s images.`)
    if (m?.cell !== undefined && (!Array.isArray(m.cell) || m.cell.length !== 2 || !whole(m.cell[0]) || !whole(m.cell[1]) || m.cell[0] < 0 || m.cell[1] < 0)) out.push(`${at}: the cell must be a column and a row, whole numbers from 0.`)
    if (m?.anchor !== undefined && !(BADGE_ANCHORS as readonly string[]).includes(m.anchor)) out.push(`${at}: the point must be one of the four corners or the middle.`)
    const o = m?.offset
    if (o !== undefined && !(Array.isArray(o) && o.length === 2 && whole(o[0]) && whole(o[1]) && Math.abs(o[0]) <= BADGE_OFFSET_MAX && Math.abs(o[1]) <= BADGE_OFFSET_MAX)) out.push(`${at}: the offset must be two whole numbers from -${BADGE_OFFSET_MAX} to ${BADGE_OFFSET_MAX}.`)
  })
  return out
}

/** What the loader refuses about an event's `marker`: only a reach event takes one, and it must name a marker of the game. */
export function eventMarkerProblem(doc: ProjectDoc, e: { when?: object; marker?: unknown }): string | null {
  if (e.marker === undefined) return null
  if (typeof e.marker !== 'string') return 'the marker must be the name of one of the game’s markers.'
  if (triggerKind((e.when ?? {}) as never) !== 'reach') return 'a marker is for an event that waits for a unit to reach an area: it is drawn on the squares of that area.'
  if (!markerList(doc).some((m) => m.id === e.marker)) return `“${e.marker}” is not a marker of this game (Art, Map markers).`
  return null
}
