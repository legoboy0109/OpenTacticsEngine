// The "Tool" tab: options of the active tool, and details of what is selected.
import { type Cell } from '../../../../model/mapGrid'
import { sideOf } from '../../../../model/mapSides'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useCellWords } from '../../../../ui/cellWords'
import { Toggle } from '../../../../ui/Toggle'
import type { Tool, ToolState } from '../tools'
import { T } from '../text'
import { Inspect } from './Inspect'
import { SidePicker, TerrainPicker } from './pickers'
import { UnitPicker } from './UnitPicker'
import { HeightsTool } from './HeightsTool'
import { SlotsTool } from './SlotsTool'
import { StructureTool } from './StructureTool'
import { UnitProps } from './UnitProps'

interface Props {
  doc: ProjectDoc
  map: MapFile
  mapId: string
  tool: Tool
  state: ToolState
  /** The selected square (null when none). */
  selected: string | null
  onChange: (patch: Partial<ToolState>) => void
  say: (m: string | null) => void
}

const parse = (k: string | null): Cell | null => { if (!k) return null; const [x, y] = k.split(',').map(Number); return { x, y } }

export function ToolPanel({ doc, map, mapId, tool, state, selected, onChange, say }: Props) {
  const sides = map.sides ?? []
  const words = useCellWords()
  const cell = parse(selected)
  switch (tool) {
    case 'terrain':
      return (
        <div className="stack tight">
          <h3>{T.terrainTitle}</h3>
          <p className="help">{T.terrainHelp}</p>
          <Toggle checked={state.fill} onChange={(v) => onChange({ fill: v })} label={`${T.fill} (F)`} help={T.fillHelp} />
          <TerrainPicker terrain={doc.terrain ?? []} value={state.terrainId} onChange={(id) => onChange({ terrainId: id })} />
          {(doc.terrain?.length ?? 0) === 1 && <p className="help">{T.oneTerrain}</p>}
        </div>
      )
    case 'units':
      return (
        <div className="stack tight">
          <h3>{T.unitsTitle}</h3>
          <p className="help">{words(doc.characters === null ? T.unitsHelp : T.unitsHelpCharacters)}</p>
          <SidePicker sides={sides} value={state.unitSide} onChange={(id) => onChange({ unitSide: id })} />
          {(doc.units?.length ?? 0) === 0 && <p className="help">{T.noUnitsTypes}</p>}
          <UnitPicker doc={doc} map={map} state={state} color={sideOf(map, state.unitSide)?.color} onChange={onChange} />
          <hr className="rule" />
          {cell ? <UnitProps mapId={mapId} cell={cell} say={say} /> : <p className="help">{T.unitNone}</p>}
        </div>
      )
    case 'structures':
      return <StructureTool doc={doc} map={map} mapId={mapId} state={state} cell={cell} onChange={onChange} say={say} />
    case 'heights':
      return <HeightsTool state={state} onChange={onChange} />
    case 'slots':
      return <SlotsTool doc={doc} map={map} mapId={mapId} />
    case 'select':
      return <Inspect doc={doc} map={map} mapId={mapId} cell={cell} say={say} />
  }
}
