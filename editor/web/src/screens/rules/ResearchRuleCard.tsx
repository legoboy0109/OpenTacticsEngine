// "Research" group on the Rules screen: rules.research.needsStructure in game.json (docs/ECONOMY.md). Shown when the game
// has technologies or the rule is already set (so a game without research does not get a card about it).
import { researchNeedsStructure, researchRuleSet, setResearchNeedsStructure } from '../../model/optionRules'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function ResearchRuleCard() {
  const doc = useDoc()
  const edit = useEdit()
  if ((doc.research?.length ?? 0) === 0 && !researchRuleSet(doc)) return null
  return (
    <Card title={T.researchGroup} help={T.researchGroupHelp}>
      <div className="stack" data-testid="research-rule">
        <Check label={T.researchNeedsStructure} help={T.researchNeedsStructureHelp} checked={researchNeedsStructure(doc)}
          onChange={(v) => edit((d) => setResearchNeedsStructure(d, v))} />
      </div>
    </Card>
  )
}
