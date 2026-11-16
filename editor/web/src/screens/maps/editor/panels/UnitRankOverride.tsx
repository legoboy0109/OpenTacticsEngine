// A placed unit's own starting ranks (docs/SRPG.md section 7): replaces the class's (or character's) rank in a group, for this unit only.
// "Its own" removes the entry. Shown only in a game that has a ladder (or when the unit already has entries, so they can be cleared).
import { editMap } from '../../../../model/mapCtx'
import { unitAt, updateMapUnit } from '../../../../model/mapUnits'
import { hasLadder, setRankIn } from '../../../../model/ranks'
import type { Cell } from '../../../../model/mapGrid'
import { useDoc, useEdit } from '../../../../store/context'
import { FieldGroup } from '../../../../ui/Field'
import { RankMapTable } from '../../../ranks/RankMapTable'
import { R } from '../../../ranks/text'

export function UnitRankOverride({ mapId, cell, classId }: { mapId: string; cell: Cell; classId: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.maps[mapId] ? unitAt(doc.maps[mapId]!, cell.x, cell.y) : undefined
  if (!unit || (!hasLadder(doc) && !unit.ranks)) return null
  const groups = doc.units?.find((u) => u.id === classId)?.wields ?? []
  if (groups.length === 0 && !unit.ranks) return null
  return (
    <FieldGroup label={R.mapRanksTitle} help={R.mapRanksHelp}>
      <RankMapTable testId="unit-rank-override" groups={groups} value={unit.ranks} noneLabel={R.mapOwnRank}
        onSet={(g, v) => edit((d) => editMap(d, mapId, (m) => updateMapUnit(m, cell.x, cell.y, { ranks: setRankIn(unitAt(m, cell.x, cell.y)?.ranks, g, v) })), `unit:${mapId}:${cell.x},${cell.y}:rank:${g}`)} />
    </FieldGroup>
  )
}
