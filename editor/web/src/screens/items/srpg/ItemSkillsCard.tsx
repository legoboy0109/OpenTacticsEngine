// The Skills picker of an accessory (E2 of docs/SRPG.md): skills a unit has while it carries the item. Only accessories can
// carry skills (the loader refuses the key on other kinds), so the card shows for accessories only.
import { updateItem } from '../../../model/items'
import { useDoc, useEdit } from '../../../store/context'
import { SkillsCard } from '../../skills/SkillsCard'
import { T } from '../../skills/text'

export function ItemSkillsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((i) => i.id === id)
  if (!item || item.kind !== 'accessory' || (doc.skills === null && !item.skills?.length)) return null
  return <SkillsCard testId="item-skills" help={T.cardHelpItem} skills={item.skills} onSkills={(skills) => edit((d) => updateItem(d, id, { skills }))} />
}
