// Checks for staves (docs/SRPG.md section 8), in plain words. `blocks` = the loader refuses the game for it (mirrors
// engine/core/data/loader_staves.cpp, so a maker sees it before pressing Play; the loader's own message still shows in Problems after a
// save); the others load fine but a staff nobody can use is worth a word. Rank and lock mistakes are in rankChecks / lockChecks.
import { staffItems } from './gear'
import { featureEnabled } from './rules'
import { hasLadder } from './ranks'
import { STAFF_EFFECTS_MAX, STAFF_HEAL_MAX, STAFF_RADIUS_MAX, STAFF_RANGE_MAX, STAFF_XP_MAX } from './staff'
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

function healProblem(i: Item): string | undefined {
  const fx = i.effects as Extra[] | undefined
  if (!fx || fx.length < 1 || fx.length > STAFF_EFFECTS_MAX) return `a staff needs 1 to ${STAFF_EFFECTS_MAX} effects (it can heal).`
  for (const e of fx) {
    if (!e || typeof e !== 'object' || !('heal' in e)) return 'a staff can only heal for now: this effect is not a heal.'
    if (i.target === 'foe') return 'healing cannot be used on foes. Pick who it can be used on.'
    const h = e.heal
    if (typeof h === 'string') { if (h.trim() === '') return 'the healing formula is empty.'; continue }
    if (whole(h)) { if (h < 1 || h > STAFF_HEAL_MAX) return `healing must be from 1 to ${STAFF_HEAL_MAX}.`; continue }
    if (h && typeof h === 'object') {
      const p = (h as { percent?: unknown }).percent
      if (!whole(p) || p < 1 || p > 100) return 'a healing percentage must be a whole number from 1 to 100.'
      continue
    }
    return 'healing must be a number, a percentage, “full” or a formula.'
  }
  return undefined
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
  for (const i of staffItems(doc)) {
    const ids = [i.id]
    const say = (text: string, blocks = true) => out.push({ section: 'items', ids, text: `${i.name}: ${text}`, blocks })
    if (!i.group || i.group.trim() === '') say('a staff needs a group (the kind of staff a class can use, such as “staff”).')
    for (const p of [rangeProblem(i), areaProblem(i), healProblem(i), ...hitXpProblems(doc, i)]) if (p) say(p)
    if ((i.rank !== undefined || i.rankXp !== undefined) && !hasLadder(doc)) say('it needs a rank, but the game has no weapon ranks. Add them on the Rules screen, or clear the rank.')
    if (i.wieldedBy?.length && (doc.characters?.length ?? 0) === 0) say('it is locked to characters, but the game has none.')
    // Loads fine, can never be used: no class lists its group.
    if (i.group && (doc.units?.length ?? 0) > 0 && !doc.units!.some((u) => u.wields?.includes(i.group!)))
      say(`nobody can use it yet: no class can wield its group (“${i.group}”). Tick the group for a class on the Units screen.`, false)
  }
  return out
}
