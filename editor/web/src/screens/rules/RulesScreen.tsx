import { attentionList } from '../../model/formulas'
import { Needs } from '../../shell/Blocked'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, PageHeader } from '../../ui/Card'
import { CampaignChecks } from '../campaign/CampaignChecks'
import { BattleCard } from './BattleCard'
import { FeaturesCard } from './FeaturesCard'
import { ItemsCard } from './ItemsCard'
import { FormulaEditor } from './FormulaEditor'
import { MoveTypesCard, ResourcesCard } from './ListsCards'
import { BattleViewCard } from './BattleViewCard'
import { DifficultyCard } from './DifficultyCard'
import { SavingCard } from './SavingCard'
import { FogRuleCard } from './FogRuleCard'
import { PlacesRuleCard } from './PlacesRuleCard'
import { ResearchRuleCard } from './ResearchRuleCard'
import { SupplyCard } from './SupplyCard'
import { SrpgRulesCards } from './srpg/SrpgRulesCards'
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
      {/* Switches, variables and resources the campaign's scenes use (the rest of its checks are on the Campaign screen). */}
      <CampaignChecks topics={['switch', 'variable', 'resource']} />
      <StatsCard />
      <ResourcesCard />
      <MoveTypesCard />
      <BattleCard />
      <BattleViewCard />
      <FeaturesCard />
      <SrpgRulesCards />
      <SupplyCard />
      <ResearchRuleCard />
      <FogRuleCard />
      <ItemsCard />
      <PlacesRuleCard />
      <SavingCard />
      <DifficultyCard />
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
