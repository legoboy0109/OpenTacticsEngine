// The `fight` command (docs/EVENTS.md "A staged fight"): two named map units fight, with every strike written down or with real rolls.
// Checks and edits mirror the loader (engine/core/data/loader_story_fight.cpp); defaults are never written, unknown keys stay.
import { unitText, type SumCtx } from './storySummary'
import { DROP, type Walk } from './storyWalk'
import type { Extra, ProjectDoc } from './types'

export type StrikeResult = 'hit' | 'miss' | 'crit'
export const STRIKE_RESULTS: StrikeResult[] = ['hit', 'miss', 'crit']
export interface FightStrike extends Extra { by: 'attacker' | 'defender'; result: StrikeResult; damage?: number }
export interface FightSpec extends Extra {
  attacker: string
  defender: string
  strikes?: FightStrike[]
  weapon?: { attacker?: string; defender?: string }
  lethal?: boolean
  xp?: boolean
  wear?: boolean
}
export const MAX_FIGHT_STRIKES = 24

export const fightOf = (c: { fight?: FightSpec }): FightSpec => (c.fight && typeof c.fight === 'object' ? c.fight : { attacker: '', defender: '' })

/** A new staged fight: the first two named units, the attacker strikes, the defender answers; real strikes only when the maker asks for them. */
export const newFight = (units: string[]): FightSpec => ({ attacker: units[0] ?? '', defender: units[1] ?? units[0] ?? '' })

/** A flag at its default (lethal true, xp / wear false) is removed rather than written. */
export function setFightFlag(f: FightSpec, key: 'lethal' | 'xp' | 'wear', on: boolean): FightSpec {
  const next: FightSpec = { ...f }
  if (key === 'lethal' ? on : !on) delete next[key]
  else next[key] = on
  return next
}

export function setFightWeapon(f: FightSpec, who: 'attacker' | 'defender', weapon: string): FightSpec {
  const w = { ...(f.weapon ?? {}) }
  if (weapon === '') delete w[who]
  else w[who] = weapon
  const next: FightSpec = { ...f }
  if (Object.keys(w).length) next.weapon = w
  else delete next.weapon
  return next
}

/** "Strikes written down" on / off: on starts with the exchange a plain fight would most likely be (the attacker hits, the defender answers). */
export function setScripted(f: FightSpec, on: boolean): FightSpec {
  const next: FightSpec = { ...f }
  if (on) next.strikes = Array.isArray(f.strikes) && f.strikes.length ? f.strikes : [{ by: 'attacker', result: 'hit' }, { by: 'defender', result: 'hit' }]
  else delete next.strikes
  return next
}

export function addStrike(f: FightSpec): FightSpec {
  const list = Array.isArray(f.strikes) ? f.strikes : []
  if (list.length >= MAX_FIGHT_STRIKES) return f
  const last = list[list.length - 1]
  return { ...f, strikes: [...list, { by: last?.by === 'attacker' ? 'defender' : 'attacker', result: 'hit' }] }
}

export function removeStrike(f: FightSpec, i: number): FightSpec {
  const list = (f.strikes ?? []).filter((_, k) => k !== i)
  return { ...f, strikes: list.length ? list : [{ by: 'attacker', result: 'hit' }] }  // the loader wants at least one: use "Strikes written down" off for none
}

export function moveStrike(f: FightSpec, from: number, to: number): FightSpec {
  const list = [...(f.strikes ?? [])]
  if (from < 0 || to < 0 || from >= list.length || to >= list.length) return f
  const [s] = list.splice(from, 1)
  list.splice(to, 0, s)
  return { ...f, strikes: list }
}

/** A strike's result or striker; a miss does no damage, so its damage is dropped. */
export function patchStrike(f: FightSpec, i: number, patch: Partial<FightStrike>): FightSpec {
  const list = (f.strikes ?? []).map((s, k) => {
    if (k !== i) return s
    const next: FightStrike = { ...s, ...patch }
    if (next.result === 'miss') delete next.damage
    if (next.damage === undefined) delete next.damage
    return next
  })
  return { ...f, strikes: list }
}

