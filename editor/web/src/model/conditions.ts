// Status conditions (docs/SRPG.md section 9, data/conditions.json): the list and the settings of one condition, as edits on the document.
// A condition's rules are skill blocks (the Skills screen's block editor edits them); the four effects only a condition has (cannot act,
// cannot move, forbid, acts on its own) are `conditionBehaviour.ts`. Deleting lives in conditionRefs.ts. Every edit leaves keys it does not
// know alone; the loader (engine/core/data/loader_conditions.cpp) is the source of truth for what is legal.
import { ids, withConditions } from './doc'
import { makeId } from './ids'
import { MAX_BLOCKS } from './skillRules'
import { conditionStatIds, newBlock, patchBlock, setWhen } from './skillBlocks'
import type { Condition } from './srpgTypes'
import type { Extra, ProjectDoc } from './types'
import { moveItem, patchObj, updateById } from './util'

/** Loader limits (`kMaxConditions`, `kMaxConditionTurns`, tag 1 to 4 characters, `wearOff` 1 to 99). */
export const MAX_CONDITIONS_LISTED = 100
export const CONDITION_TURNS_MAX = 99
export const CONDITION_WEAROFF_MAX = 99
export const TAG_MAX = 4

export type Duration = 'turns' | 'cured' | 'mapEnd'

export const conditionList = (doc: ProjectDoc): Condition[] => doc.conditions ?? []
export const conditionName = (doc: ProjectDoc, id: string): string => doc.conditions?.find((c) => c.id === id)?.name ?? id
export const blocksOfCondition = (c: Condition): Extra[] => (Array.isArray(c.blocks) ? c.blocks : [])
export const newConditionId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.conditions), { fallback: 'condition' })

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
/** The first `n` characters (as the loader counts them: code points). */
const firstChars = (s: string, n: number): string => Array.from(s).slice(0, n).join('')

// --- the list ---------------------------------------------------------------------------------------------------------------

/** A new condition lasts 3 turns and lowers the first stat that is not hit points by 1: valid as it stands (a game with no such stat gets "cannot move"). */
export function newConditionData(doc: ProjectDoc, name: string, id: string): Condition {
  const stat = conditionStatIds(doc)[0]
  const first: Extra = stat ? { when: 'always', do: [{ stats: { [stat]: -1 } }] } : { when: 'always', do: [{ cannotMove: true }] }
  return { id, name, turns: 3, blocks: [first] }
}

export function addCondition(doc: ProjectDoc, name: string, id: string = newConditionId(doc, name)): ProjectDoc {
  if (conditionList(doc).length >= MAX_CONDITIONS_LISTED) return doc
  return withConditions(doc, (list) => [...list, newConditionData(doc, name, id)])
}

export function duplicateCondition(doc: ProjectDoc, id: string, newId?: string): ProjectDoc {
  return withConditions(doc, (list) => {
    const i = list.findIndex((c) => c.id === id)
    if (i < 0 || list.length >= MAX_CONDITIONS_LISTED) return list
    const name = `${list[i].name} copy`
    return [...list.slice(0, i + 1), { ...structuredClone(list[i]), id: newId ?? newConditionId(doc, name), name }, ...list.slice(i + 1)]
  })
}

export const moveCondition = (doc: ProjectDoc, from: number, to: number): ProjectDoc => withConditions(doc, (list) => moveItem(list, from, to))

export const updateCondition = (doc: ProjectDoc, id: string, patch: Partial<Omit<Condition, 'id'>>): ProjectDoc =>
  withConditions(doc, (list) => updateById(list, id, (c) => patchObj(c, patch)))

// --- how long it lasts ------------------------------------------------------------------------------------------------------

export function durationOf(c: Condition): Duration {
  if (c.until === 'cured') return 'cured'
  if (c.until === 'mapEnd') return 'mapEnd'
  return 'turns'
}

/** Exactly one of `turns` and `until` is written. */
export function setDuration(doc: ProjectDoc, id: string, mode: Duration): ProjectDoc {
  const c = doc.conditions?.find((x) => x.id === id)
  if (!c || durationOf(c) === mode) return doc
  return updateCondition(doc, id, mode === 'turns' ? { turns: 3, until: undefined } : { turns: undefined, until: mode })
}
export const setTurns = (doc: ProjectDoc, id: string, n: number): ProjectDoc => updateCondition(doc, id, { turns: clampInt(n, 1, CONDITION_TURNS_MAX), until: undefined })

/** "A chance to wear off early each turn": undefined removes it (never). */
export const setWearOff = (doc: ProjectDoc, id: string, n: number | undefined): ProjectDoc =>
  updateCondition(doc, id, { wearOff: n === undefined ? undefined : clampInt(n, 1, CONDITION_WEAROFF_MAX) })

export const setEndsWhenStruck = (doc: ProjectDoc, id: string, on: boolean): ProjectDoc => updateCondition(doc, id, { endsWhenStruck: on ? true : undefined })

/** The map tag: 1 to 4 characters; empty removes the key (the loader then uses the first two letters of the name). */
export const setTag = (doc: ProjectDoc, id: string, text: string): ProjectDoc => updateCondition(doc, id, { tag: text === '' ? undefined : firstChars(text, TAG_MAX) })

/** What the map shows for a condition (the loader's default: the first two letters of the name). */
export const tagOf = (c: Condition): string => c.tag ?? firstChars(c.name, 2)

/** The other conditions this one replaces (never itself); an empty list removes the key. */
export function setReplaces(doc: ProjectDoc, id: string, list: string[]): ProjectDoc {
  const next = list.filter((x) => x !== id)
  return updateCondition(doc, id, { replaces: next.length ? next : undefined })
}

// --- the rules (skill blocks) ---------------------------------------------------------------------------------------------

export const updateConditionBlock = (doc: ProjectDoc, id: string, index: number, fn: (b: Extra) => Extra): ProjectDoc =>
  withConditions(doc, (list) => updateById(list, id, (c) => ({ ...c, blocks: blocksOfCondition(c).map((b, i) => (i === index ? fn(b) : b)) })))

export function addConditionBlock(doc: ProjectDoc, id: string, when: 'always' | 'turnStart' | 'combat' | 'strike' | 'struck' = 'turnStart'): ProjectDoc {
  return withConditions(doc, (list) => updateById(list, id, (c) => (blocksOfCondition(c).length >= MAX_BLOCKS ? c : { ...c, blocks: [...blocksOfCondition(c), newBlock(doc, when)] })))
}

/** The last rule stays: a condition needs at least one. */
export const removeConditionBlock = (doc: ProjectDoc, id: string, index: number): ProjectDoc =>
  withConditions(doc, (list) => updateById(list, id, (c) => (blocksOfCondition(c).length <= 1 ? c : { ...c, blocks: blocksOfCondition(c).filter((_, i) => i !== index) })))

export const moveConditionBlock = (doc: ProjectDoc, id: string, from: number, to: number): ProjectDoc =>
  withConditions(doc, (list) => updateById(list, id, (c) => ({ ...c, blocks: moveItem(blocksOfCondition(c), from, to) })))

export const changeConditionWhen = (doc: ProjectDoc, id: string, index: number, when: Parameters<typeof setWhen>[1]): ProjectDoc =>
  updateConditionBlock(doc, id, index, (b) => setWhen(b, when, doc))
export { patchBlock }
