// A placed structure whose kind has a weapon to use: its own number of uses (docs/SRPG.md "Map weapons"). Shown in the Structures tool's panel.
import { editMap } from '../../../../model/mapCtx'
import { hasWeapon, setPlacedUses } from '../../../../model/mapWeapon'
import type { MapFile, MapStructure, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { W } from '../../../structures/weaponText'

export function MapWeaponProps({ doc, mapId, s }: { doc: ProjectDoc; map: MapFile; mapId: string; s: MapStructure }) {
  const edit = useEdit()
  const def = doc.structures?.find((k) => k.id === s.type)
  if (!def || !hasWeapon(def)) return null
  const kindText = def.uses === undefined ? W.placedKindUnlimited : W.placedKindUses(def.uses)
  return (
    <div className="stack tight" data-testid="map-weapon-props">
      <Field label={W.placedUses} help={W.placedUsesHelp(kindText)}>
        <NumberInput value={s.uses} min={1} max={9999} allowEmpty placeholder={kindText}
          onChange={(v) => edit((d) => editMap(d, mapId, (m) => setPlacedUses(m, { x: s.x, y: s.y }, v)), `weapon-uses:${mapId}:${s.x},${s.y}`)} />
      </Field>
    </div>
  )
}