/** What the loader refuses, in plain words. Unknown units and weapons are the walk's (they are references). */
export function fightProblems(doc: ProjectDoc, c: { fight?: FightSpec }, unitIds: string[]): string[] {
  const out: string[] = []
  const f = c.fight
  if (!f || typeof f !== 'object') return ['the fight needs an attacker and a defender.']
  if (!f.attacker) out.push('choose the unit that starts the fight.')
  else if (!unitIds.includes(f.attacker)) out.push(`the attacker “${f.attacker}” is not a named unit of this map.`)
  if (!f.defender) out.push('choose the unit that is attacked.')
  else if (!unitIds.includes(f.defender)) out.push(`the defender “${f.defender}” is not a named unit of this map.`)
  if (f.attacker && f.attacker === f.defender) out.push('a unit cannot fight itself.')
  for (const who of ['attacker', 'defender'] as const) {
    const w = f.weapon?.[who]
    if (w !== undefined && !doc.weapons?.some((x) => x.id === w)) out.push(`the ${who}’s weapon “${w}” is not a weapon of this game.`)
  }
  if (f.strikes !== undefined) {
    if (!Array.isArray(f.strikes) || f.strikes.length === 0 || f.strikes.length > MAX_FIGHT_STRIKES) out.push(`the strikes must be a list of 1 to ${MAX_FIGHT_STRIKES}.`)
    else
      f.strikes.forEach((s, i) => {
        if (s.by !== 'attacker' && s.by !== 'defender') out.push(`strike ${i + 1}: who strikes must be the attacker or the defender.`)
        if (!STRIKE_RESULTS.includes(s.result)) out.push(`strike ${i + 1}: the result must be hit, miss or crit.`)
        if (s.damage !== undefined && (!Number.isInteger(s.damage) || s.damage < 0 || s.damage > 9999)) out.push(`strike ${i + 1}: the damage must be a whole number from 0 to 9999.`)
        if (s.result === 'miss' && s.damage !== undefined && s.damage !== 0) out.push(`strike ${i + 1}: a miss does no damage.`)
      })
  }
  if ((f.xp === true) && doc.game?.rules?.progression?.enabled !== true) out.push('experience for the fight needs experience to be switched on (Rules screen).')
  return out
}

/** The walk over a `fight`: both fighters are unit story names (renamed with the unit; a deleted unit takes the command away). */
export function withFightUnits<T extends { fight?: unknown }>(c: T, w: Walk, where: string): T | typeof DROP {
  const f = c.fight as FightSpec | undefined
  if (!f || typeof f !== 'object') return c
  let next = f
  for (const k of ['attacker', 'defender'] as const) {
    const v = next[k]
    if (typeof v !== 'string' || v === '') continue
    const r = w.ref({ kind: 'unit', id: v }, where)
    if (r === DROP) return DROP
    if (r !== v) next = { ...next, [k]: r }
  }
  // The weapons named for the two fighters (a weapon of data/weapons.json): a deleted weapon leaves the fight as it is, with that fighter's own weapon.
  if (next.weapon && typeof next.weapon === 'object') {
    let wp = next.weapon
    for (const k of ['attacker', 'defender'] as const) {
      const v = wp[k]
      if (typeof v !== 'string' || v === '') continue
      const r = w.ref({ kind: 'weapon', id: v }, where)
      if (r === DROP) {
        const { [k]: _gone, ...rest } = wp
        wp = rest
        w.dropped?.({ what: 'setting', text: `A staged fight: the ${k}’s weapon “${v}” goes (it uses its own weapon)`, where })
      } else if (r !== v) wp = { ...wp, [k]: r }
    }
    if (wp !== next.weapon) {
      const { weapon: _w, ...rest } = next
      next = Object.keys(wp).length ? { ...rest, weapon: wp } : (rest as FightSpec)
    }
  }
  return next === f ? c : { ...c, fight: next }
}

/** "Eirik fights Brigand: 3 strikes written down (no dice), nobody dies, gives experience". */
export function fightText(c: SumCtx, cmd: { fight?: FightSpec }): string {
  const f = fightOf(cmd)
  const strikes = Array.isArray(f.strikes) ? `${f.strikes.length} ${f.strikes.length === 1 ? 'strike' : 'strikes'} written down` : 'a real fight with real dice'
  const extras = [f.lethal === false ? 'nobody dies' : '', f.xp === true ? 'gives experience' : '', f.wear === true ? 'wears the weapons' : ''].filter(Boolean)
  return `${unitText(c, f.attacker)} fights ${unitText(c, f.defender)}: ${[strikes, ...extras].join(', ')}`
}
