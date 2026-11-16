// The Skills card of a unit type (E2 of docs/SRPG.md): skills it always has and "learns at a level" rows. Edits `skills` and
// `learns` of the unit type. Shown once the game has skills (or the unit type already names some, so they can be removed).
import { updateUnit } from '../../../model/units'
import { useDoc, useEdit } from '../../../store/context'
import { SkillsCard } from '../../skills/SkillsCard'
import { T } from '../../skills/text'

export function UnitSkillsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit || (doc.skills === null && !unit.skills?.length && !unit.learns?.length)) return null
  return (
    <SkillsCard testId="unit-skills" help={T.cardHelpUnit} skills={unit.skills} learns={unit.learns}
      onSkills={(skills, key) => edit((d) => updateUnit(d, id, { skills }), key && `unit:${id}:${key}`)}
      onLearns={(learns, key) => edit((d) => updateUnit(d, id, { learns }), key && `unit:${id}:${key}`)} />
  )
}
