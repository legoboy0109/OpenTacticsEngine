// Skills (docs/SRPG.md section 3, data/skills.json): the list and the rules of a skill, as edits on the document. Deleting
// lives in srpgRefs.ts (`deleteSkill`), which also takes the skill off every unit type, character, weapon, item and event.
import { withSkills } from './doc'
import { MAX_BLOCKS, type When } from './skillRules'
import { newBlock, patchBlock, setWhen } from './skillBlocks'
import { newSkillId } from './srpgRefs'
import type { Skill } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'
import { moveItem, patchObj, updateById } from './util'

export const blocksOf = (s: Skill): Extra[] => (Array.isArray(s.blocks) ? s.blocks : [])
/** Every tag any skill carries, sorted (the picker of "nullifySkills" and the suggestions of the tag field). */
export const allSkillTags = (doc: ProjectDoc): string[] => [...new Set((doc.skills ?? []).flatMap((s) => s.tags ?? []))].sort()
export const skillName = (doc: ProjectDoc, id: string): string => doc.skills?.find((s) => s.id === id)?.name ?? id

/** A new skill gives +1 to the first stat that is not hit points or movement, always: valid as it stands. */
export function addSkill(doc: ProjectDoc, name: string, id: string = newSkillId(doc, name)): ProjectDoc {
  const stat = (doc.game?.stats ?? []).find((s) => s.id !== doc.game?.roles?.hp && s.id !== doc.game?.roles?.move) ?? doc.game?.stats?.[0]
  const first: Extra = stat ? { when: 'always', do: [{ stats: { [stat.id]: 1 } }] } : newBlock(doc, 'always')
  return withSkills(doc, (list) => [...list, { id, name, blocks: [first] }])
}

export function duplicateSkill(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withSkills(doc, (list) => {
    const i = list.findIndex((s) => s.id === id)
    if (i < 0) return list
    const name = `${list[i].name} copy`
    return [...list.slice(0, i + 1), { ...structuredClone(list[i]), id: newId ?? newSkillId(doc, name), name }, ...list.slice(i + 1)]
  })
}

export const moveSkill = (doc: ProjectDoc, from: number, to: number): ProjectDoc => withSkills(doc, (list) => moveItem(list, from, to))

export const updateSkill = (doc: ProjectDoc, id: string, patch: Partial<Omit<Skill, 'id'>>): ProjectDoc =>
  withSkills(doc, (list) => updateById(list, id, (s) => patchObj(s, patch)))

/** One rule of the skill changed by `fn`. */
export const updateBlock = (doc: ProjectDoc, id: string, index: number, fn: (b: Extra) => Extra): ProjectDoc =>
  withSkills(doc, (list) => updateById(list, id, (s) => ({ ...s, blocks: blocksOf(s).map((b, i) => (i === index ? fn(b) : b)) })))

export function addBlock(doc: ProjectDoc, id: string, when: When = 'combat'): ProjectDoc {
  return withSkills(doc, (list) => updateById(list, id, (s) => (blocksOf(s).length >= MAX_BLOCKS ? s : { ...s, blocks: [...blocksOf(s), newBlock(doc, when)] })))
}

/** The last rule stays: a skill needs at least one. */
export function removeBlock(doc: ProjectDoc, id: string, index: number): ProjectDoc {
  return withSkills(doc, (list) => updateById(list, id, (s) => (blocksOf(s).length <= 1 ? s : { ...s, blocks: blocksOf(s).filter((_, i) => i !== index) })))
}

export const moveBlock = (doc: ProjectDoc, id: string, from: number, to: number): ProjectDoc =>
  withSkills(doc, (list) => updateById(list, id, (s) => ({ ...s, blocks: moveItem(blocksOf(s), from, to) })))

export const changeWhen = (doc: ProjectDoc, id: string, index: number, when: When): ProjectDoc => updateBlock(doc, id, index, (b) => setWhen(b, when, doc))
export { patchBlock }
