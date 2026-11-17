// The starter set of status conditions (docs/SRPG.md section 9 "The starter set"): the six conditions of editor/templates/strategy-rpg/data/conditions.json,
// which the editor can add to any game that has the Conditions screen switched on. The curse lowers two stats, written as `$atk` and `$def` and filled
// in with the game's own (the same guess the starter skills make); where the game has no such stat the curse is skipped and listed. A unit test compares
// this list with the template's file. Names, tags and numbers are placeholders for the owner to review.
import { withConditions } from './doc'
import { guessStats, type StatMap } from './starterSkills'
import type { Condition } from './srpgTypes'
import type { ProjectDoc } from './types'

export const STARTER_CONDITIONS: Condition[] = [
  { id: 'poison', name: 'Poison', tag: 'Po', turns: 3, description: 'Loses a little HP at the start of each of its turns. Never defeats a unit.',
    blocks: [{ when: 'turnStart', do: [{ hurt: { percent: 10 }, to: 'self' }] }] },
  { id: 'sleep', name: 'Sleep', tag: 'Zz', turns: 3, endsWhenStruck: true, description: 'Cannot act or move. Wakes up when it is struck.',
    blocks: [{ when: 'always', do: [{ cannotAct: true }, { cannotMove: true }] }] },
  { id: 'silence', name: 'Silence', tag: 'Si', turns: 3, description: 'Cannot use staves.',
    blocks: [{ when: 'always', do: [{ forbid: { kinds: ['staff'] } }] }] },
  { id: 'berserk', name: 'Berserk', tag: 'Br', turns: 3, description: 'Out of control: attacks the nearest unit, friend or foe, on its own.',
    blocks: [{ when: 'always', do: [{ berserk: true }] }] },
  { id: 'stun', name: 'Stun', tag: 'St', turns: 2, description: 'Cannot move, but can still act.',
    blocks: [{ when: 'always', do: [{ cannotMove: true }] }] },
  { id: 'curse', name: 'Curse', tag: 'Cu', turns: 3, description: 'Strength and defense are lowered.',
    blocks: [{ when: 'always', do: [{ stats: { $atk: -2, $def: -2 } }] }] },
]

const ROLE = /\$(atk|def)\b/g

/** The condition with the stat ids filled in, or null when a stat it needs is not set. */
function resolve(c: Condition, map: StatMap): Condition | null {
  const text = JSON.stringify(c)
  const roles = [...text.matchAll(ROLE)].map((m) => m[1] as 'atk' | 'def')
  if (roles.some((r) => !map[r])) return null
  return JSON.parse(text.replace(ROLE, (_m, r: 'atk' | 'def') => map[r]!)) as Condition
}

export interface StarterConditionsResult { doc: ProjectDoc; added: string[]; existing: string[]; missingStat: string[] }

/** Adds the starter conditions whose id is not in the game yet, in one edit (one Undo step); the rest are listed as skipped. */
export function installStarterConditions(doc: ProjectDoc): StarterConditionsResult {
  const have = new Set((doc.conditions ?? []).map((c) => c.id))
  const map = guessStats(doc)
  const toAdd: Condition[] = []
  const r: StarterConditionsResult = { doc, added: [], existing: [], missingStat: [] }
  for (const c of STARTER_CONDITIONS) {
    if (have.has(c.id)) { r.existing.push(c.name); continue }
    const done = resolve(c, map)
    if (!done) { r.missingStat.push(c.name); continue }
    toAdd.push(done); r.added.push(c.name)
  }
  r.doc = toAdd.length ? withConditions(doc, (list) => [...list, ...toAdd]) : doc
  return r
}
