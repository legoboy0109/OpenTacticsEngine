// Editor-side checks for character-locked weapons (docs/SRPG.md "Character-locked weapons"), beside the loader's own refusals (which
// reach the Problems list in the engine's words): a name that is no character, and a lock nobody on the list can ever use.
import type { SrpgCheck } from './abilityChecks'
import { classCanUse } from './locks'
import { carriedWeapons } from './itemRules'
import type { ProjectDoc } from './types'

export function lockChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  if (!carriedWeapons(doc)) return out
  const chars = doc.characters ?? []
  for (const w of doc.weapons ?? []) {
    if (!w.wieldedBy?.length) continue
    const ids = [w.id]
    const known = w.wieldedBy.flatMap((id) => chars.find((c) => c.id === id) ?? [])
    for (const id of w.wieldedBy) if (!chars.some((c) => c.id === id)) out.push({ section: 'weapons', ids, text: `${w.name} is locked to “${id}”, who is not a character of this game.` })
    // Known people only: an unknown name is already said above.
    if (known.length > 0 && !known.some((c) => classCanUse(doc.units?.find((u) => u.id === c.type), w)))
      out.push({ section: 'weapons', ids, text: `Nobody on the list can ever wield ${w.name}: ${known.length === 1 ? 'their class cannot' : 'their classes cannot'} use its group${w.group ? ` (“${w.group}”)` : ''}. A lock only narrows; give the class the group first.` })
  }
  return out
}
