// "Goes for" of a computer unit (docs/SRPG.md section 10 "Computer opponent"): what it heads for besides fighting. Shown only for a unit of a
// computer side. One control for the map's unit panel and the story "Add a unit" command: `onChange` gets the new list (empty = remove the key).
import { GOES_FOR } from '../../../../model/mapPlaces'
import type { MapFile, MapUnit } from '../../../../model/types'
import { FieldGroup } from '../../../../ui/Field'
import { Check } from '../../../../ui/Toggle'
import { Pl } from './placeText'

type Goals = NonNullable<MapUnit['goesFor']>

export function GoesFor({ map, side, value, onChange }: { map: MapFile; side: string; value: MapUnit['goesFor']; onChange: (goals: Goals) => void }) {
  if (map.sides?.find((s) => s.id === side)?.controller !== 'ai') return null
  const now = value ?? []
  return (
    <FieldGroup label={Pl.goesFor} help={Pl.goesForHelp}>
      <div className="check-list" data-testid="goes-for">
        {GOES_FOR.map((g) => (
          <Check key={g} label={Pl.goesForShort[g]} help={Pl.goesForKinds[g]} checked={now.includes(g)}
            onChange={(v) => onChange(GOES_FOR.filter((x) => (x === g ? v : now.includes(x))))} />
        ))}
      </div>
    </FieldGroup>
  )
}
