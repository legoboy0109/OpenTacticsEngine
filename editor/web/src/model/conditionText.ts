// The plain sentence a condition adds up to, shown on its form and as the second line in the list: how long it lasts, what ends it early,
// what it replaces, and each rule as the Skills screen words it. Wording is a placeholder for the owner to review.
import { blocksOfCondition, conditionName } from './conditions'
import { blockSummary } from './skillText'
import type { Condition } from './srpgTypes'
import type { ProjectDoc } from './types'

export function durationText(c: Condition): string {
  if (c.until === 'cured') return 'Lasts until it is cured.'
  if (c.until === 'mapEnd') return 'Lasts until the map ends.'
  const n = c.turns ?? 0
  return `Lasts ${n} of the unit’s own ${n === 1 ? 'turn' : 'turns'}.`
}

/** "Lasts 3 of the unit’s own turns. Each turn there is a 25% chance it wears off early. It ends when the unit is struck. At the start of its turn: loses 10% ... ." */
export function conditionSummary(doc: ProjectDoc, c: Condition): string {
  const parts = [durationText(c)]
  if (c.wearOff) parts.push(`Each turn there is a ${c.wearOff}% chance it wears off early.`)
  if (c.endsWhenStruck) parts.push('It ends when the unit is struck.')
  if (c.scale === 'turnsLeft') parts.push('Its always-on numbers count as many times as it has turns left, so they fade as it runs out.')
  if (c.replaces?.length) parts.push(`Putting it on removes ${c.replaces.map((r) => conditionName(doc, r)).join(', ')}.`)
  for (const b of blocksOfCondition(c)) parts.push(blockSummary(doc, b))
  return parts.join(' ')
}

/** The shorter line under a condition's name in the list: its duration. */
export const conditionListLine = (c: Condition): string => durationText(c).replace(/\.$/, '')
