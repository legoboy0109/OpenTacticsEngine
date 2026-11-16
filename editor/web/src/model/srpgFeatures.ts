// Which strategy-RPG features a game has (docs/SRPG.md "Editor"), and how a feature is switched on or off.
// A feature is "on" when its list / file exists in the document; switching on creates it EMPTY. An empty list is never
// written (it removes the file, so opening a screen can not change a game's data hash), which means a feature that
// has nothing in it yet is forgotten when the page is reloaded; that is the contract's own rule ("a game without the
// file and a game with an empty list are the same game").
import { cleanAbility, cleanCharacter, cleanSkill } from './srpgRefs'
import type { ProjectDoc, Section } from './types'
import type { SrpgFeature, Supports } from './srpgTypes'

export const SRPG_FEATURES: SrpgFeature[] = ['characters', 'skills', 'abilities', 'supports']

export const srpgFeatureOn = (doc: ProjectDoc, f: SrpgFeature): boolean => doc[f] !== null

/** Supports with no pair and no setting are the same as no file (see above); anything else is written. */
export const supportsEmpty = (s: Supports): boolean =>
  (s.pairs?.length ?? 0) === 0 && Object.entries(s).every(([k, v]) => k === 'pairs' || (k === 'rules' && Object.keys((v ?? {}) as object).length === 0))

/** Supports need characters: two characters make a pair. */
export const canSwitchOn = (doc: ProjectDoc, f: SrpgFeature): boolean => f !== 'supports' || doc.characters !== null

/** Creates the (empty) list or file. */
export function switchOnFeature(doc: ProjectDoc, f: SrpgFeature): ProjectDoc {
  if (doc[f] !== null || !canSwitchOn(doc, f)) return doc
  return { ...doc, [f]: f === 'supports' ? { pairs: [] } : [] }
}

/** Removes the list or file (the caller asked first) and everything that named its items: a unit type that has a
 *  removed skill would make the loader refuse the game. Switching characters off takes the supports with it, since
 *  they cannot exist without characters. */
export function switchOffFeature(doc: ProjectDoc, f: SrpgFeature): ProjectDoc {
  const list = doc[f]
  if (list === null) return doc
  let out = doc
  if (f === 'characters') for (const c of doc.characters ?? []) out = cleanCharacter(out, c.id)
  if (f === 'skills') for (const sk of doc.skills ?? []) out = cleanSkill(out, sk.id)
  if (f === 'abilities') for (const ab of doc.abilities ?? []) out = cleanAbility(out, ab.id)
  return { ...out, [f]: null, ...(f === 'characters' ? { supports: null } : {}) }
}

/** How many things the feature holds (for the "are you sure" question). */
export const featureCount = (doc: ProjectDoc, f: SrpgFeature): number => (f === 'supports' ? doc.supports?.pairs?.length ?? 0 : doc[f]?.length ?? 0)

/** Sidebar rule: a strategy-RPG screen is listed once its feature is on (or the file exists); the screen being looked at
 *  stays listed so a link or a reload never leaves the sidebar without it. */
export const srpgSectionShown = (doc: ProjectDoc, section: Section, current: Section): boolean =>
  !(SRPG_FEATURES as string[]).includes(section) || section === current || srpgFeatureOn(doc, section as SrpgFeature)
