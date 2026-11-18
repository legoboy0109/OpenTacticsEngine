// The two menu options of the player (game.json "menuUnavailable" and "attackWeaponChoice"; docs/SRPG.md "Menu options"). Presentation
// only. The default of each is written as NO key. A word the loader would refuse reads as the default here and is reported by
// menuOptionProblems (the real loader refuses it too); unknown keys stay.
import { withGame } from './doc'
import { patchObj } from './util'
import type { ProjectDoc } from './types'

export type MenuUnavailable = 'grey' | 'hide'
export type AttackWeaponChoice = 'equipped' | 'pick'
export const MENU_UNAVAILABLE: MenuUnavailable[] = ['grey', 'hide']
export const ATTACK_WEAPON_CHOICES: AttackWeaponChoice[] = ['equipped', 'pick']

export const menuUnavailable = (doc: ProjectDoc): MenuUnavailable => (doc.game?.menuUnavailable === 'hide' ? 'hide' : 'grey')
export function setMenuUnavailable(doc: ProjectDoc, v: MenuUnavailable): ProjectDoc {
  if (!doc.game || menuUnavailable(doc) === v) return doc
  return withGame(doc, (g) => patchObj(g, { menuUnavailable: v === 'hide' ? 'hide' : undefined }))
}

export const attackWeaponChoice = (doc: ProjectDoc): AttackWeaponChoice => (doc.game?.attackWeaponChoice === 'pick' ? 'pick' : 'equipped')
export function setAttackWeaponChoice(doc: ProjectDoc, v: AttackWeaponChoice): ProjectDoc {
  if (!doc.game || attackWeaponChoice(doc) === v) return doc
  return withGame(doc, (g) => patchObj(g, { attackWeaponChoice: v === 'pick' ? 'pick' : undefined }))
}

/** What the loader refuses (a word that is not one of the two), said in plain words, and a note where "pick" changes nothing. */
export function menuOptionProblems(doc: ProjectDoc): string[] {
  const g = doc.game
  const out: string[] = []
  if (g?.menuUnavailable !== undefined && !MENU_UNAVAILABLE.includes(g.menuUnavailable)) out.push('“Actions with nothing to act on” must be greyed out or hidden.')
  if (g?.attackWeaponChoice !== undefined && !ATTACK_WEAPON_CHOICES.includes(g.attackWeaponChoice)) out.push('“Choosing Attack” must go straight to the targets or ask for the weapon first.')
  if (attackWeaponChoice(doc) === 'pick' && g?.rules?.items?.weapons !== 'carried') out.push('Units here do not carry weapons they can swap, so the list would only ever show the unit’s own weapons. Choosing a weapon matters most when weapons are items a unit carries (Items card).')
  return out
}
