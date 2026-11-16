// The Skills card of a character (E2 of docs/SRPG.md): personal skills and "learns at a level" rows. Edits `skills` and `learns`
// of the character (`updateCharacter` in model/characters.ts). A character always exists in a game that has the feature.
import { updateCharacter } from '../../../model/characters'
import { useDoc, useEdit } from '../../../store/context'
import { SkillsCard } from '../../skills/SkillsCard'
import { T } from '../../skills/text'

export function CharacterSkillsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c || (doc.skills === null && !c.skills?.length && !c.learns?.length)) return null
  return (
    <SkillsCard testId="character-skills" help={T.cardHelpCharacter} skills={c.skills} learns={c.learns}
      onSkills={(skills, key) => edit((d) => updateCharacter(d, id, { skills }), key && `character:${id}:${key}`)}
      onLearns={(learns, key) => edit((d) => updateCharacter(d, id, { learns }), key && `character:${id}:${key}`)} />
  )
}
