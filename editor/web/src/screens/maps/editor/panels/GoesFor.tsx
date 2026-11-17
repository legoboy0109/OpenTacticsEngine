// "Goes for" of a computer unit (docs/SRPG.md section 10 "Computer opponent"): what it heads for besides fighting. Shown only for a unit of a computer side.
import { GOES_FOR, setGoesFor } from '../../../../model/mapPlaces'
import { editMap } from '../../../../model/mapCtx'
import type { MapFile, MapUnit } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { FieldGroup } from '../../../../ui/Field'
import { Check } from '../../../../ui/Toggle'
import { Pl } from './placeText'

export function GoesFor({ mapId, map, unit }: { mapId: string; map: MapFile; unit: MapUnit }) {
  const edit = useEdit()
  const side = map.sides?.find((s) => s.id === unit.side)
  if (side?.controller !== 'ai') return null
  const now = unit.goesFor ?? []
  const set = (goals: typeof now) => edit((d) => editMap(d, mapId, (m) => setGoesFor(m, unit, goals)))
  return (
    <FieldGroup label={Pl.goesFor} help={Pl.goesForHelp}>
      <div className="check-list" data-testid="goes-for">
        {GOES_FOR.map((g) => (
          <Check key={g} label={Pl.goesForShort[g]} help={Pl.goesForKinds[g]} checked={now.includes(g)} onChange={(v) => set(v ? [...now, g] : now.filter((x) => x !== g))} />
        ))}
      </div>
    </FieldGroup>
  )
}
