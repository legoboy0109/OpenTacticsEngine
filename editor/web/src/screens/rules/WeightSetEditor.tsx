// One set of attack weights (docs/AI.md "Attack weights"): its eight weights, its bases and caps, "attack at zero" and the unit type priority table.
// An empty box means the documented default, which is never written.
import { PRIORITY_MAX, PRIORITY_MIN, setAttackAtZero, setPriority, setWeightNumber, WEIGHT_KEYS, type WeightSet } from '../../model/aiRules'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { AI } from './aiRuleText'

export function WeightSetEditor({ name, set }: { name: string; set: WeightSet }) {
  const doc = useDoc()
  const edit = useEdit()
  const row = (kind: 'weight' | 'base') => WEIGHT_KEYS.filter((k) => k.kind === kind).map((k) => (
    <Field key={k.key} label={AI.labels[k.key]} help={AI.helps[k.key]}>
      <NumberInput value={typeof set[k.key] === 'number' ? (set[k.key] as number) : undefined} min={k.min} max={k.max} allowEmpty placeholder={AI.noDefault(k.def)}
        onChange={(v) => edit((d) => setWeightNumber(d, name, k.key, v), `weight:${name}:${k.key}`)} />
    </Field>
  ))
  const priority = set.priority ?? {}
  const free = (doc.units ?? []).filter((u) => !(u.id in priority))
  return (
    <div className="stack tight" data-testid={`weight-set-${name}`}>
      <fieldset className="fieldset"><legend>{AI.weightsGroup}</legend><div className="stack tight">{row('weight')}</div></fieldset>
      <fieldset className="fieldset"><legend>{AI.basesGroup}</legend><div className="stack tight">{row('base')}</div></fieldset>
      <Check label={AI.attackAtZero} help={AI.attackAtZeroHelp} checked={set.attackAtZero === true} onChange={(v) => edit((d) => setAttackAtZero(d, name, v))} />
      <fieldset className="fieldset" data-testid={`weight-priority-${name}`}>
        <legend>{AI.priorityTitle}</legend>
        <p className="help">{AI.priorityHelp}</p>
        {Object.entries(priority).map(([type, v]) => (
          <div className="form-grid" key={type}>
            <Field label={doc.units?.find((u) => u.id === type)?.name ?? type}>
              <NumberInput aria-label={AI.priorityLabel(type)} value={v} min={PRIORITY_MIN} max={PRIORITY_MAX}
                onChange={(n) => n !== undefined && edit((d) => setPriority(d, name, type, n), `priority:${name}:${type}`)} />
            </Field>
            <button type="button" className="btn sm" aria-label={AI.priorityRemove(type)} onClick={() => edit((d) => setPriority(d, name, type, undefined))}>×</button>
          </div>
        ))}
        {free.length > 0 ? (
          <Select aria-label={AI.priorityAdd} value="" placeholder={AI.priorityAdd} options={free.map((u) => ({ value: u.id, label: u.name }))}
            onChange={(t) => t !== '' && edit((d) => setPriority(d, name, t, 0))} />
        ) : <p className="help">{AI.priorityNone}</p>}
      </fieldset>
    </div>
  )
}
