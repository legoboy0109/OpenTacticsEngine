// The "Tool" tab: options of the active tool, and details of what is selected.
import { terrainAt, type Cell } from '../../../../model/mapGrid'
import { ownerAt } from '../../../../model/mapOwners'
import { sideOf } from '../../../../model/mapSides'
import { unitAt } from '../../../../model/mapUnits'
import type { MapFile, ProjectDoc, Terrain } from '../../../../model/types'
import { Toggle } from '../../../../ui/Toggle'
import type { Tool, ToolState } from '../tools'
import { T } from '../text'
import { SidePicker, TerrainPicker, UnitTypePicker } from './pickers'
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
          <p className="help">{T.unitsHelp}</p>
          <SidePicker sides={sides} value={state.unitSide} onChange={(id) => onChange({ unitSide: id })} />
          {(doc.units?.length ?? 0) === 0 && <p className="help">{T.noUnitsTypes}</p>}
          <UnitTypePicker types={doc.units ?? []} value={state.unitType} color={sideOf(map, state.unitSide)?.color} onChange={(id) => onChange({ unitType: id })} />
          <hr className="rule" />
          {cell ? <UnitProps mapId={mapId} cell={cell} say={say} /> : <p className="help">{T.unitNone}</p>}
        </div>
      )
    case 'owners':
      return (
        <div className="stack tight">
          <h3>{T.ownersTitle}</h3>
          <p className="help">{T.ownersHelp}</p>
          <SidePicker sides={sides} value={state.ownerSide} onChange={(id) => onChange({ ownerSide: id })} neutral={T.neutral} label="Owner" />
        </div>
      )
    case 'select':
      return <Inspect doc={doc} map={map} mapId={mapId} cell={cell} say={say} />
  }
}

function Inspect({ doc, map, mapId, cell, say }: { doc: ProjectDoc; map: MapFile; mapId: string; cell: Cell | null; say: (m: string | null) => void }) {
  if (!cell) return <div className="stack tight"><h3>{T.selectTitle}</h3><p className="help">{T.selectHelp}</p><p className="help">{T.selectNone}</p></div>
  const t: Terrain | undefined = doc.terrain?.find((x) => x.id === terrainAt(map, cell.x, cell.y))
  const owner = ownerAt(map, cell.x, cell.y)
  const unit = unitAt(map, cell.x, cell.y)
  const costs = doc.game?.moveTypes.map((m) => ({ name: m.name, cost: t?.moveCost?.[m.id] ?? 1 })) ?? []
  return (
    <div className="stack tight">
      <h3>{T.selectTitle} <span className="help inline">{T.statusSquare(cell.x, cell.y)}</span></h3>
      <dl className="facts compact">
        <div><dt>{T.terrainOf}</dt><dd>{t?.name ?? '?'}</dd></div>
        <div><dt>{T.ownerOf}</dt><dd>{owner ? sideOf(map, owner)?.name ?? owner : T.noOwner}</dd></div>
        <div><dt>{T.unitOf}</dt><dd>{unit ? doc.units?.find((u) => u.id === unit.type)?.name ?? unit.type : T.noUnit}</dd></div>
      </dl>
      {costs.length > 0 && (
        <p className="help">{T.moveCostsTitle}: {costs.map((c) => `${c.name} ${c.cost > 0 ? c.cost : T.cannotEnter}`).join(', ')}</p>
      )}
      {unit && <><hr className="rule" /><UnitProps mapId={mapId} cell={cell} say={say} /></>}
    </div>
  )
}
