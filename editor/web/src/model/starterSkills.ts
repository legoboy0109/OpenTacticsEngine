// The starter set of skills (docs/SRPG.md section 3 "The starter set"): thirty plain-data skills that the editor can install
// into any game. Stats are written as `$atk`, `$skl`, `$spd`, `$def` (the four the maker points at when installing) and
// `$hp`, `$move` (the game's own hit points and movement stats); a skill whose stat is unset is not installed.
// editor/templates/strategy-rpg/data/skills.json is this same list with the template's stat ids (a unit test compares them).
// Names and descriptions are placeholders for the owner to review.
import { withSkills } from './doc'
import type { Skill } from './srpgTypes'
import type { ProjectDoc } from './types'

export type StatRole = 'atk' | 'skl' | 'spd' | 'def' | 'hp' | 'move'
/** The four the installer asks for; `hp` and `move` come from the game's roles. */
export const PICKED_ROLES: Array<'atk' | 'skl' | 'spd' | 'def'> = ['atk', 'skl', 'spd', 'def']
export type StatMap = Partial<Record<StatRole, string>>

const sk = (id: string, name: string, description: string, ...blocks: object[]) => ({ id, name, description, blocks }) as Skill

export const STARTER_SKILLS: Skill[] = [
  sk('tough', 'Tough', 'More hit points.', { when: 'always', do: [{ stats: { $hp: 5 } }] }),
  sk('light_step', 'Light Step', 'Moves one square farther.', { when: 'always', do: [{ stats: { $move: 1 } }] }),
  sk('sure_footing', 'Sure Footing', 'Rough ground never costs more than one step.', { when: 'always', do: [{ maxMoveCost: 1 }] }),
  sk('slip_through', 'Slip Through', 'Can move through enemy units.', { when: 'always', do: [{ pass: 'enemies' }] }),
  sk('strike_and_move', 'Strike and Move', 'After acting, can still move with the movement it has left.', { when: 'always', do: [{ moveAgain: true }] }),
  sk('fast_learner', 'Fast Learner', 'Gains experience faster.', { when: 'always', do: [{ xpPercent: 150 }] }),
  sk('gifted', 'Gifted', 'Grows faster in four stats.', { when: 'always', do: [{ growths: { $atk: 10, $skl: 10, $spd: 10, $def: 10 } }] }),
  sk('opening_blow', 'Opening Blow', 'Hits harder when it starts the fight.', { when: 'combat', if: [{ initiating: true }], do: [{ damage: 3 }] }),
  sk('hold_the_line', 'Hold the Line', 'Takes less damage when defending.', { when: 'combat', if: [{ initiating: false }], do: [{ damageTaken: -3 }] }),
  sk('back_to_the_wall', 'Back to the Wall', 'Critical chance up at half HP or less.', { when: 'combat', if: [{ hp: { op: '<=', percent: 50 } }], do: [{ crit: 25 }] }),
  sk('on_my_own', 'On My Own', 'Fights better with no ally nearby.', { when: 'combat', if: [{ near: 'ally', within: 2, op: '=', count: 0 }], do: [{ damage: 3 }, { hit: 10 }] }),
  sk('close_fighter', 'Close Fighter', 'Harder to hit in close combat.', { when: 'combat', if: [{ distance: { op: '=', value: 1 } }], do: [{ avoid: 15 }] }),
  sk('steady_aim', 'Steady Aim', 'Hits more often from a distance.', { when: 'combat', if: [{ distance: { op: '>=', value: 2 } }], do: [{ hit: 20 }] }),
  sk('beat_them_to_it', 'Beat Them to It', 'When defending at half HP or less, strikes first.', { when: 'combat', if: [{ initiating: false }, { hp: { op: '<=', percent: 50 } }], do: [{ strikeFirst: true }] }),
  sk('double_strike', 'Double Strike', 'Each strike is made twice when it starts the fight.', { when: 'combat', if: [{ initiating: true }], do: [{ multiStrike: 2 }] }),
  sk('always_ready', 'Always Ready', 'Counterattacks from any distance.', { when: 'combat', do: [{ counterAnyRange: true }] }),
  sk('clean_hit', 'Clean Hit', 'At full HP, the foe cannot counterattack when it starts the fight.', { when: 'combat', if: [{ initiating: true }, { hp: { op: '=', percent: 100 } }], do: [{ noCounter: true }] }),
  sk('braced', 'Braced', 'When defending, the foe strikes one time fewer.', { when: 'combat', if: [{ initiating: false }], do: [{ strikes: -1, to: 'foe' }] }),
  sk('rallying_presence', 'Rallying Presence', 'Allies nearby hit and avoid more.', { when: 'combat', aura: { to: 'allies', within: 2 }, do: [{ hit: 10 }, { avoid: 10 }] }),
  sk('unnerving_presence', 'Unnerving Presence', 'Enemies nearby are easier to hit.', { when: 'combat', aura: { to: 'enemies', within: 2 }, do: [{ avoid: -10 }] }),
  sk('heavy_blow', 'Heavy Blow', 'A chance to deal double damage.', { when: 'strike', chance: 'a.$skl', do: [{ damagePercent: 200 }] }),
  sk('draining_blow', 'Draining Blow', 'A chance to heal by half of the damage dealt.', { when: 'strike', chance: 'a.$skl', do: [{ drain: 50 }] }),
  sk('extra_swing', 'Extra Swing', 'A chance to strike once more.', { when: 'strike', chance: 'a.$spd', do: [{ strikeAgain: true }] }),
  sk('glancing_hit', 'Glancing Hit', 'A chance to take half damage.', { when: 'struck', chance: 'a.$def * 2', do: [{ damageTakenPercent: 50 }] }),
  sk('hang_on', 'Hang On', 'At half HP or more, survives a deadly strike with 1 HP.', { when: 'struck', if: [{ hp: { op: '>=', percent: 50 } }], do: [{ survive: true }] }),
  sk('second_charge', 'Second Charge', 'After defeating a foe it started fighting, can move and act again.', { when: 'defeat', if: [{ initiating: true }], do: [{ actAgain: true }] }),
  sk('catch_breath', 'Catch Breath', 'Heals a quarter of its HP after defeating a foe.', { when: 'defeat', do: [{ heal: { percent: 25 } }] }),
  sk('parting_jab', 'Parting Jab', 'After starting a fight, hurts the foe a little.', { when: 'afterCombat', if: [{ initiating: true }], do: [{ hurt: 5, to: 'foe' }] }),
  sk('steady_recovery', 'Steady Recovery', 'Heals a tenth of its HP at the start of its turn.', { when: 'turnStart', do: [{ heal: { percent: 10 } }] }),
  sk('caring_presence', 'Caring Presence', 'Heals allies next to it at the start of its turn.', { when: 'turnStart', do: [{ heal: 5, to: 'allies', within: 1 }] }),
]

