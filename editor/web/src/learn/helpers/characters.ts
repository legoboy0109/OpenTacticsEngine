// Done-test helpers of the "Characters and skills" lesson (lessons/characters.ts). They read data/characters.json,
// data/skills.json and the first map of the practice game.
import { addSkill, blocksOf } from '../../model/skills'
import { ruleIssues } from '../../model/skillChecks'
import { STARTER_SKILLS } from '../../model/starterSkills'
import type { ProjectDoc } from '../../model/types'
import { mapOf } from '../helpers'

const STARTER_IDS = new Set(STARTER_SKILLS.map((s) => s.id))

/** Characters with a personal bonus (a number other than 0) on at least one stat. */
export const charactersWithBonus = (doc: ProjectDoc): number =>
  (doc.characters ?? []).filter((c) => Object.values(c.stats ?? {}).some((v) => typeof v === 'number' && v !== 0)).length

/** Characters standing on the map (placed with the Characters tab: the unit names the character). */
export const charactersOnMap = (doc: ProjectDoc): number => (mapOf(doc)?.units ?? []).filter((u) => typeof u.character === 'string').length

/** Skills of the starter set the game has (the installer's work). */
export const starterSkillsAdded = (doc: ProjectDoc): number => (doc.skills ?? []).filter((s) => STARTER_IDS.has(s.id)).length

export const charactersWithSkill = (doc: ProjectDoc): number => (doc.characters ?? []).filter((c) => (c.skills ?? []).length > 0).length
export const charactersWithLearned = (doc: ProjectDoc): number => (doc.characters ?? []).filter((c) => (c.learns ?? []).length > 0).length

/** The first skill that is not in the starter set (what "Take me there" opens in the own-skill step). */
export const firstOwnSkillId = (doc: ProjectDoc): string | undefined => (doc.skills ?? []).find((s) => !STARTER_IDS.has(s.id))?.id

/** Skills the maker made that are not in the starter set, whose rules differ from the one a brand-new skill comes with and
 *  that the editor finds nothing wrong with. (A new skill is valid as it stands, so "made" alone would be done at once.) */
export function ownSkillsEdited(doc: ProjectDoc): number {
  const fresh = JSON.stringify(blocksOf(addSkill(doc, 'x', 'x').skills!.at(-1)!))
  return (doc.skills ?? []).filter((s) => !STARTER_IDS.has(s.id) && JSON.stringify(blocksOf(s)) !== fresh && blocksOf(s).every((b) => ruleIssues(doc, b).length === 0)).length
}
