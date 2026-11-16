// The "Starting ranks" card of a unit type (docs/SRPG.md section 7): one rank per weapon group the class can wield. A choice of "None"
// removes the entry, so the unit starts at the lowest rank. Shown once the game has a ladder (or the class already has entries).
import { hasLadder, setRankIn } from '../../../model/ranks'
import { updateUnit } from '../../../model/units'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { RankMapTable } from '../../ranks/RankMapTable'
import { R } from '../../ranks/text'

export function UnitRanksCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit || (!hasLadder(doc) && !unit.ranks)) return null
  const groups = unit.wields ?? []
  return (
    <Card title={R.unitRanksTitle} help={R.unitRanksHelp} id="unit-ranks">
      <div className="stack" data-testid="unit-ranks">
        {groups.length === 0 && !unit.ranks && <p className="help">{R.noGroups}</p>}
        <RankMapTable testId="unit-ranks-table" groups={groups} value={unit.ranks} noneLabel={R.noneRank}
          onSet={(g, v) => edit((d) => updateUnit(d, id, { ranks: setRankIn(d.units?.find((u) => u.id === id)?.ranks, g, v) }), `unit:${id}:rank:${g}`)} />
      </div>
    </Card>
  )
}
