// Editor-side checks for weapon ranks and stat caps (docs/SRPG.md section 7 "Editor"), beside the loader's own refusals (which reach the
// Problems list in the engine's words): these name the same mistakes where they are made, plus one that loads fine but changes
// nothing ("no weapon needs a rank"). `ids` names the weapons / unit types / characters a line is about, so a form shows only its own.
import type { SrpgCheck } from './abilityChecks'
import { allGear } from './gear'
import { MAX_CAP, MAX_RANKS, MAX_RANK_XP, MAX_USE_XP, RANK_ID, canSwitchOnRanks, groupsOf, ladderOf } from './ranks'
import type { RankMap } from './srpgTypes'
import type { ProjectDoc } from './types'

/** One starting-ranks map: groups no weapon has, rank ids not in the ladder, numbers above the top rank. */
function mapChecks(doc: ProjectDoc, who: string, map: RankMap | undefined, ids: string[], section: SrpgCheck['section']): SrpgCheck[] {
  const ladder = ladderOf(doc)
  const groups = groupsOf(doc)
  const top = ladder[ladder.length - 1]?.xp ?? 0
  const out: SrpgCheck[] = []
  for (const [g, v] of Object.entries(map ?? {})) {
    if (!groups.includes(g)) out.push({ section, ids, text: `${who} has a rank in “${g}”, but no weapon or staff is in that group.` })
    else if (typeof v === 'string' && !ladder.some((r) => r.id === v)) out.push({ section, ids, text: `${who} starts in a rank (“${v}”) that is not in the ladder.` })
    else if (typeof v === 'number' && (v < 0 || v > top)) out.push({ section, ids, text: `${who}: the experience for “${g}” must be between 0 and ${top}.` })
  }
  return out
}

function ladderChecks(doc: ProjectDoc): SrpgCheck[] {
  const ladder = ladderOf(doc)
  const out: SrpgCheck[] = []
  const add = (text: string) => out.push({ section: 'rules', text })
  if (ladder.length > 0 && !canSwitchOnRanks(doc)) add('Weapon ranks need weapons that units carry. Set “Weapons” to carried in the Items part of the rules.')
  if (ladder.length > MAX_RANKS) add(`A ladder can have at most ${MAX_RANKS} ranks.`)
  if (ladder[0] && ladder[0].xp !== 0) add(`The first rank (${ladder[0].name}) needs 0 experience: every unit has at least that rank.`)
  ladder.forEach((r, i) => {
    if (i > 0 && r.xp <= ladder[i - 1].xp) add(`${r.name} needs more experience than ${ladder[i - 1].name} (${ladder[i - 1].xp}). Experience must rise up the ladder.`)
    if (r.xp > MAX_RANK_XP) add(`${r.name}: experience can be at most ${MAX_RANK_XP}.`)
    if (!RANK_ID.test(r.id)) add(`${r.name}: the id may only hold lowercase letters, digits, “_” and “-”.`)
    if (ladder.findIndex((x) => x.id === r.id) !== i) add(`Two ranks use the id “${r.id}”.`)
  })
  // Loads fine, does nothing: a ladder that no weapon asks for.
  const gear = allGear(doc)
  if (ladder.length > 0 && gear.some((g) => g.keys.group) && !gear.some((g) => g.keys.rank))
    add(gear.some((g) => g.owner === 'item') ? 'No weapon or staff needs a rank yet, so ranks change nothing. Pick a required rank on a weapon (Weapons screen) or a staff (Items screen).' : 'No weapon needs a rank yet, so ranks change nothing. Pick a required rank on a weapon (Weapons screen).')
  return out
}

export function rankChecks(doc: ProjectDoc): SrpgCheck[] {
  if (doc.ranks === null) return []
  const ladder = ladderOf(doc)
  const out = ladderChecks(doc)
  // Weapons and staves alike (model/gear.ts).
  for (const g of allGear(doc)) {
    const ids = [g.id]
    const { name } = g
    const w = g.keys
    const section = g.section
    if (w.rank !== undefined && !w.group) out.push({ section, ids, text: `${name} needs a rank, but it has no group. Ranks are kept per weapon group: give it a group.` })
    else if (w.rank !== undefined && ladder.length > 0 && !ladder.some((r) => r.id === w.rank)) out.push({ section, ids, text: `${name} needs a rank (“${w.rank}”) that is not in the ladder.` })
    if (w.rankXp !== undefined && !w.group) out.push({ section, ids, text: `${name} earns weapon experience, but it has no group.` })
    if (w.rankXp !== undefined && (w.rankXp < 0 || w.rankXp > MAX_USE_XP)) out.push({ section, ids, text: `${name}: experience per use must be from 0 to ${MAX_USE_XP} (0 trains nothing).` })
  }
  for (const u of doc.units ?? []) out.push(...mapChecks(doc, `${u.name}`, u.ranks, [u.id], 'units'))
  for (const c of doc.characters ?? []) out.push(...mapChecks(doc, `${c.name}`, c.ranks, [c.id], 'characters'))
  return out
}

/** Caps with a number the loader would refuse, or for a stat the game no longer has. */
export function capChecks(doc: ProjectDoc): SrpgCheck[] {
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const name = (s: string) => doc.game?.stats.find((x) => x.id === s)?.name ?? s
  const out: SrpgCheck[] = []
  const look = (m: Record<string, number> | undefined, who: string, section: SrpgCheck['section'], ids: string[] | undefined, adjust: boolean) => {
    for (const [s, v] of Object.entries(m ?? {})) {
      if (!stats.has(s)) out.push({ section, ids, text: `${who}: “${s}” is not a stat of this game.` })
      else if (!adjust && (v < 1 || v > MAX_CAP)) out.push({ section, ids, text: `${who}: the cap for ${name(s)} must be from 1 to ${MAX_CAP}.` })
    }
  }
  look(doc.caps?.caps, 'Stat caps', 'rules', undefined, false)
  for (const u of doc.units ?? []) look(u.caps, u.name, 'units', [u.id], false)
  for (const c of doc.characters ?? []) look(c.capAdjust, c.name, 'characters', [c.id], true)
  return out
}
