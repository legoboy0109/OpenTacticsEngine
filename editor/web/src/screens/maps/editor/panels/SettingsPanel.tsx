// The "Map" tab: name and size.
import { useState } from 'react'
import { isCutScene, setCutScene } from '../../../../model/cutScene'
import { renameMap } from '../../../../model/maps'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { useCellWords } from '../../../../ui/cellWords'
import { Field } from '../../../../ui/Field'
import { TextInput } from '../../../../ui/TextInput'
import { Check } from '../../../../ui/Toggle'
import { T } from '../text'
import type { MapViewState } from '../viewState'
import { MapMusic } from '../../../sound/MapMusic'
import { BackdropSettings } from './BackdropSettings'
import { BattleSceneSettings } from './BattleSceneSettings'
import { ExitsSettings } from './ExitsSettings'
import { FogSettings } from './FogSettings'
import { MapApSettings } from './MapApSettings'
import { MapDifficulty } from './MapDifficulty'
import { ResizeDialog } from './ResizeDialog'

export function SettingsPanel({ doc, map, mapId, vs }: { doc: ProjectDoc; map: MapFile; mapId: string; vs: MapViewState }) {
  const edit = useEdit()
  const words = useCellWords()
  const [resizing, setResizing] = useState(false)
  const ref = doc.game?.maps.find((m) => m.id === mapId)
  const { width, height } = map.grid
  return (
    <div className="stack tight">
      <h3>{T.settingsTitle}</h3>
      <Field label={T.nameLabel} help={T.nameHelp}>
        <TextInput value={ref?.name ?? map.name} onChange={(v) => edit((d) => renameMap(d, mapId, v), `map:${mapId}:name`)} />
      </Field>
      <Check label={T.cutSceneLabel} help={T.cutSceneHelp} checked={isCutScene(doc, mapId)} onChange={(on) => edit((d) => setCutScene(d, mapId, on))} />
      <div className="row space-between">
        <div><div className="label-sm">{T.sizeLabel}</div><strong data-testid="map-size">{words(T.sizeValue(width, height))}</strong></div>
        <button type="button" className="btn" onClick={() => setResizing(true)}>{T.resizeButton}</button>
      </div>
      <ExitsSettings doc={doc} map={map} mapId={mapId} />
      <BackdropSettings doc={doc} map={map} mapId={mapId} />
      <FogSettings doc={doc} map={map} mapId={mapId} vs={vs} />
      <BattleSceneSettings doc={doc} map={map} mapId={mapId} />
      <MapMusic doc={doc} map={map} mapId={mapId} />
      <MapApSettings doc={doc} map={map} mapId={mapId} />
      <MapDifficulty doc={doc} map={map} mapId={mapId} />
      {resizing && <ResizeDialog doc={doc} map={map} mapId={mapId} onClose={() => setResizing(false)} />}
    </div>
  )
}
