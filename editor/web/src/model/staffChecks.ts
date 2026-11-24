// Checks for staves (docs/SRPG.md section 8), in plain words. `blocks` = the loader refuses the game for it (mirrors
// engine/core/data/loader_staves.cpp, so a maker sees it before pressing Play; the loader's own message still shows in Problems after a
// save); the others load fine but a staff nobody can use is worth a word. Rank and lock mistakes are in rankChecks / lockChecks.
import { staffItems } from './gear'
import { featureEnabled } from './rules'
import { hasLadder } from './ranks'
import { mapHasFog } from './fog'
import { CONDITION_TURNS_MAX } from './conditions'
import { STAFF_EFFECTS_MAX, STAFF_HEAL_MAX, STAFF_RADIUS_MAX, STAFF_RANGE_MAX, STAFF_XP_MAX } from './staff'
import { STAFF_LIGHT_TURNS_MAX, STAFF_REPAIR_MAX, repairableItems } from './staffEffects'
import { staffReviveProblems } from './downedChecks'
import { MOVE_KINDS, fxKind, hasFx } from './staffFx'
import type { Extra, Item, ProjectDoc } from './types'

export interface StaffCheck { section: 'items'; ids: string[]; text: string; blocks: boolean }

const whole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

function rangeProblem(i: Item): string | undefined {
  const r = i.range
  if (r === undefined) return undefined
  if (i.target === 'self') return 'a staff used on its user takes no reach. Remove the reach or pick someone else to use it on.'
  if (typeof r === 'string') return r.trim() === '' ? 'the reach formula is empty.' : undefined
  if (whole(r)) return r < 1 || r > STAFF_RANGE_MAX ? `the reach must be from 1 to ${STAFF_RANGE_MAX} squares.` : undefined
  if (Array.isArray(r)) {
    if (r.length !== 2 || !whole(r[0]) || !whole(r[1])) return 'the reach must be two whole numbers: from and to.'
    if (r[0] < 1 || r[1] < r[0] || r[1] > STAFF_RANGE_MAX) return `the reach must go from 1 or more up to ${STAFF_RANGE_MAX} squares, and “from” cannot be above “to”.`
    return undefined
  }
  return 'the reach must be a number, two numbers (from and to) or a formula.'
}

function areaProblem(i: Item): string | undefined {
  const a = i.area
  if (a === undefined || a === 'single') return undefined
  if (i.target === 'self') return 'a staff used on its user has no area. Remove the area or pick someone else to use it on.'
  if (a === 'all') return undefined
  if (a && typeof a === 'object') {
    const n = (a as { radius?: unknown }).radius
    return whole(n) && n >= 1 && n <= STAFF_RADIUS_MAX ? undefined : `the area radius must be a whole number from 1 to ${STAFF_RADIUS_MAX} squares.`
  }
  return 'the area must be one unit, everyone in reach, or everyone within some squares.'
}

