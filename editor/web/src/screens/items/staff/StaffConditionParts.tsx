// The two parts of a staff's "What it does" that use status conditions (docs/SRPG.md section 9): "It inflicts a condition" (which one, optional turns) and
// "It cures" (the conditions I pick, or every condition). Area is allowed for both; they go with healing, repairing and a move; a light staff has neither
// (the same greyed-out-with-a-reason rule as the other parts). An effect written in a way the form cannot show is kept (the form says so).
import { useEffect, useState } from 'react'
import { conditionList } from '../../../model/conditions'
import {
  conditionPartBlock, cureOf, dropCure, inflictOf, setCure, setInflictCondition, setInflictOn, setInflictTurns,
} from '../../../model/staffConditions'
import { canDropEffect } from '../../../model/staffEffects'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { MultiSelect } from '../../../ui/MultiSelect'
import { Select } from '../../../ui/Select'
import { Check } from '../../../ui/Toggle'
import { ConditionPick } from '../../conditions/ConditionPick'
import { C } from '../../conditions/siteText'

export function StaffConditionParts({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  // "It cures" with "the conditions I pick" and nothing picked yet: nothing is written (the loader refuses an empty list); the form
  // shows the part open with the check below it, and writes it with the first pick.
  const [picking, setPicking] = useState(false)
  useEffect(() => setPicking(false), [id])
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const key = `item:${id}`
  const inflict = inflictOf(item)
  const stored = cureOf(item)
  const cure = picking ? { on: true, all: false, ids: [] as string[], editable: true } : stored
  const bInflict = conditionPartBlock(doc, item, 'inflict')
  const bCure = conditionPartBlock(doc, item, 'cure')
  const why = (b: string): string | undefined => (b === 'light' ? C.staffLight : b === 'full' ? C.staffFull : b === 'none' ? C.staffNone : undefined)
  const options = conditionList(doc).map((c) => ({ value: c.id, label: c.name }))
  for (const x of cure.ids) if (!options.some((o) => o.value === x)) options.push({ value: x, label: C.notOne(x) })

  return (
    <>
      <Check label={C.staffInflictOn} checked={inflict.on} disabled={inflict.on ? !canDropEffect(item) : bInflict !== ''}
        help={inflict.on ? C.staffInflictHelp : why(bInflict) ?? C.staffInflictHelp} onChange={(on) => edit((d) => setInflictOn(d, id, on))} />
      {inflict.on && !inflict.editable && <p className="help" data-testid="staff-inflict-other">{C.staffInflictOther}</p>}
      {inflict.on && inflict.editable && (
        <div className="indent stack" data-testid="staff-inflict">
          <ConditionPick condition={inflict.condition} turns={inflict.turns} learn="staff-inflict-condition"
            onCondition={(c) => edit((d) => setInflictCondition(d, id, c))} onTurns={(n) => edit((d) => setInflictTurns(d, id, n), `${key}:inflictTurns`)} />
        </div>
      )}

      <Check label={C.staffCureOn} checked={cure.on} disabled={cure.on ? !picking && !canDropEffect(item) : bCure !== ''}
        help={cure.on ? C.staffCureHelp : why(bCure) ?? C.staffCureHelp}
        onChange={(on) => { if (on) setPicking(true); else { setPicking(false); edit((d) => dropCure(d, id)) } }} />
      {cure.on && !cure.editable && <p className="help" data-testid="staff-cure-other">{C.staffCureOther}</p>}
      {cure.on && cure.editable && (
        <div className="indent stack" data-testid="staff-cure">
          <Field label={C.staffCureWhich}>
            <Select value={cure.all ? 'all' : cure.kind ?? 'list'} options={[{ value: 'list', label: C.staffCureKinds.list }, { value: 'all', label: C.staffCureKinds.all },
              { value: 'harmful', label: C.staffCureKinds.harmful }, { value: 'helpful', label: C.staffCureKinds.helpful }]}
              onChange={(v) => {
                if (v !== 'list') { setPicking(false); edit((d) => setCure(d, id, v as 'all' | 'harmful' | 'helpful')) } else if (stored.all || stored.kind || !stored.on) setPicking(true)
              }} />
          </Field>
          {!cure.all && !cure.kind && <MultiSelect label={C.staffCureList} options={options} value={cure.ids} onChange={(v) => { if (v.length) { setPicking(false); edit((d) => setCure(d, id, v)) } }} />}
          {picking && <p className="help" data-testid="staff-cure-pick">{C.cureNeedsOne}</p>}
        </div>
      )}
    </>
  )
}
