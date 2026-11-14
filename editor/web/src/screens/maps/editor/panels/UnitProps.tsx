// The selected unit's details: type, side, name, level, leader and its own stats.
import { ctxOfParts, editMap } from '../../../../model/mapCtx'
import { changeUnitType, removeMapUnit, setUnitStat, unitAt, updateMapUnit } from '../../../../model/mapUnits'
import { useState } from 'react'
import { useDoc, useEdit } from '../../../../store/context'
import { Check } from '../../../../ui/Toggle'
import { Field, FieldGroup } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import type { Cell } from '../../../../model/mapGrid'
import { T } from '../text'

interface Props { mapId: string; cell: Cell; say: (m: string | null) => void }

export function UnitProps({ mapId, cell, say }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const map = doc.maps[mapId]
  const unit = map ? unitAt(map, cell.x, cell.y) : undefined
  if (!map || !unit) return <p className="help">{T.unitNone}</p>
  const base = doc.units?.find((u) => u.id === unit.type)
  const stats = doc.game?.stats ?? []
  const key = (f: string) => `unit:${mapId}:${cell.x},${cell.y}:${f}`
  const patch = (p: Parameters<typeof updateMapUnit>[3], k?: string) => edit((d) => editMap(d, mapId, (m) => updateMapUnit(m, cell.x, cell.y, p)), k)
  const typeError = (id: string) => {
    const r = changeUnitType(map, ctxOfParts(doc.terrain ?? [], doc.structures ?? [], doc.units ?? []), cell.x, cell.y, id)
    if (r.error) { say(r.error); return }
    edit((d) => editMap(d, mapId, (m, c) => changeUnitType(m, c, cell.x, cell.y, id).map))
  }
  return (
    <div className="stack tight" data-testid="unit-props">
      <h3>{T.unitTitle} <span className="help inline">{T.unitAt(cell.x, cell.y)}</span></h3>
      <div className="form-grid panel-form">
        <Field label={T.unitTypeLabel}>
          <Select value={unit.type} onChange={typeError} options={(doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))} />
        </Field>
        <Field label={T.sideLabel}>
          <Select value={unit.side} onChange={(v) => patch({ side: v })} options={(map.sides ?? []).map((s) => ({ value: s.id, label: s.name || s.id }))} />
        </Field>
        <Field label={T.unitName} help={T.unitNameHelp}>
          <TextInput value={unit.name ?? ''} onChange={(v) => patch({ name: v }, key('name'))} />
        </Field>
        {doc.game?.rules?.progression?.enabled && (
          <Field label={T.unitLevel} help={T.unitLevelHelp}>
            <NumberInput value={unit.level} min={1} allowEmpty placeholder="1" onChange={(v) => patch({ level: v }, key('level'))} />
          </Field>
        )}
      </div>
      <Check checked={!!unit.leader} label={T.unitLeader} help={T.unitLeaderHelp} onChange={(v) => patch({ leader: v })} />
      <FieldGroup label={T.statsTitle} help={T.statsHelp}>
        <table className="kv">
          <tbody>
            {stats.map((s) => (
              <tr key={s.id}>
                <th scope="row">{s.name}</th>
                <td>
                  <StatCell label={`${s.name} for this unit`} override={unit.stats?.[s.id]} base={base?.stats?.[s.id] ?? 0}
                    onChange={(v) => edit((d) => editMap(d, mapId, (m, c) => setUnitStat(m, c, cell.x, cell.y, s.id, v)), key(`stat:${s.id}`))} />
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </FieldGroup>
      <button type="button" className="btn danger sm" onClick={() => edit((d) => editMap(d, mapId, (m) => removeMapUnit(m, cell.x, cell.y)))}>{T.unitRemove}</button>
    </div>
  )
}

/** A value equal to the unit type's own is not stored; the box keeps showing what was typed so typing "25" over a base of 2 is not interrupted. */
function StatCell({ label, override, base, onChange }: { label: string; override: number | undefined; base: number; onChange: (v: number | undefined) => void }) {
  const [typed, setTyped] = useState<number | undefined>()
  const shown = override ?? (typed === base ? typed : undefined)
  return <NumberInput aria-label={label} value={shown} allowEmpty placeholder={String(base)} onChange={(v) => { setTyped(v); onChange(v) }} />
}