/** One effect on its own (kind and value); `n` numbers it the way the loader's "effects entry N" does. */
function effectProblem(doc: ProjectDoc, i: Item, e: unknown, n: number): string | undefined {
  const at = `effect ${n}`
  const kind = fxKind(e)
  if (!kind) return `${at} must be one thing, such as healing for 10.`
  const v = (e as Extra)[kind]
  switch (kind) {
    case 'heal': {
      if (i.target === 'foe') return 'healing cannot be used on foes. Pick who it can be used on.'
      if (typeof v === 'string') return v.trim() === '' ? 'the healing formula is empty.' : undefined
      if (whole(v)) return v < 1 || v > STAFF_HEAL_MAX ? `healing must be from 1 to ${STAFF_HEAL_MAX}.` : undefined
      if (v && typeof v === 'object') {
        const p = (v as { percent?: unknown }).percent
        return whole(p) && p >= 1 && p <= 100 ? undefined : 'a healing percentage must be a whole number from 1 to 100.'
      }
      return 'healing must be a number, a percentage, “full” or a formula.'
    }
    case 'warp': {
      if (!v || typeof v !== 'object' || Array.isArray(v)) return `${at}: a warp needs how far from the user the unit can land.`
      const extra = Object.keys(v).find((k) => k !== 'range')
      if (extra) return `${at}: a warp has no “${extra}”, only how far it reaches.`
      if (!('range' in v)) return `${at}: a warp needs how far from the user the unit can land.`
      const r = (v as { range?: unknown }).range
      if (typeof r === 'string') return r.trim() === '' ? 'the warp’s reach formula is empty.' : undefined
      if (whole(r)) return r < 1 || r > STAFF_RANGE_MAX ? `the warp’s reach must be from 1 to ${STAFF_RANGE_MAX} squares.` : undefined
      if (Array.isArray(r)) {
        if (r.length !== 2 || !whole(r[0]) || !whole(r[1])) return 'the warp’s reach must be two whole numbers: from and to.'
        return r[0] < 1 || r[1] < r[0] || r[1] > STAFF_RANGE_MAX ? `the warp’s reach must go from 1 or more up to ${STAFF_RANGE_MAX} squares, and “from” cannot be above “to”.` : undefined
      }
      return 'the warp’s reach must be a number, two numbers (from and to) or a formula.'
    }
    case 'pull':
    case 'swap':
      return v === true ? undefined : `${at}: “${kind}” must be switched on, not changed.`
    case 'repair':
      return v === 'full' || (whole(v) && v >= 1 && v <= STAFF_REPAIR_MAX) ? undefined : `repairing must be all uses or a number of uses from 1 to ${STAFF_REPAIR_MAX}.`
    case 'light': {
      if (!v || typeof v !== 'object' || Array.isArray(v)) return 'a light needs a radius and a number of turns.'
      const extra = Object.keys(v).find((k) => k !== 'radius' && k !== 'turns' && k !== 'shrink')
      if (extra) return `a light has no “${extra}”, only a radius, a number of turns and how much it shrinks each turn.`
      const { radius, turns, shrink } = v as { radius?: unknown; turns?: unknown; shrink?: unknown }
      if (shrink !== undefined && (!whole(shrink) || shrink < 1 || !whole(radius) || shrink >= radius)) return 'the light’s shrink must be a whole number of 1 or more and less than its radius (leave it out for a light that does not shrink).'
      if (!whole(radius) || radius < 1 || radius > STAFF_RADIUS_MAX) return `the light’s radius must be a whole number from 1 to ${STAFF_RADIUS_MAX} squares.`
      if (turns !== undefined && (!whole(turns) || turns < 1 || turns > STAFF_LIGHT_TURNS_MAX)) return `the light’s turns must be a whole number from 1 to ${STAFF_LIGHT_TURNS_MAX} (leave it out for 1).`
      return undefined
    }
    case 'unlock': {
      const ok = v === true || (Array.isArray(v) && v.length > 0 && v.every((x) => x === 'door' || x === 'chest'))
      return ok ? undefined : 'unlocking must be for doors, chests or both.'
    }
    case 'revive': return staffReviveProblems(doc, i, v, (i.effects as unknown[] | undefined)?.length === 1)[0]
    case 'inflict': return inflictProblem(doc, v, at)
    case 'cure': return cureProblem(doc, v, at)
    default:
      return `${at} is “${kind}”, which a staff cannot do (a staff can heal, revive, warp, pull, swap, repair, light, inflict a condition, cure or unlock).`
  }
}

const noSuchCondition = (doc: ProjectDoc, id: string): string =>
  doc.conditions?.length ? `the condition “${id}” does not exist (see the Conditions screen).` : `the game has no conditions yet, so “${id}” cannot be named (add them on the Conditions screen).`

