// The parts of "What it does" besides healing: moving a unit (warp / pull / swap), repairing an item, lighting an area. Every choice the
// loader would refuse is greyed out (never silently dropped), see model/staffEffects.ts.
import { STAFF_RADIUS_MAX, STAFF_RANGE_MAX, reachExample, type ReachMode } from '../../../model/staff'
import {
  STAFF_LIGHT_TURNS_MAX, STAFF_REPAIR_MAX, canDropEffect, lightOf, moveBlock, moveOf, repairBlock, repairOf, setLightOn, setLightRadius, setLightTurns, setMoveKind,
  setRepairKind, setRepairOn, setRepairUses, setWarpFormula, setWarpMax, setWarpMin, setWarpMode, unknownEffectCount, type RepairKind,
} from '../../../model/staffEffects'
import { MOVE_KINDS, isLightStaff, type MoveKind } from '../../../model/staffFx'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { Check } from '../../../ui/Toggle'
import { S } from './text'

export function StaffMovePart({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const key = `item:${id}`
  const move = moveOf(item)
  const blocks = MOVE_KINDS.map((k) => moveBlock(item, k))
  const options = [
    { value: 'none', label: S.moveKinds.none, disabled: move.kind !== 'none' && !canDropEffect(item) },
    ...MOVE_KINDS.map((k, n) => ({ value: k, label: S.moveKinds[k], disabled: k !== move.kind && blocks[n] !== '' })),
  ]
  const why = blocks.includes('repair') ? S.moveRepair : blocks.includes('self') ? S.moveSelf : S.moveHelp
  const w = move.warp
  return (
    <>
      <Field label={S.move} help={why}>
        <Select value={move.kind} options={options} onChange={(v) => edit((d) => setMoveKind(d, id, v as 'none' | MoveKind))} />
      </Field>
      {move.kind === 'warp' && (
        <>
          <Field label={S.warpReach} help={S.warpReachHelp}>
            <Select value={w.mode} options={(Object.keys(S.reachModes) as ReachMode[]).map((m) => ({ value: m, label: S.reachModes[m] }))}
              onChange={(v) => edit((d) => setWarpMode(d, id, v as ReachMode))} />
          </Field>
          {w.mode === 'upto' && (
            <Field label={S.warpFarthest}>
              <NumberInput value={w.max} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setWarpMax(d, id, v), `${key}:warp`) }} />
            </Field>
          )}
          {w.mode === 'between' && (
            <>
              <Field label={S.warpNearest}>
                <NumberInput value={w.min} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setWarpMin(d, id, v), `${key}:warpMin`) }} />
              </Field>
              <Field label={S.warpFarthest}>
                <NumberInput value={w.max} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setWarpMax(d, id, v), `${key}:warp`) }} />
              </Field>
            </>
          )}
          {w.mode === 'formula' && (
            <Field label={S.warpFormula} help={S.reachFormulaHelp(reachExample(doc))}>
              <TextInput mono value={w.text} data-learn="staff-warp-formula" onChange={(v) => edit((d) => setWarpFormula(d, id, v), `${key}:warpFormula`)} />
            </Field>
          )}
        </>
      )}
    </>
  )
}

export function StaffRepairPart({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const repair = repairOf(item)
  const block = repairBlock(item)
  return (
    <>
      <Check label={S.repairOn} checked={repair.on} disabled={repair.on ? !canDropEffect(item) : block !== ''}
        help={repair.on ? S.repairOnHelp : block === 'move' ? S.repairBlocked : block === 'full' ? S.fourMax : S.repairOnHelp}
        onChange={(on) => edit((d) => setRepairOn(d, id, on))} />
      {repair.on && (
        <>
          <Field label={S.repair}>
            <Select value={repair.kind} options={(Object.keys(S.repairKinds) as RepairKind[]).map((k) => ({ value: k, label: S.repairKinds[k] }))}
              onChange={(v) => edit((d) => setRepairKind(d, id, v as RepairKind))} />
          </Field>
          {repair.kind === 'uses' && (
            <Field label={S.repairUses}>
              <NumberInput value={repair.uses} min={1} max={STAFF_REPAIR_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setRepairUses(d, id, v), `item:${id}:repair`) }} />
            </Field>
          )}
        </>
      )}
    </>
  )
}

export function StaffLightPart({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const key = `item:${id}`
  const light = lightOf(item)
  const unknown = !isLightStaff(item) && unknownEffectCount(item) > 0
  return (
    <>
      <Check label={S.lightOn} checked={light.on} disabled={unknown} help={unknown ? S.lightUnknown : S.lightOnHelp}
        onChange={(on) => edit((d) => setLightOn(d, id, on))} />
      {light.on && (
        <>
          <Field label={S.lightRadius}>
            <NumberInput value={light.radius} min={1} max={STAFF_RADIUS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setLightRadius(d, id, v), `${key}:lightRadius`) }} />
          </Field>
          <Field label={S.lightTurns} help={S.lightTurnsHelp}>
            <NumberInput value={light.turns} min={1} max={STAFF_LIGHT_TURNS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setLightTurns(d, id, v), `${key}:lightTurns`) }} />
          </Field>
        </>
      )}
    </>
  )
}
