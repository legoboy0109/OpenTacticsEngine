// rules.research and rules.fog in game.json (docs/ECONOMY.md "Research", "Fog of war"): research needs a structure
// (default off) and what an ambush does to the unit (default "endsTurn"). Defaults are never written, and a block with
// nothing left in it is removed, so a game that does not touch these keeps a game.json without them. Unknown keys inside
// a block are kept.
import { withGame } from './doc'
import type { AmbushRule, ProjectDoc } from './types'

export const researchNeedsStructure = (doc: ProjectDoc): boolean => doc.game?.rules?.research?.needsStructure === true
/** The rule block is present at all (even set to false): the Research group stays visible for a game without technologies. */
export const researchRuleSet = (doc: ProjectDoc): boolean => doc.game?.rules?.research !== undefined

export function setResearchNeedsStructure(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withGame(doc, (g) => {
    const block = { ...(g.rules?.research ?? {}) }
    if (on) block.needsStructure = true
    else delete block.needsStructure
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(block).length) rules.research = block
    else delete rules.research
    return { ...g, rules }
  })
}

export const ambushRule = (doc: ProjectDoc): AmbushRule => (doc.game?.rules?.fog?.ambush === 'canAct' ? 'canAct' : 'endsTurn')

export function setAmbushRule(doc: ProjectDoc, rule: AmbushRule): ProjectDoc {
  return withGame(doc, (g) => {
    const block = { ...(g.rules?.fog ?? {}) }
    if (rule === 'canAct') block.ambush = 'canAct'
    else delete block.ambush
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(block).length) rules.fog = block
    else delete rules.fog
    return { ...g, rules }
  })
}
