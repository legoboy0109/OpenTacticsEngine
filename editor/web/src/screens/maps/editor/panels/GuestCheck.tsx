// "Guest" tick of a unit (map unit panel and the story "Add a unit" command): a unit on a side a person plays that the computer moves.
// Shown for a unit of a human side (a computer side's units are the computer's already), and for a unit that already carries the key so it can be removed.
import { isGuest } from '../../../../model/guests'
import type { MapFile, MapUnit } from '../../../../model/types'
import { Check } from '../../../../ui/Toggle'
import { S } from '../story/text'

export function GuestCheck({ map, unit, onChange }: { map: MapFile; unit: Pick<MapUnit, 'side' | 'guest'>; onChange: (on: boolean) => void }) {
  const human = map.sides?.find((s) => s.id === unit.side)?.controller !== 'ai'
  if (!human && unit.guest === undefined) return null
  return <Check label={S.unitGuest} help={S.unitGuestHelp} checked={isGuest(unit)} onChange={onChange} />
}
