// What the loader refuses about the story's support pieces beyond the names (E4 of docs/SRPG.md): the command `addSupport`, the
// condition `support` and the trigger `support` must name a pair that data/supports.json declares, a level that exists and
// numbers in range. Shared by the map events' checks and the campaign scenes' checks; each passes its own `say`.
import type { Command, Condition, Trigger } from './storyTypes'
import { characterName, MAX_LEVELS, MAX_POINTS, pairLabel, pairsOf, samePair } from './supports'
import type { ProjectDoc } from './types'

type Say = (where: string, text: string) => void

/** Why `pair` cannot be used (null when it can). A character that does not exist is the name check's to report. */
export function pairProblem(doc: ProjectDoc, pair: unknown): string | null {
  if (!Array.isArray(pair) || pair.length !== 2 || typeof pair[0] !== 'string' || typeof pair[1] !== 'string') return 'pick a pair.'
  const [a, b] = pair
  if (a === '' || b === '') return 'pick a pair.'
  const known = (id: string) => !!doc.characters?.some((c) => c.id === id)
  if (!known(a) || !known(b)) return null
  return pairsOf(doc).some((p) => samePair(p, a, b)) ? null : `${characterName(doc, a)} and ${characterName(doc, b)} are not a support pair (make the pair on the Supports screen).`
}

export function addSupportChecks(doc: ProjectDoc, c: Command, where: string, say: Say): void {
  const p = pairProblem(doc, c.addSupport)
  if (p) say(where, p)
  if (typeof c.by !== 'number' || Math.abs(c.by) > MAX_POINTS) say(where, `the points must be a number from -${MAX_POINTS} to ${MAX_POINTS}.`)
}

export function supportConditionChecks(doc: ProjectDoc, list: Condition[] | undefined, where: string, say: Say): void {
  list?.forEach((c, i) => {
    if (c.support === undefined) return
    const at = `${where}, condition ${i + 1}`
    const p = pairProblem(doc, c.support)
    if (p) say(at, p)
    if (c.value !== undefined && (!Number.isInteger(c.value) || c.value < 0 || c.value > MAX_LEVELS)) say(at, `the level must be from 0 to ${MAX_LEVELS}.`)
  })
}

export function supportTriggerChecks(doc: ProjectDoc, t: Trigger, where: string, say: Say): void {
  if (t.support === undefined) return
  const p = pairProblem(doc, t.support)
  if (p) { say(where, p); return }
  if (t.level === undefined) return
  const pair = pairsOf(doc).find((x) => Array.isArray(t.support) && samePair(x, t.support[0], t.support[1]))
  const n = pair?.levels?.length ?? 0
  if (pair && (!Number.isInteger(t.level) || t.level < 1 || t.level > n)) say(where, `${pairLabel(doc, pair)} have ${n} ${n === 1 ? 'level' : 'levels'}, so the level must be from 1 to ${n}.`)
}
