// The strategy-RPG cards of an item, in the order they are shown. The form mounts only this component, so each later
// step fills its own file here and never edits the form.
import { ItemSkillsCard } from './ItemSkillsCard'
import { ItemPromotionCard } from './ItemPromotionCard'
import { ItemTeachesCard } from './ItemTeachesCard'
import { StaffExtraCards } from '../staff/StaffCards'

export function ItemSrpgCards({ id }: { id: string }) {
  return (
    <>
      <ItemSkillsCard id={id} />
      <ItemPromotionCard id={id} />
      <ItemTeachesCard id={id} />
      <StaffExtraCards id={id} />
    </>
  )
}
