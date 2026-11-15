// Done-test helpers for the lessons about structures (Structures, Economy, Hexagons): "a structure kind the maker made" and
// "one of those standing on the map". The template's own kinds are passed in as `start`, so only new work counts.
import { mapOf } from '../helpers'
import type { ProjectDoc, Structure } from '../../model/types'

/** Structure kinds that are not among `start` (the ids the template began with). */
export const newKinds = (doc: ProjectDoc, start: string[]): Structure[] => (doc.structures ?? []).filter((s) => !start.includes(s.id))

/** The id of the first kind the maker made (for "Take me there"), or undefined. */
export const firstNewKindId = (start: string[]) => (doc: ProjectDoc): string | undefined => newKinds(doc, start)[0]?.id

/** Does any kind the maker made satisfy `want`? */
export const someNewKind = (doc: ProjectDoc, start: string[], want: (s: Structure) => boolean): boolean => newKinds(doc, start).some(want)

/** Structures on the map whose kind is new (not in `start`) and passes `want`, optionally owned by `side`. */
export function newKindsOnMap(doc: ProjectDoc, start: string[], opts: { side?: string; want?: (s: Structure) => boolean } = {}, mapId?: string): number {
  const kinds = new Map(newKinds(doc, start).filter((k) => opts.want?.(k) ?? true).map((k) => [k.id, k]))
  return (mapOf(doc, mapId)?.structures ?? []).filter((s) => kinds.has(s.type) && (opts.side === undefined || s.side === opts.side)).length
}

/** Total of a number map (income, cost): 0 when missing. */
export const total = (m: Record<string, number> | undefined): number => Object.values(m ?? {}).reduce((a, b) => a + (Number.isFinite(b) ? b : 0), 0)
