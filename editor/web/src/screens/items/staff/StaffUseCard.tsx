// The "How it is used" card of a staff (docs/SRPG.md section 8): its group, who it works on, how far it reaches and what it affects.
// The edits are model/staff.ts; this file only lays them out. One column everywhere, so it fits 360 px.
import { useId } from 'react'
import { groupsOf } from '../../../model/ranks'
import { STAFF_RADIUS_MAX, STAFF_RANGE_MAX, areaModeOf, needsSingleArea, radiusOf, reachExample, reachOf, setAreaMode, setRadius, setReachFormula, setReachMax, setReachMin, setReachMode, setStaffGroup, setStaffTarget, type AreaMode, type ReachMode } from '../../../model/staff'
import { hasFx, isLightStaff } from '../../../model/staffFx'
import type { ItemTarget } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { S } from './text'

const TARGETS: ItemTarget[] = ['ally', 'self', 'team', 'any']

export function StaffUseCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const listId = useId()
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'staff') return null
  const key = `item:${id}`
  const target: ItemTarget = item.target ?? 'ally'
  const self = target === 'self'
  const reach = reachOf(item)
  const area = areaModeOf(item)
  const light = isLightStaff(item)
  const noSelf = hasFx(item, ['pull', 'swap'])
  const singleOnly = needsSingleArea(item)
  // "An enemy" is offered only to a staff that does not heal (healing refuses it); a file that has it keeps it shown, so the select never lies.
  // "Only the user" is not offered to a pull or swap (they need another unit).
  const targetOptions = (target === 'foe' || !hasFx(item, ['heal']) ? [...TARGETS, 'foe' as const] : TARGETS)
    .map((t) => ({ value: t, label: S.targets[t], disabled: t === 'self' && noSelf && target !== 'self' }))
  const groups = groupsOf(doc)

  return (
    <Card title={S.useTitle} help={S.useHelp} id="staff-use">
      <div className="stack" data-testid="staff-use">
        <Field label={S.group} help={S.groupHelp}>
          <TextInput value={item.group ?? ''} placeholder={S.groupPlaceholder} list={listId} data-learn="staff-group"
            onChange={(v) => edit((d) => setStaffGroup(d, id, v), `${key}:group`)} />
          <datalist id={listId}>{groups.map((g) => <option key={g} value={g} />)}</datalist>
        </Field>

        {!light && (
          <Field label={S.target} help={noSelf ? S.targetNoSelf : S.targetHelp}>
            <Select value={target} options={targetOptions} onChange={(v) => edit((d) => setStaffTarget(d, id, v as ItemTarget))} />
          </Field>
        )}

        {self && !light ? <p className="help" data-testid="staff-self-note">{S.reachSelf}</p> : (
          <>
            <Field label={S.reach} help={S.reachHelp}>
              <Select value={reach.mode} options={(Object.keys(S.reachModes) as ReachMode[]).map((m) => ({ value: m, label: S.reachModes[m] }))}
                onChange={(v) => edit((d) => setReachMode(d, id, v as ReachMode))} />
            </Field>
            {reach.mode === 'upto' && (
              <Field label={S.reachSquares}>
                <NumberInput value={reach.max} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setReachMax(d, id, v), `${key}:reach`) }} />
              </Field>
            )}
            {reach.mode === 'between' && (
              <>
                <Field label={S.reachFrom}>
                  <NumberInput value={reach.min} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setReachMin(d, id, v), `${key}:reachMin`) }} />
                </Field>
                <Field label={S.reachTo}>
                  <NumberInput value={reach.max} min={1} max={STAFF_RANGE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setReachMax(d, id, v), `${key}:reach`) }} />
                </Field>
              </>
            )}
            {reach.mode === 'formula' && (
              <Field label={S.reachFormula} help={S.reachFormulaHelp(reachExample(doc))}>
                <TextInput mono value={reach.text} data-learn="staff-reach-formula" onChange={(v) => edit((d) => setReachFormula(d, id, v), `${key}:reachFormula`)} />
              </Field>
            )}

            {light ? <p className="help" data-testid="staff-light-note">{S.lightUse}</p> : (
              <Field label={S.area} help={singleOnly ? S.areaSingleOnly : S.areaHelp}>
                <Select value={area} options={(Object.keys(S.areaModes) as AreaMode[]).map((m) => ({ value: m, label: S.areaModes[m], disabled: singleOnly && m !== 'single' }))}
                  onChange={(v) => edit((d) => setAreaMode(d, id, v as AreaMode))} />
              </Field>
            )}
            {!light && area === 'radius' && (
              <Field label={S.areaRadius}>
                <NumberInput value={radiusOf(item)} min={1} max={STAFF_RADIUS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setRadius(d, id, v), `${key}:radius`) }} />
              </Field>
            )}
          </>
        )}
      </div>
    </Card>
  )
}
