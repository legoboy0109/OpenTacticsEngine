// A promotion level per character (docs/SRPG.md "A promotion level per character"): a character's own `promotion.fromLevel`, and a promotion
// entry's `fromLevelFor` { character id: level }. Defaults are never written; keys the editor does not know stay.
import type { SrpgCheck } from './abilityChecks'
import { updateCharacter } from './characters'
import { maxLevelOf, patchPromotion, promotionsOf } from './promotion'
import type { ProjectDoc } from './types'

/** The character's own lowest promotion level (undefined = not set). */
export const characterPromotionLevel = (doc: ProjectDoc, id: string): number | undefined =>
  doc.characters?.find((c) => c.id === id)?.promotion?.fromLevel

/** undefined clears it (and the whole `promotion` object when nothing else is in it). */
export function setCharacterPromotionLevel(doc: ProjectDoc, id: string, level: number | undefined): ProjectDoc {
  const c = doc.characters?.find((x) => x.id === id)
  if (!c) return doc
  const rest = { ...(c.promotion ?? {}) }
  if (level === undefined) delete rest.fromLevel
  else rest.fromLevel = level
  return updateCharacter(doc, id, { promotion: Object.keys(rest).length ? rest : undefined })
}

/** The rows of a promotion entry's `fromLevelFor`, in the file's order. */
export const levelForRows = (doc: ProjectDoc, unitId: string, index: number): Array<{ character: string; level: number }> => {
  const v = promotionsOf(doc.units?.find((u) => u.id === unitId))[index]?.fromLevelFor
  return v && typeof v === 'object' ? Object.entries(v).map(([character, level]) => ({ character, level: typeof level === 'number' ? level : 1 })) : []
}

function writeLevelFor(doc: ProjectDoc, unitId: string, index: number, next: Record<string, number>): ProjectDoc {
  return patchPromotion(doc, unitId, index, { fromLevelFor: Object.keys(next).length ? next : undefined })
}

/** Sets (or adds) the level for a character; undefined removes the row. */
export function setLevelFor(doc: ProjectDoc, unitId: string, index: number, character: string, level: number | undefined): ProjectDoc {
  const next = Object.fromEntries(levelForRows(doc, unitId, index).map((r) => [r.character, r.level]))
  if (level === undefined) delete next[character]
  else next[character] = level
  return writeLevelFor(doc, unitId, index, next)
}

/** Adds a row for the first character that has none yet (at the entry's own level). */
export function addLevelFor(doc: ProjectDoc, unitId: string, index: number): ProjectDoc {
  const have = new Set(levelForRows(doc, unitId, index).map((r) => r.character))
  const c = (doc.characters ?? []).find((x) => !have.has(x.id))
  if (!c) return doc
  const entry = promotionsOf(doc.units?.find((u) => u.id === unitId))[index]
  return setLevelFor(doc, unitId, index, c.id, entry?.fromLevel ?? 1)
}

/** A character renamed or deleted: its rows in every promotion follow (`to` undefined = drop them). */
export function followCharacterInPromotions(doc: ProjectDoc, from: string, to: string | undefined): ProjectDoc {
  if (!(doc.units ?? []).some((u) => promotionsOf(u).some((p) => p.fromLevelFor && from in p.fromLevelFor))) return doc
  return {
    ...doc,
    units: (doc.units ?? []).map((u) => {
      if (!promotionsOf(u).some((p) => p.fromLevelFor && from in p.fromLevelFor)) return u
      return {
        ...u,
        promotions: promotionsOf(u).map((p) => {
          if (!p.fromLevelFor || !(from in p.fromLevelFor)) return p
          const next: Record<string, number> = {}
          for (const [k, v] of Object.entries(p.fromLevelFor)) {
            if (k !== from) next[k] = v
            else if (to !== undefined) next[to] = v
          }
          const { fromLevelFor: _drop, ...keep } = p
          return Object.keys(next).length ? { ...keep, fromLevelFor: next } : keep
        }),
      }
    }),
  }
}

/** Plain problems of the new keys (the loader refuses the same); shown in the unit type's Promotion card and the character's own. */
export function promotionLevelChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const max = maxLevelOf(doc)
  for (const c of doc.characters ?? []) {
    const l = c.promotion?.fromLevel
    if (c.promotion !== undefined && (typeof c.promotion !== 'object' || l === undefined || !Number.isInteger(l) || l < 1 || l > max))
      out.push({ section: 'characters', ids: [c.id], text: `${c.name}: the level it can promote from must be a whole number from 1 to ${max}.` })
  }
  for (const u of doc.units ?? []) {
    promotionsOf(u).forEach((p, i) => {
      if (p.fromLevelFor === undefined) return
      for (const [k, v] of Object.entries(p.fromLevelFor)) {
        if (!doc.characters?.some((c) => c.id === k)) out.push({ section: 'units', ids: [u.id], text: `${u.name}, promotion ${i + 1}: “${k}” is not a character of this game.` })
        if (!Number.isInteger(v) || v < 1 || v > max) out.push({ section: 'units', ids: [u.id], text: `${u.name}, promotion ${i + 1}: the level for “${k}” must be a whole number from 1 to ${max}.` })
      }
    })
  }
  return out
}
