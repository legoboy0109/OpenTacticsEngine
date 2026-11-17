// A placed unit's own starting ranks (docs/SRPG.md section 7): replaces the class's (or character's) rank in a group, for this unit only.
// "Its own" removes the entry. The one control behind both the map's unit panel and the story "Add a unit" command: they only differ in
// where the value is stored (`onChange` gets the whole new map, undefined when empty). Shown only in a game that has a ladder (or when
// the unit already has entries, so they can be cleared) and when the class can wield some group.
import { hasLadder, setRankIn, wieldsOf } from '../../model/ranks'
import type { RankMap, RankValue } from '../../model/srpgTypes'
import { useDoc } from '../../store/context'
import { FieldGroup } from '../../ui/Field'
import { RankMapTable } from './RankMapTable'
import { R } from './text'

interface Props {
  /** The unit's class (unit type id); for a character on the map, the character's own groups count too. */
  type: string
  character?: string
  value: RankMap | undefined
  onChange: (next: RankMap | undefined, key: string) => void
  testId: string
}

export function StartingRanksField({ type, character, value, onChange, testId }: Props) {
  const doc = useDoc()
  if (!hasLadder(doc) && !value) return null
  const groups = wieldsOf(doc, type, character)
  if (groups.length === 0 && !value) return null
  return (
    <FieldGroup label={R.mapRanksTitle} help={R.mapRanksHelp}>
      <RankMapTable testId={testId} groups={groups} value={value} noneLabel={R.mapOwnRank}
        onSet={(g, v: RankValue | undefined) => onChange(setRankIn(value, g, v), `rank:${g}`)} />
    </FieldGroup>
  )
}
