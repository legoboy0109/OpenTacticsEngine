// The map settings' fog of war part: the checkbox (with the reason when it cannot be switched on) and, while it is on,
// the "Show what a side sees" aid that dims on the canvas what a side's team would not see at the start.
import { fogBlock, setFog } from '../../../../model/fog'
import { sideChoices } from '../../../../model/fogSight'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { Select } from '../../../../ui/Select'
import { Check } from '../../../../ui/Toggle'
import { T } from '../text'
import { useViewSlice, type MapViewState } from '../viewState'

export function FogSettings({ doc, map, mapId, vs }: { doc: ProjectDoc; map: MapFile; mapId: string; vs: MapViewState }) {
  const edit = useEdit()
  const on = map.fog === true
  const block = fogBlock(doc, mapId)
  const side = useViewSlice(vs, (s) => s.fogSide)
  const sides = sideChoices(map)
  return (
    <div className="stack tight" data-testid="fog-settings">
      {/* Switching off stays possible when the map already has too many sides; only switching on is refused. */}
      <Check label={T.fogLabel} checked={on} disabled={!on && block !== null} onChange={(v) => edit((d) => setFog(d, mapId, v))}
        help={!on && block ? <>{T.fogHelp} <strong>{block}</strong></> : T.fogHelp} />
      {on && sides.length > 0 && (
        <>
          <Check label={T.fogAid} help={T.fogAidHelp} checked={side !== null} onChange={(v) => vs.set({ fogSide: v ? sides[0].value : null })} />
          {side !== null && (
            <Field label={T.fogAidSide}>
              <Select value={sides.some((s) => s.value === side) ? side : ''} options={sides} placeholder={T.fogAidPick} onChange={(v) => vs.set({ fogSide: v })} />
            </Field>
          )}
        </>
      )}
    </div>
  )
}
