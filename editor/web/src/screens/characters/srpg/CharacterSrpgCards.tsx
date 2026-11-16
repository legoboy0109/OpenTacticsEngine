// The strategy-RPG cards of a character, in the order they are shown. The form mounts only this component, so E2 and E3
// each fill their own file here and never edit the form.
import { CharacterSkillsCard } from './CharacterSkillsCard'
import { CharacterAbilitiesCard } from './CharacterAbilitiesCard'

export function CharacterSrpgCards({ id }: { id: string }) {
  return (
    <>
      <CharacterSkillsCard id={id} />
      <CharacterAbilitiesCard id={id} />
    </>
  )
}
