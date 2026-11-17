// Editor-side checks for character- and class-locked weapons and staves (docs/SRPG.md "Character-locked weapons", section 8), beside the loader's own
// refusals (which reach the Problems list in the engine's words): a name that is no character or class, and a lock nobody on the lists can ever use.
import type { SrpgCheck } from './abilityChecks'
import { classWord } from './characters'
import { allGear } from './gear'
import { carriedWeapons } from './itemRules'
import { classCanUse } from './locks'
import type { ProjectDoc } from './types'

export function lockChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const chars = doc.characters ?? []
  const types = doc.units ?? []
  const word = classWord(doc)
  for (const g of allGear(doc)) {
    // A weapon lock needs carried weapons to mean anything; a staff's does not.
    if (g.owner === 'weapon' && !carriedWeapons(doc)) continue
    const byChar = g.keys.wieldedBy ?? []
    const byType = g.keys.wieldedByTypes ?? []
    if (!byChar.length && !byType.length) continue
    const ids = [g.id]
    const known = byChar.flatMap((id) => chars.find((c) => c.id === id) ?? [])
    const knownTypes = byType.flatMap((id) => types.find((u) => u.id === id) ?? [])
    for (const id of byChar) if (!chars.some((c) => c.id === id)) out.push({ section: g.section, ids, text: `${g.name} is locked to “${id}”, who is not a character of this game.` })
    for (const id of byType) if (!types.some((u) => u.id === id)) out.push({ section: g.section, ids, text: `${g.name} is locked to the ${word} “${id}”, which is not in this game.` })
    // Known people and classes only: an unknown name is already said above.
    const classes = [...known.map((c) => types.find((u) => u.id === c.type)), ...knownTypes]
    if (classes.length > 0 && !classes.some((u) => classCanUse(u, doc, g.owner, g.id))) {
      const who = byType.length === 0 ? (known.length === 1 ? 'their class cannot' : 'their classes cannot') : 'none of those classes can'
      out.push({ section: g.section, ids, text: `Nobody on the ${byType.length && byChar.length ? 'lists' : 'list'} can ever wield ${g.name}: ${who} use its group${g.keys.group ? ` (“${g.keys.group}”)` : ''}. A lock only narrows; give the ${word} the group first.` })
    }
  }
  return out
}
