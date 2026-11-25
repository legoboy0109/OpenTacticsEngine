// "Behaviour" of a placed computer unit (docs/AI.md). Shown only for a unit of a computer side, like "Goes for".
import { isGuest } from '../../../../model/guests'
import { editMap } from '../../../../model/mapCtx'
import { updateMapUnit } from '../../../../model/mapUnits'
import type { MapFile, MapUnit } from '../../../../model/types'
import { useDoc, useEdit } from '../../../../store/context'
import { FieldGroup } from '../../../../ui/Field'
import { BehaviourEditor } from './BehaviourEditor'
import { B } from './behaviourText'

export function UnitBehaviour({ mapId, map, unit, variant = 0 }: { mapId: string; map: MapFile; unit: MapUnit; variant?: number }) {
  const edit = useEdit()
  const doc = useDoc()
  const side = map.sides?.find((s) => s.id === unit.side)
  if (side?.controller !== 'ai' && !isGuest(unit)) return null  // a guest on a human side is moved by the computer too
  const set = (next: MapUnit['behaviour'], key?: string) =>
    edit((d) => editMap(d, mapId, (m) => updateMapUnit(m, unit.x, unit.y, { behaviour: next }, variant)), key && `unit:${mapId}:${unit.x},${unit.y},${variant}:behaviour:${key}`)
  return (
    <FieldGroup label={B.title} help={B.help}>
      <BehaviourEditor value={unit.behaviour} onChange={set} doc={doc} map={map} id="unit-behaviour" />
    </FieldGroup>
  )
}