function inflictProblem(doc: ProjectDoc, v: unknown, at: string): string | undefined {
  if (!v || typeof v !== 'object' || Array.isArray(v) || typeof (v as Extra).condition !== 'string') return `${at}: inflicting needs a condition, such as { "condition": "poison" }.`
  const o = v as Extra
  const extra = Object.keys(o).find((k) => k !== 'condition' && k !== 'turns')
  if (extra) return `${at}: inflicting has no “${extra}”, only a condition and optional turns.`
  const c = doc.conditions?.find((x) => x.id === o.condition)
  if (!c) return `${at}: ${noSuchCondition(doc, String(o.condition))}`
  if (o.turns !== undefined) {
    if (!whole(o.turns) || o.turns < 1 || o.turns > CONDITION_TURNS_MAX) return `${at}: the turns must be a whole number from 1 to ${CONDITION_TURNS_MAX}.`
    if (c.until !== undefined) return `${at}: ${c.name} does not count turns (it lasts until it is cured or the map ends), so the turns mean nothing. Remove them.`
  }
  return undefined
}

function cureProblem(doc: ProjectDoc, v: unknown, at: string): string | undefined {
  if (v === 'all') return undefined
  if (v === 'harmful' || v === 'helpful') return (doc.conditions ?? []).length ? undefined : `${at}: it cures every ${v} condition, but the game has no conditions yet.`
  if (!Array.isArray(v) || v.length === 0) return `${at}: curing needs a list of conditions, or every condition.`
  const seen = new Set<string>()
  for (const x of v) {
    if (typeof x !== 'string') return `${at}: curing lists conditions by their ids.`
    if (!doc.conditions?.some((c) => c.id === x)) return `${at}: ${noSuchCondition(doc, x)}`
    if (seen.has(x)) return `${at}: “${x}” is listed twice.`
    seen.add(x)
  }
  return undefined
}

/** What the effects may be put together with (the loader's checkCombination). */
function combinationProblem(i: Item, fx: unknown[]): string | undefined {
  const kinds = fx.map(fxKind)
  const count = (set: readonly string[]) => kinds.filter((k) => !!k && set.includes(k)).length
  const moves = count(MOVE_KINDS), repairs = count(['repair']), lights = count(['light']), unlocks = count(['unlock'])
  if (unlocks > 1) return 'a staff can have only one unlock.'
  if (unlocks) {
    if (fx.length !== 1) return 'unlocking picks a door or chest, not a unit, so it cannot be put together with another effect.'
    if (i.target !== undefined) return 'unlocking picks a square of a door or chest, so the staff cannot say who it is used on. Remove the target.'
    if (i.area !== undefined) return 'unlocking opens the one door or chest picked, so the staff has no area. Remove the area.'
    if (i.if !== undefined) return 'unlocking picks a door or chest, so there is no unit for “Only if” to judge. Remove the conditions.'
    return undefined
  }
  if (moves > 1) return 'a staff can have only one of warp, pull and swap (each one decides where the unit ends up).'
  if (repairs > 1) return 'a staff can have only one repair.'
  if (lights > 1) return 'a staff can have only one light.'
  if (count(['inflict']) > 1) return 'a staff can have only one “inflicts a condition” (make two staves, or two conditions that replace each other).'
  if (count(['cure']) > 1) return 'a staff can have only one “cures” (it takes a list of conditions).'
  if (repairs && moves) return 'repairing cannot be put together with warp, pull or swap (each one asks the player for a second pick).'
  if (lights) {
    if (fx.length !== 1) return 'a light picks a square, not a unit, so it cannot be put together with another effect.'
    if (i.target !== undefined) return 'a light picks a square, so the staff cannot say who it is used on. Remove the target.'
    if (i.area !== undefined) return 'a light picks a square, so the staff has no area (the light has its own radius). Remove the area.'
    if (i.hit !== undefined) return 'a light picks a square, so there is no unit to roll a chance of missing for. Remove the chance.'
    if (i.if !== undefined) return 'a light picks a square, so there is no unit for “Only if” to judge. Remove the conditions.'
    return undefined
  }
  if (moves || repairs) {
    const what = repairs ? 'repairing' : kinds.includes('warp') ? 'a warp' : kinds.includes('pull') ? 'a pull' : 'a swap'
    if (i.area !== undefined && i.area !== 'single') return `${what} works on the one unit picked, so what it affects must be “one unit”.`
    if ((kinds.includes('pull') || kinds.includes('swap')) && i.target === 'self') return `${what} needs another unit, so it cannot be used only on the user.`
  }
  return undefined
}

