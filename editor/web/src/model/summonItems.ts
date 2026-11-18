// A summon's follow-ups (docs/SRPG.md "Summoning"): the candidates for what the summoned unit carries ("items": { item, minLevel }), which
// one is picked ("pick"), and "replace". Pure edits on one ability; the loader's refusals said in plain words.
import { setSummon, summonOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

export interface SummonItemRow { item: string; minLevel: number }
export const MAX_SUMMON_ITEMS = 8

export const summonItemRows = (a: Ability): SummonItemRow[] =>
  (summonOf(a).items ?? []).map((i) => ({ item: typeof i?.item === 'string' ? i.item : '', minLevel: typeof i?.minLevel === 'number' ? i.minLevel : 1 }))

/** Rows go back as { item } when the level is the default 1. */
const write = (a: Ability, rows: SummonItemRow[]): Ability =>
  setSummon(a, { items: rows.map((r) => (r.minLevel === 1 ? { item: r.item } : { item: r.item, minLevel: r.minLevel })) })

export function addSummonItem(doc: ProjectDoc, a: Ability): Ability {
  const rows = summonItemRows(a)
  if (rows.length >= MAX_SUMMON_ITEMS) return a
  const first = (doc.items ?? []).find((i) => i.kind === 'weapon') ?? doc.items?.[0]
  const last = rows[rows.length - 1]
  return write(a, [...rows, { item: first?.id ?? '', minLevel: last ? last.minLevel + 1 : 1 }])
}
export const removeSummonItem = (a: Ability, i: number): Ability => write(a, summonItemRows(a).filter((_, k) => k !== i))
export const patchSummonItem = (a: Ability, i: number, p: Partial<SummonItemRow>): Ability => write(a, summonItemRows(a).map((r, k) => (k === i ? { ...r, ...p } : r)))
export const setSummonPick = (a: Ability, pick: 'highest' | 'random'): Ability => setSummon(a, { pick })
export const setSummonReplace = (a: Ability, on: boolean): Ability => setSummon(a, { replace: on })

export function summonItemProblems(doc: ProjectDoc, a: Ability): string[] {
  const s = summonOf(a)
  const out: string[] = []
  const max = doc.game?.rules?.progression?.maxLevel ?? 20
  if (s.pick !== undefined && s.items === undefined) out.push('A pick goes with a list of items. Add an item, or remove the pick.')
  if (s.pick !== undefined && s.pick !== 'highest' && s.pick !== 'random') out.push('The pick must be “highest” or “random”.')
  if (s.items !== undefined && (!Array.isArray(s.items) || s.items.length === 0 || s.items.length > MAX_SUMMON_ITEMS)) out.push(`The items must be a list of 1 to ${MAX_SUMMON_ITEMS}.`)
  summonItemRows(a).forEach((r, i) => {
    if (!doc.items?.some((x) => x.id === r.item)) out.push(`Item ${i + 1} of the summon (“${r.item}”) is not an item of this game.`)
    if (!Number.isInteger(r.minLevel) || r.minLevel < 1 || r.minLevel > max) out.push(`Item ${i + 1} of the summon: the level must be a whole number from 1 to ${max}.`)
  })
  return out
}
