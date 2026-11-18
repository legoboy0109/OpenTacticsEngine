// The Structures tool's panel: which structure and owner to place, and the details of the selected one.
import { fitPlaceKeys } from '../../../../model/mapPlaces'
import { ctxOf, editMap } from '../../../../model/mapCtx'
import type { Cell } from '../../../../model/mapGrid'
import { changeStructureKind, removeStructureAt, setStructureOwner, structureAt } from '../../../../model/mapStructures'
import { sideOf } from '../../../../model/mapSides'
import { gridOfGame, isHex } from '../../../../model/gridSpec'
import { ownershipMatters, ringsOf, sizeOf } from '../../../../model/structures'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { Select } from '../../../../ui/Select'
import type { ToolState } from '../tools'
import { T } from '../text'
import { MapWeaponProps } from './MapWeaponProps'
import { PlaceProps } from './PlaceProps'
import { SidePicker, StructurePicker } from './pickers'

interface Props {
  doc: ProjectDoc
  map: MapFile
  mapId: string
  state: ToolState
  /** The selected square (null when none). */
  cell: Cell | null
  onChange: (patch: Partial<ToolState>) => void
  say: (m: string | null) => void
}

export function StructureTool({ doc, map, mapId, state, cell, onChange, say }: Props) {
  const kinds = doc.structures ?? []
  const sides = map.sides ?? []
  const hex = isHex(gridOfGame(doc.game))
  return (
    <div className="stack tight">
      <h3>{T.structuresTitle}</h3>
      <p className="help">{hex ? T.structuresHelpHex : T.structuresHelp}</p>
      <SidePicker sides={sides} value={state.structureSide} onChange={(id) => onChange({ structureSide: id })} neutral={T.neutral} label={T.ownerLabel} learn="owner-picker" />
      {kinds.length === 0 && <p className="help">{T.noStructureKinds}</p>}
      <StructurePicker kinds={kinds} value={state.structureType} side={sideOf(map, state.structureSide)} onChange={(id) => onChange({ structureType: id })} />
      <hr className="rule" />
      {cell ? <StructureProps doc={doc} map={map} mapId={mapId} cell={cell} say={say} /> : <p className="help">{T.structureNone}</p>}
    </div>
  )
}

/** The selected structure (the one covering `cell`): kind, owner, position, Remove. */
export function StructureProps({ doc, map, mapId, cell, say }: { doc: ProjectDoc; map: MapFile; mapId: string; cell: Cell; say: (m: string | null) => void }) {
  const edit = useEdit()
  const ctx = ctxOf(doc)
  const s = ctx ? structureAt(map, ctx, cell.x, cell.y) : undefined
  if (!ctx || !s) return <p className="help">{T.structureNone}</p>
  const def = ctx.structureTypes.find((d) => d.id === s.type)
  const { w, h } = sizeOf(def)
  const hex = isHex(ctx.grid)
  const at = { x: s.x, y: s.y }
  const changeKind = (type: string) => {
    const r = changeStructureKind(map, ctx, at, type)
    if (r.error) { say(r.error); return }
    edit((d) => editMap(d, mapId, (m, c) => fitPlaceKeys(d, changeStructureKind(m, c, at, type).map, at)))
  }
  return (
    <div className="stack tight" data-testid="structure-props">
      <h3>{T.structureTitle} <span className="help inline">{hex ? T.structureFromHex(s.x, s.y) : T.structureFrom(s.x, s.y)}</span></h3>
      <p className="help">{hex ? T.footprintHex(ringsOf(def)) : T.footprint(w, h)}</p>
      <div className="form-grid panel-form">
        <Field label={T.structureKind}>
          <Select value={s.type} onChange={changeKind} options={ctx.structureTypes.map((k) => ({ value: k.id, label: k.name }))} />
        </Field>
        <Field label={T.structureOwner}>
          <Select value={s.side ?? ''} onChange={(v) => edit((d) => editMap(d, mapId, (m) => setStructureOwner(m, at, v || null)))}
            options={[{ value: '', label: T.neutral }, ...(map.sides ?? []).map((x) => ({ value: x.id, label: x.name || x.id }))]} />
        </Field>
      </div>
      <PlaceProps doc={doc} map={map} mapId={mapId} s={s} />
      <MapWeaponProps doc={doc} map={map} mapId={mapId} s={s} />
      {s.side && !ownershipMatters(def) && <p className="help">{T.structureOwnerNoEffect(def?.name ?? 'This structure')}</p>}
      <button type="button" className="btn danger sm" onClick={() => edit((d) => editMap(d, mapId, (m, c) => removeStructureAt(m, c, s.x, s.y)))}>{T.structureRemove}</button>
    </div>
  )
}
