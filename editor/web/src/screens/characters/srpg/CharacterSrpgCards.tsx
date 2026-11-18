// The strategy-RPG cards of a character, in the order they are shown. The form mounts only this component, so E2 and E3
// each fill their own file here and never edit the form.
import { CharacterCapsCard } from './CharacterCapsCard'
import { CharacterLockedWeapons } from './CharacterLockedWeapons'
import { CharacterRanksCard } from './CharacterRanksCard'
import { CharacterWieldsCard } from './CharacterWieldsCard'
import { CharacterSkillsCard } from './CharacterSkillsCard'
import { CharacterAbilitiesCard } from './CharacterAbilitiesCard'
import { CharacterQuotesCard } from './CharacterQuotesCard'
import { CharacterPromotionCard } from './CharacterPromotionCard'

export function CharacterSrpgCards({ id }: { id: string }) {
  return (
    <>
      <CharacterWieldsCard id={id} />
      <CharacterRanksCard id={id} />
      <CharacterCapsCard id={id} />
      <CharacterLockedWeapons id={id} />
      <CharacterSkillsCard id={id} />
      <CharacterAbilitiesCard id={id} />
      <CharacterQuotesCard id={id} />
      <CharacterPromotionCard id={id} />
    </>
  )
}
