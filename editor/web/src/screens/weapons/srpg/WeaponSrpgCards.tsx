// The strategy-RPG cards of a weapon, in the order they are shown. The form mounts only this component, so each later
// step fills its own file here and never edits the form.
import { WeaponRankCard } from './WeaponRankCard'
import { WeaponSkillsCard } from './WeaponSkillsCard'

export function WeaponSrpgCards({ id }: { id: string }) {
  return (
    <>
      <WeaponRankCard id={id} />
      <WeaponSkillsCard id={id} />
    </>
  )
}
