// The map settings' "AP for a won battle (this map)" box: replaces the game's number for battles here; empty = the game's.
import { abilityPointsOf, apName } from '../../../../model/abilityPoints'
import { mapApPerBattle, setMapApPerBattle } from '../../../../model/mapAbilityPoints'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { AP } from './mapApText'

export function MapApSettings({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const own = mapApPerBattle(map)
  // Shown only where it can mean something: the game switched ability points on, or this map already carries the key.
  if (!abilityPointsOf(doc) && map.abilityPoints === undefined) return null
  const game = abilityPointsOf(doc)?.perBattle
  return (
    <Field label={AP.label(apName(doc))} help={AP.help(game)}>
      <NumberInput value={own} min={0} max={9999} allowEmpty placeholder={game === undefined ? '' : String(game)}
        onChange={(v) => edit((d) => setMapApPerBattle(d, mapId, v), `map:${mapId}:apPerBattle`)} />
    </Field>
  )
}
