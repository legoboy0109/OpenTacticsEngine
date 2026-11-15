// Section helpers: an edit on a section that failed to load (null) is a no-op.
import type { Campaign } from './campaignTypes'
import type { Game, Item, ProjectDoc, Structure, Technology, Terrain, Unit, Weapon } from './types'

export const withGame = (doc: ProjectDoc, fn: (g: Game) => Game): ProjectDoc => (doc.game ? { ...doc, game: fn(doc.game) } : doc)
export const withUnits = (doc: ProjectDoc, fn: (u: Unit[]) => Unit[]): ProjectDoc => (doc.units ? { ...doc, units: fn(doc.units) } : doc)
export const withWeapons = (doc: ProjectDoc, fn: (w: Weapon[]) => Weapon[]): ProjectDoc => (doc.weapons ? { ...doc, weapons: fn(doc.weapons) } : doc)
export const withTerrain = (doc: ProjectDoc, fn: (t: Terrain[]) => Terrain[]): ProjectDoc => (doc.terrain ? { ...doc, terrain: fn(doc.terrain) } : doc)

/** Applies `fn` to the structure list; the first structure edit creates it (`null` = "none yet"). */
export const withStructures = (doc: ProjectDoc, fn: (s: Structure[]) => Structure[]): ProjectDoc => {
  const next = fn(doc.structures ?? [])
  return next === doc.structures ? doc : { ...doc, structures: next }
}

/** Applies `fn` to the item list; the first item edit creates it (`null` = "none yet"). */
export const withItems = (doc: ProjectDoc, fn: (i: Item[]) => Item[]): ProjectDoc => {
  const next = fn(doc.items ?? [])
  return next === doc.items ? doc : { ...doc, items: next }
}

/** Applies `fn` to the technology list; the first technology creates it (`null` = "none yet"; an edit that leaves it
 *  empty keeps `null` rather than inventing an empty list). */
export const withResearch = (doc: ProjectDoc, fn: (t: Technology[]) => Technology[]): ProjectDoc => {
  const next = fn(doc.research ?? [])
  if (next === doc.research || (doc.research === null && next.length === 0)) return doc
  return { ...doc, research: next }
}

/** Applies `fn` to the campaign (an edit while there is none is a no-op: creating one is `createCampaign`). */
export const withCampaign = (doc: ProjectDoc, fn: (c: Campaign) => Campaign): ProjectDoc => {
  if (!doc.campaign) return doc
  const next = fn(doc.campaign)
  return next === doc.campaign ? doc : { ...doc, campaign: next }
}

export const ids = (list: Array<{ id: string }> | null | undefined): string[] => (list ?? []).map((x) => x.id)

/** Deep copy for duplicating items (plain JSON data only). */
export const clone = <T>(v: T): T => JSON.parse(JSON.stringify(v)) as T
