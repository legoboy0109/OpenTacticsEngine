// The strategy-RPG cards of a unit type, in the order they are shown. The form mounts only this component, so each later
// step fills its own file here and never edits the form.
import { UnitCapsCard } from './UnitCapsCard'
import { UnitRanksCard } from './UnitRanksCard'
import { UnitSkillsCard } from './UnitSkillsCard'
import { UnitAbilitiesCard } from './UnitAbilitiesCard'
import { UnitPromotionCard } from './UnitPromotionCard'
import { UnitPairBonusCard } from './UnitPairBonusCard'

export function UnitSrpgCards({ id }: { id: string }) {
  return (
    <>
      <UnitRanksCard id={id} />
      <UnitCapsCard id={id} />
      <UnitSkillsCard id={id} />
      <UnitAbilitiesCard id={id} />
      <UnitPromotionCard id={id} />
      <UnitPairBonusCard id={id} />
    </>
  )
}
