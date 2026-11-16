// The Skills picker of a weapon (E2 of docs/SRPG.md): skills a unit has while this is the weapon it fights with.
import { updateWeapon } from '../../../model/weapons'
import { useDoc, useEdit } from '../../../store/context'
import { SkillsCard } from '../../skills/SkillsCard'
import { T } from '../../skills/text'

export function WeaponSkillsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w || (doc.skills === null && !w.skills?.length)) return null
  return <SkillsCard testId="weapon-skills" help={T.cardHelpWeapon} skills={w.skills} onSkills={(skills) => edit((d) => updateWeapon(d, id, { skills }))} />
}
