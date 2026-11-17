// The "Personal ranks" card of a character (docs/SRPG.md section 7): a rank per weapon group its class can wield. The character starts
// with the higher of this and the class's, so a personal rank only raises. Shown once the game has a ladder (or the character has some).
import { updateCharacter } from '../../../model/characters'
import { hasLadder, setRankIn, wieldsOf } from '../../../model/ranks'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { RankMapTable } from '../../ranks/RankMapTable'
import { R } from '../../ranks/text'

export function CharacterRanksCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c || (!hasLadder(doc) && !c.ranks)) return null
  const groups = wieldsOf(doc, c.type, c.id)
  return (
    <Card title={R.charRanksTitle} help={R.charRanksHelp} id="character-ranks">
      <div className="stack" data-testid="character-ranks">
        {groups.length === 0 && !c.ranks && <p className="help">{R.noGroups}</p>}
        <RankMapTable testId="character-ranks-table" groups={groups} value={c.ranks} noneLabel={R.noneCharRank}
          onSet={(g, v) => edit((d) => updateCharacter(d, id, { ranks: setRankIn(d.characters?.find((x) => x.id === id)?.ranks, g, v) }), `character:${id}:rank:${g}`)} />
      </div>
    </Card>
  )
}
