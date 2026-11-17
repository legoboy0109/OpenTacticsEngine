// A placed unit's own starting ranks (docs/SRPG.md section 7) in the map's unit panel: the shared control (`StartingRanksField`) stored on the
// map unit. The story "Add a unit" command uses the same control on the command.
import { editMap } from '../../../../model/mapCtx'
import { unitAt, updateMapUnit } from '../../../../model/mapUnits'
import type { Cell } from '../../../../model/mapGrid'
import { useDoc, useEdit } from '../../../../store/context'
import { StartingRanksField } from '../../../ranks/StartingRanksField'

export function UnitRankOverride({ mapId, cell, classId }: { mapId: string; cell: Cell; classId: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.maps[mapId] ? unitAt(doc.maps[mapId]!, cell.x, cell.y) : undefined
  if (!unit) return null
  return (
    <StartingRanksField type={classId} character={unit.character} value={unit.ranks} testId="unit-rank-override"
      onChange={(ranks, k) => edit((d) => editMap(d, mapId, (m) => updateMapUnit(m, cell.x, cell.y, { ranks })), `unit:${mapId}:${cell.x},${cell.y}:${k}`)} />
  )
}
