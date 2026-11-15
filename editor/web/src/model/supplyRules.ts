// rules.supply in game.json (docs/ECONOMY.md "Repair and supply"). Defaults are never written, and a block with nothing
// left in it is removed, so a game that does not use repair or supply keeps a game.json without `rules.supply`.
import { withGame } from './doc'
import type { ProjectDoc, SupplyRules } from './types'

export const REPAIR_AMOUNT_MIN = 1
export const REPAIR_AMOUNT_MAX = 100
export const REPAIR_SHARE_DEFAULT = 100
/** Percent of maximum HP per turn a switched-on repair starts with. */
export const REPAIR_AMOUNT_START = 20

const supplyOf = (doc: ProjectDoc): SupplyRules | undefined => doc.game?.rules?.supply

export const repairOn = (doc: ProjectDoc): boolean => !!supplyOf(doc)?.repair
export const repairAmount = (doc: ProjectDoc): number => supplyOf(doc)?.repair?.amount ?? REPAIR_AMOUNT_START
export const repairShare = (doc: ProjectDoc): number => supplyOf(doc)?.repair?.costShare ?? REPAIR_SHARE_DEFAULT
export const structuresResupply = (doc: ProjectDoc): boolean => supplyOf(doc)?.structures === true

function withSupply(doc: ProjectDoc, fn: (s: SupplyRules) => SupplyRules): ProjectDoc {
  return withGame(doc, (g) => {
    const next = fn({ ...(g.rules?.supply ?? {}) })
    if (next.structures !== true) delete next.structures
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(next).length) rules.supply = next
    else delete rules.supply
    return { ...g, rules }
  })
}

export const setRepairOn = (doc: ProjectDoc, on: boolean): ProjectDoc =>
  withSupply(doc, (s) => {
    if (on) return s.repair ? s : { ...s, repair: { amount: REPAIR_AMOUNT_START } }
    const { repair: _off, ...rest } = s
    return rest
  })

const clampInt = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))

export const setRepairAmount = (doc: ProjectDoc, percent: number): ProjectDoc =>
  withSupply(doc, (s) => ({ ...s, repair: { ...s.repair, amount: clampInt(percent, REPAIR_AMOUNT_MIN, REPAIR_AMOUNT_MAX) } }))

/** 0 = free; 100 (the default) is not written. */
export const setRepairShare = (doc: ProjectDoc, percent: number): ProjectDoc =>
  withSupply(doc, (s) => {
    const share = clampInt(percent, 0, 100)
    const repair = { ...s.repair, amount: s.repair?.amount ?? REPAIR_AMOUNT_START }
    if (share === REPAIR_SHARE_DEFAULT) delete repair.costShare; else repair.costShare = share
    return { ...s, repair }
  })

export const setStructuresResupply = (doc: ProjectDoc, on: boolean): ProjectDoc =>
  withSupply(doc, (s) => ({ ...s, structures: on }))