/** Each effect by itself first; when all are fine, what they are put together with. */
function effectProblems(doc: ProjectDoc, i: Item): string[] {
  const fx = i.effects as unknown[] | undefined
  if (!Array.isArray(fx) || fx.length < 1 || fx.length > STAFF_EFFECTS_MAX) return [`a staff needs 1 to ${STAFF_EFFECTS_MAX} effects (it can heal, move a unit, repair an item, light an area, inflict a condition, cure or unlock).`]
  const each = fx.map((e, n) => effectProblem(doc, i, e, n + 1)).filter((p): p is string => !!p)
  if (each.length > 0) return each
  const combo = combinationProblem(i, fx)
  return combo ? [combo] : []
}

function hitXpProblems(doc: ProjectDoc, i: Item): string[] {
  const out: string[] = []
  if (i.hit !== undefined) {
    if (typeof i.hit === 'string' ? i.hit.trim() === '' : !whole(i.hit) || i.hit < 0 || i.hit > 100) out.push('the chance of working must be a percentage from 0 to 100, or a formula.')
  }
  if (i.xp !== undefined) {
    if (!featureEnabled(doc, 'progression')) out.push('it gives experience, but the game has no experience yet. Turn it on in the Rules screen, or clear the experience.')
    else if (typeof i.xp === 'string' ? i.xp.trim() === '' : !whole(i.xp) || i.xp < 0 || i.xp > STAFF_XP_MAX) out.push(`the experience must be a whole number from 0 to ${STAFF_XP_MAX}, or a formula.`)
  }
  return out
}

export function staffChecks(doc: ProjectDoc): StaffCheck[] {
  const out: StaffCheck[] = []
  const anyFog = Object.keys(doc.maps).some((m) => mapHasFog(doc, m))
  for (const i of staffItems(doc)) {
    const ids = [i.id]
    const say = (text: string, blocks = true) => out.push({ section: 'items', ids, text: `${i.name}: ${text}`, blocks })
    if (!i.group || i.group.trim() === '') say('a staff needs a group (the kind of staff a class can use, such as “staff”).')
    for (const p of [rangeProblem(i), areaProblem(i), ...effectProblems(doc, i), ...hitXpProblems(doc, i)]) if (p) say(p)
    if ((i.rank !== undefined || i.rankXp !== undefined) && !hasLadder(doc)) say('it needs a rank, but the game has no weapon ranks. Add them on the Rules screen, or clear the rank.')
    if (i.wieldedBy?.length && (doc.characters?.length ?? 0) === 0) say('it is locked to characters, but the game has none.')
    // Loads fine, can never be used: no class lists its group.
    if (i.group && (doc.units?.length ?? 0) > 0 && !doc.units!.some((u) => u.wields?.includes(i.group!)))
      say(`nobody can use it yet: no class can wield its group (“${i.group}”). Tick the group for a class on the Units screen.`, false)
    // Loads fine, but the staff can never do anything in this game.
    if (hasFx(i, ['light']) && !anyFog)
      say('it lights up squares in fog of war, but no map of this game has fog, so it can never be used. Switch fog of war on for a map.', false)
    if (hasFx(i, ['repair']) && repairableItems(doc).length === 0)
      say('it repairs items, but the game has no item that can be repaired (an item needs a limited number of uses and must not be marked as not repairable).', false)
  }
  return out
}
