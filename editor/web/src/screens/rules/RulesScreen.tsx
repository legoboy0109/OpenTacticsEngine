import { attentionList } from '../../model/formulas'
import { Needs } from '../../shell/Blocked'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, PageHeader } from '../../ui/Card'
import { BattleCard } from './BattleCard'
import { FeaturesCard } from './FeaturesCard'
import { ItemsCard } from './ItemsCard'
import { FormulaEditor } from './FormulaEditor'
import { MoveTypesCard, ResourcesCard } from './ListsCards'
import { SavingCard } from './SavingCard'
import { SwitchesCard, VariablesCard } from './StoryListsCards'
import { StatsCard } from './StatsCard'
import { T } from './text'

export function RulesScreen() {
  return (
    <div className="screen">
      <PageHeader title={T.title} help={T.intro} />
      <Needs parts={['game']}><RulesForm /></Needs>
    </div>
  )
}

function RulesForm() {
  const game = useDoc().game!
  const attention = attentionList(game)
  return (
    <>
      {attention.length > 0 && (
        <Banner tone="warn" title={T.attentionTitle}>{T.attentionBody(attention.map((a) => a.label))}</Banner>
      )}
      <StatsCard />
      <ResourcesCard />
      <MoveTypesCard />
      <BattleCard />
      <FeaturesCard />
      <ItemsCard />
      <SavingCard />
      <SwitchesCard />
      <VariablesCard />
      <Card title={T.combat} help={T.combatHelp}>
        <div className="stack formulas">
          <FormulaEditor fkey="damage" />
          <FormulaEditor fkey="hit" />
          <FormulaEditor fkey="crit" />
          <FormulaEditor fkey="strikes" />
        </div>
      </Card>
    </>
  )
}
