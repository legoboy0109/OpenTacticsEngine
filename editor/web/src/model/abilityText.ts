// The plain one-line summary of an ability ("Pushes a unit next to it 1 square away. Uses the unit’s action."), shown under
// its form and as the list's second line. Wording is a placeholder for the owner to review (docs/SRPG.md "E3 as built").
import { conditionsText } from './abilityConditions'
import { areaOf } from './abilityArea'
import { inflictEntries } from './abilityInflicts'
import { effectKind, healKind, refreshPerTurn, rangeOf, stealOf, summonOf, targetOf, unlockOf } from './abilities'
import type { Ability } from './srpgTypes'
import type { ProjectDoc } from './types'

const squares = (n: number) => `${n} ${n === 1 ? 'square' : 'squares'}`
const times = (n: number) => `${n} ${n === 1 ? 'time' : 'times'}`

/** "the square it aims at and the ones next to it", "a cross of 2 squares around the square it aims at" ... */
function areaPhrase(a: Ability): string {
  const v = areaOf(a)
  if (!v) return 'an area'
  const up = v.vertical === undefined ? '' : v.vertical === 0 ? ', on the same level only'
    : `, up to ${v.vertical} ${v.vertical === 1 ? 'level' : 'levels'} up or down`
  switch (v.shape) {
    case 'radius': return `${squares(v.size ?? 1)} around the square it aims at${up}`
    case 'cross': return `a cross reaching ${squares(v.size ?? 1)} from the square it aims at${up}`
    case 'ring': return `a ring of ${squares(v.size ?? 1)} around the square it aims at${up}`
    case 'field': return 'the whole map'
    case 'line': return `a line of ${squares(v.size ?? 1)} from it${up}`
    default: return `the square it aims at and the ones next to it${up}`
  }
}

const whom = (a: Ability): string => {
  if (areaOf(a)) {
    const who = targetOf(a) === 'ally' ? 'every ally (itself too)' : targetOf(a) === 'enemy' ? 'every enemy' : 'every unit'
    return `${who} in ${areaPhrase(a)}`
  }
  switch (targetOf(a)) {
    case 'self': return 'itself'
    case 'ally': return 'an ally next to it'
    case 'enemy': return 'an enemy next to it'
    case 'unit': return 'a unit next to it'
    case 'cell': { const [lo, hi] = rangeOf(a); return lo === hi ? `a free square ${squares(lo)} away` : `a free square ${lo} to ${squares(hi)} away` }
  }
}

/** `who`: whom it cures (the heal part has already named them, so a heal + cure says "them"). */
function cureText(doc: ProjectDoc, a: Ability, who: string): string {
  const e = a.effect ?? {}
  const ids = Array.isArray(e.cure) ? e.cure : []
  const names = ids.map((id) => doc.conditions?.find((c) => c.id === id)?.name ?? id)
  if (e.cure === 'harmful') return `cures every harmful condition of ${who}`
  if (e.cure === 'helpful') return `takes every helpful condition away from ${who}`
  return `cures ${e.cure === 'all' ? 'every condition of' : names.length ? `${names.join(' and ')} on` : 'nothing on'} ${who}`
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
      return `takes ${what} from ${whom(a)}${s.if ? ` (only if ${s.if})` : ''}${s.if && s.reason ? `, otherwise “${s.reason}”` : ''}`
    }
    case 'refresh': { const n = refreshPerTurn(a); return `lets ${whom(a)} act again after it has acted${n > 1 ? ` (a unit can be refreshed ${times(n)} a turn)` : ''}` }
    case 'cure': return cureText(doc, a, whom(a))
    case 'unlock': {
      const u = unlockOf(a)
      const lock = u.needs ? ` (needs a ${doc.items?.find((i) => i.id === u.needs)?.name ?? u.needs}, which wears out)` : ''
      return `opens ${u.kinds === 'door' ? 'a locked door' : u.kinds === 'chest' ? 'a locked chest' : 'a locked door or chest'} on a square ${(() => { const [lo, hi] = rangeOf(a); return hi === 1 ? 'next to it' : lo === hi ? `${squares(lo)} away` : `${lo} to ${squares(hi)} away` })()} without a key${lock}`
    }
    case 'none': {
      const gives = inflictEntries(a).map((c) => {
        const name = doc.conditions?.find((x) => x.id === c.condition)?.name ?? (c.condition || 'a condition')
        return `${name}${c.turns ? ` for ${c.turns} ${c.turns === 1 ? 'turn' : 'turns'}` : ''}`
      })
      return `gives ${gives.length ? gives.join(' and ') : 'a condition'} to ${whom(a)}, and nothing else`
    }
    case 'heal': {
      const h = e.heal
      const how = healKind(a) === 'full' ? 'fully' : healKind(a) === 'percent' ? `by ${(h as { percent: number }).percent}% of its maximum` : healKind(a) === 'formula' ? `by a formula (${String(h)})` : `by ${h ?? 0}`
      return `heals ${whom(a)} ${how}${'cure' in e ? `, then ${cureText(doc, a, 'them')}` : ''}`
    }
    case 'strike': {
      const w = typeof e.strike === 'string' ? (doc.weapons?.find((x) => x.id === e.strike)?.name ?? e.strike) : ''
      return e.reach === 'equipped' ? `attacks with the damage of ${w || 'a weapon'}, from as far as the weapon in hand reaches`
        : `attacks with ${w || 'a weapon'}, aiming and reaching as the weapon does`
    }
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
  const cost = Object.entries(a.spend ?? {}).map(([id, n]) => `${n} ${doc.game?.unitResources?.find((r) => r.id === id)?.name ?? doc.game?.resources?.find((r) => r.id === id)?.name ?? id}`)
  if (cost.length) parts.push(`costs ${cost.join(' and ')}`)
  const text = parts.join('; ')
  const cond = conditionsText(a.if)
  return `${text[0].toUpperCase()}${text.slice(1)}.${cond ? ` Only if ${cond}.` : ''}`
}
