// The "Map" tab: name and size.
import { useState } from 'react'
import { renameMap } from '../../../../model/maps'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { TextInput } from '../../../../ui/TextInput'
import { T } from '../text'
import type { MapViewState } from '../viewState'
import { MapMusic } from '../../../sound/MapMusic'
import { FogSettings } from './FogSettings'
import { ResizeDialog } from './ResizeDialog'

export function SettingsPanel({ doc, map, mapId, vs }: { doc: ProjectDoc; map: MapFile; mapId: string; vs: MapViewState }) {
  const edit = useEdit()
  const [resizing, setResizing] = useState(false)
  const ref = doc.game?.maps.find((m) => m.id === mapId)
  const { width, height } = map.grid
  return (
    <div className="stack tight">
      <h3>{T.settingsTitle}</h3>
      <Field label={T.nameLabel} help={T.nameHelp}>
        <TextInput value={ref?.name ?? map.name} onChange={(v) => edit((d) => renameMap(d, mapId, v), `map:${mapId}:name`)} />
      </Field>
      <div className="row space-between">
        <div><div className="label-sm">{T.sizeLabel}</div><strong data-testid="map-size">{T.sizeValue(width, height)}</strong></div>
        <button type="button" className="btn" onClick={() => setResizing(true)}>{T.resizeButton}</button>
      </div>
      <FogSettings doc={doc} map={map} mapId={mapId} vs={vs} />
      <MapMusic doc={doc} map={map} mapId={mapId} />
      {resizing && <ResizeDialog doc={doc} map={map} mapId={mapId} onClose={() => setResizing(false)} />}
    </div>
  )
}
