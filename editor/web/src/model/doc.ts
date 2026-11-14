// Section helpers: an edit on a section that failed to load (null) is a no-op.
import type { Game, ProjectDoc, Structure, Terrain, Unit, Weapon } from './types'

export const withGame = (doc: ProjectDoc, fn: (g: Game) => Game): ProjectDoc => (doc.game ? { ...doc, game: fn(doc.game) } : doc)
export const withUnits = (doc: ProjectDoc, fn: (u: Unit[]) => Unit[]): ProjectDoc => (doc.units ? { ...doc, units: fn(doc.units) } : doc)
export const withWeapons = (doc: ProjectDoc, fn: (w: Weapon[]) => Weapon[]): ProjectDoc => (doc.weapons ? { ...doc, weapons: fn(doc.weapons) } : doc)
export const withTerrain = (doc: ProjectDoc, fn: (t: Terrain[]) => Terrain[]): ProjectDoc => (doc.terrain ? { ...doc, terrain: fn(doc.terrain) } : doc)

/** Applies `fn` to the structure list; the first structure edit creates it (`null` = "none yet"). */
export const withStructures = (doc: ProjectDoc, fn: (s: Structure[]) => Structure[]): ProjectDoc => {
  const next = fn(doc.structures ?? [])
  return next === doc.structures ? doc : { ...doc, structures: next }
}

export const ids = (list: Array<{ id: string }> | null | undefined): string[] => (list ?? []).map((x) => x.id)

/** Deep copy for duplicating items (plain JSON data only). */
export const clone = <T>(v: T): T => JSON.parse(JSON.stringify(v)) as T
