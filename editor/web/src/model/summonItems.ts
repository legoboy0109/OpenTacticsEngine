// A summon's follow-ups (docs/SRPG.md "Summoning"): the candidates for what the summoned unit carries ("items": { item, minLevel }), which
// one is picked ("pick"), and "replace". Pure edits on one ability; the loader's refusals said in plain words.
import { setSummon, summonOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

/** `item` '' = the candidate "no item" (written as `"item": null`); `maxLevel` / `weight` undefined = not written (no ceiling / 1). */
export interface SummonItemRow { item: string; minLevel: number; maxLevel?: number; weight?: number }
export const MAX_SUMMON_ITEMS = 32
export const NO_ITEM = ''
const NOTHING = '__nothing__'  // the select's value for the "no item" candidate (an item id is never this)
export const NO_ITEM_VALUE = NOTHING

export const summonItemRows = (a: Ability): SummonItemRow[] =>
  (summonOf(a).items ?? []).map((i) => ({
    item: typeof i?.item === 'string' ? i.item : i && i.item === null ? NOTHING : '',
    minLevel: typeof i?.minLevel === 'number' ? i.minLevel : 1,
    maxLevel: typeof i?.maxLevel === 'number' ? i.maxLevel : undefined,
    weight: typeof i?.weight === 'number' ? i.weight : undefined,
  }))

/** Rows go back as { item } plus only the numbers that are not the default (minLevel 1, no ceiling, weight 1). */
const write = (a: Ability, rows: SummonItemRow[]): Ability =>
  setSummon(a, {
    items: rows.map((r) => {
      const o: { item: string | null; minLevel?: number; maxLevel?: number; weight?: number } = { item: r.item === NOTHING ? null : r.item }
      if (r.minLevel !== 1) o.minLevel = r.minLevel
      if (r.maxLevel !== undefined) o.maxLevel = r.maxLevel
      if (r.weight !== undefined && r.weight !== 1) o.weight = r.weight
      return o
    }),
  })

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
    if (r.item !== NOTHING && !doc.items?.some((x) => x.id === r.item)) out.push(`Item ${i + 1} of the summon (“${r.item}”) is not an item of this game.`)
    if (r.maxLevel !== undefined && (!Number.isInteger(r.maxLevel) || r.maxLevel < r.minLevel || r.maxLevel > max)) out.push(`Item ${i + 1} of the summon: the highest level must be a whole number from its lowest level (${r.minLevel}) to ${max}.`)
    if (r.weight !== undefined && (!Number.isInteger(r.weight) || r.weight < 1 || r.weight > 1000)) out.push(`Item ${i + 1} of the summon: the weight must be a whole number from 1 to 1000.`)
    if (!Number.isInteger(r.minLevel) || r.minLevel < 1 || r.minLevel > max) out.push(`Item ${i + 1} of the summon: the level must be a whole number from 1 to ${max}.`)
  })
  return out
}