const ROLE_PATTERN = /\$(atk|skl|spd|def|hp|move)\b/g
/** The roles a skill's data needs. */
export function rolesNeeded(skill: Skill): StatRole[] {
  return [...new Set([...JSON.stringify(skill).matchAll(ROLE_PATTERN)].map((m) => m[1] as StatRole))]
}
/** The skill with the stat ids filled in, or null when a stat it needs is not set. */
export function resolveSkill(skill: Skill, map: StatMap): Skill | null {
  if (rolesNeeded(skill).some((r) => !map[r])) return null
  return JSON.parse(JSON.stringify(skill).replace(ROLE_PATTERN, (_m, r: StatRole) => map[r]!)) as Skill
}

const HINTS: Record<'atk' | 'skl' | 'spd' | 'def', string[]> = {
  atk: ['str', 'strength', 'atk', 'attack', 'might', 'power'], skl: ['skl', 'skill', 'dex', 'dexterity'], spd: ['spd', 'speed', 'agi', 'agility'],
  def: ['def', 'defense', 'defence', 'armor', 'armour', 'guard'],
}
/** The pickers' starting values: stats whose id or name matches the usual one, else the stats the damage and hit formulas
 *  already use (`a.x` in damage = attack, `b.x` in damage = defense, `a.x` in hit = skill, `b.x` in hit = speed). */
export function guessStats(doc: ProjectDoc): StatMap {
  const stats = doc.game?.stats ?? []
  const out: StatMap = { hp: doc.game?.roles?.hp, move: doc.game?.roles?.move }
  const f = doc.game?.formulas ?? {}
  const from = (formula: unknown, who: 'a' | 'b'): string | undefined => {
    const m = typeof formula === 'string' ? new RegExp(`\\b${who}\\.([A-Za-z_][A-Za-z0-9_]*)`).exec(formula) : null
    return m && stats.some((s) => s.id === m[1]) ? m[1] : undefined
  }
  const fallback = { atk: from(f.damage, 'a'), def: from(f.damage, 'b'), skl: from(f.hit, 'a'), spd: from(f.hit, 'b') }
  for (const r of PICKED_ROLES) {
    const hit = stats.find((s) => HINTS[r].includes(s.id.toLowerCase())) ?? stats.find((s) => HINTS[r].includes(s.name.toLowerCase()))
    out[r] = hit?.id ?? fallback[r]
  }
  return out
}

export interface InstallResult { doc: ProjectDoc; added: string[]; existing: string[]; missingStat: string[] }
/** Adds the chosen skills. A skill whose id already exists is skipped (and listed), so is one whose stat is not set. */
export function installStarter(doc: ProjectDoc, chosen: string[], map: StatMap): InstallResult {
  const have = new Set((doc.skills ?? []).map((s) => s.id))
  const toAdd: Skill[] = []
  const r: InstallResult = { doc, added: [], existing: [], missingStat: [] }
  for (const sk of STARTER_SKILLS.filter((s) => chosen.includes(s.id))) {
    if (have.has(sk.id)) { r.existing.push(sk.name); continue }
    const done = resolveSkill(sk, map)
    if (!done) { r.missingStat.push(sk.name); continue }
    toAdd.push(done); r.added.push(sk.name)
  }
  r.doc = toAdd.length ? withSkills(doc, (list) => [...list, ...toAdd]) : doc
  return r
}
