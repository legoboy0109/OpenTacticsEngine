// Editor-side checks for character-locked weapons and staves (docs/SRPG.md "Character-locked weapons", section 8), beside the loader's own
// refusals (which reach the Problems list in the engine's words): a name that is no character, and a lock nobody on the list can ever use.
import type { SrpgCheck } from './abilityChecks'
import { allGear } from './gear'
import { carriedWeapons } from './itemRules'
import { classCanUse } from './locks'
import type { ProjectDoc } from './types'

export function lockChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const chars = doc.characters ?? []
  for (const g of allGear(doc)) {
    // A weapon lock needs carried weapons to mean anything; a staff's does not.
    if (!g.keys.wieldedBy?.length || (g.owner === 'weapon' && !carriedWeapons(doc))) continue
    const ids = [g.id]
    const known = g.keys.wieldedBy.flatMap((id) => chars.find((c) => c.id === id) ?? [])
    for (const id of g.keys.wieldedBy) if (!chars.some((c) => c.id === id)) out.push({ section: g.section, ids, text: `${g.name} is locked to “${id}”, who is not a character of this game.` })
    // Known people only: an unknown name is already said above.
    if (known.length > 0 && !known.some((c) => classCanUse(doc.units?.find((u) => u.id === c.type), doc, g.owner, g.id)))
      out.push({ section: g.section, ids, text: `Nobody on the list can ever wield ${g.name}: ${known.length === 1 ? 'their class cannot' : 'their classes cannot'} use its group${g.keys.group ? ` (“${g.keys.group}”)` : ''}. A lock only narrows; give the class the group first.` })
  }
  return out
}
