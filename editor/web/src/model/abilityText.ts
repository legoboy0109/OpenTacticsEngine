// The plain one-line summary of an ability ("Pushes a unit next to it 1 square away. Uses the unit’s action."), shown under
// its form and as the list's second line. Wording is a placeholder for the owner to review (docs/SRPG.md "E3 as built").
import { conditionsText } from './abilityConditions'
import { effectKind, refreshPerTurn, rangeOf, stealOf, summonOf, targetOf, unlockOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

const squares = (n: number) => `${n} ${n === 1 ? 'square' : 'squares'}`
const times = (n: number) => `${n} ${n === 1 ? 'time' : 'times'}`

const whom = (a: Ability): string => {
  switch (targetOf(a)) {
    case 'self': return 'itself'
    case 'ally': return 'an ally next to it'
    case 'enemy': return 'an enemy next to it'
    case 'unit': return 'a unit next to it'
    case 'cell': { const [lo, hi] = rangeOf(a); return lo === hi ? `a free square ${squares(lo)} away` : `a free square ${lo} to ${squares(hi)} away` }
  }
}

function whatText(doc: ProjectDoc, a: Ability): string {
  const e = a.effect ?? {}
  switch (effectKind(a)) {
    case 'push': return `pushes ${whom(a)} ${squares(Number(e.push ?? 1))} away`
    case 'pull': return `steps back and pulls ${whom(a)} into its place`
    case 'swap': return `swaps places with ${whom(a)}`
    case 'moveBehind': return `puts ${whom(a)} on its other side`
    case 'movePast': return `moves to the far side of ${whom(a)}`
    case 'steal': {
      const s = stealOf(a)
      const what = s.weapons === true ? (s.equipped === true ? 'any item it carries' : 'an item it carries, weapons too (not the one in hand)') : 'an item it carries (not weapons)'
      return `takes ${what} from ${whom(a)}${s.if ? ` (only if ${s.if})` : ''}`
    }
    case 'refresh': { const n = refreshPerTurn(a); return `lets ${whom(a)} act again after it has acted${n > 1 ? ` (a unit can be refreshed ${times(n)} a turn)` : ''}` }
    case 'cure': {
      const ids = Array.isArray(e.cure) ? e.cure : []
      const names = ids.map((id) => doc.conditions?.find((c) => c.id === id)?.name ?? id)
      return `cures ${e.cure === 'all' ? 'every condition of' : names.length ? `${names.join(' and ')} on` : 'nothing on'} ${whom(a)}`
    }
    case 'unlock': {
      const u = unlockOf(a)
      const lock = u.needs ? ` (needs a ${doc.items?.find((i) => i.id === u.needs)?.name ?? u.needs}, which wears out)` : ''
      return `opens ${u.kinds === 'door' ? 'a locked door' : u.kinds === 'chest' ? 'a locked chest' : 'a locked door or chest'} on a square ${(() => { const [lo, hi] = rangeOf(a); return lo === hi ? `${squares(lo)} away` : `${lo} to ${squares(hi)} away` })()} without a key${lock}`
    }
    case 'heal': return `heals ${whom(a)} ${e.heal === 'full' ? 'fully' : `by ${e.heal ?? 0}`}`
    case 'summon': {
      const s = summonOf(a)
      const type = doc.units?.find((u) => u.id === s.type)?.name ?? (s.type || 'a unit')
      return `summons ${s.limit && s.limit > 1 ? `up to ${s.limit} ` : 'a '}${type} on ${whom(a)}`
    }
    default: return 'does something the editor does not show'
  }
}

export function abilitySummary(doc: ProjectDoc, a: Ability): string {
  const parts = [whatText(doc, a)]
  parts.push(a.cost === 'free' ? 'free (the unit can still act)' : 'uses the unit’s action')
  if (a.uses !== undefined) parts.push(`${times(a.uses)} per battle`)
  if (a.cooldown !== undefined) parts.push(`then waits ${a.cooldown} ${a.cooldown === 1 ? 'turn' : 'turns'}`)
  if (a.afterMove === false) parts.push('only before moving')
  const text = parts.join('; ')
  const cond = conditionsText(a.if)
  return `${text[0].toUpperCase()}${text.slice(1)}.${cond ? ` Only if ${cond}.` : ''}`
}
