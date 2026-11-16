// The strategy-RPG cards of the Rules screen, in the order they are shown. RulesScreen mounts only this component, so
// the later steps fill their own file (E3: CarryingCard, PromotionRulesCard) and never edit RulesScreen.
import { CarryingCard } from './CarryingCard'
import { FeatureSwitchesCard } from './FeatureSwitchesCard'
import { CapsCard } from './CapsCard'
import { PromotionRulesCard } from './PromotionRulesCard'
import { RanksCard } from './RanksCard'

export function SrpgRulesCards() {
  return (
    <>
      <FeatureSwitchesCard />
      <CarryingCard />
      <PromotionRulesCard />
      <RanksCard />
      <CapsCard />
    </>
  )
}
