// Which conditions a cure takes off: a whole group (all, harmful, helpful) or a list. Used by the cure effect and by a heal that also cures.
import { setCure } from '../../model/abilities'
import { useDoc } from '../../store/context'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { T } from './text'
import type { FormProps } from './AbilityEffectFields'

export function AbilityCurePicker({ ability, change }: FormProps) {
  const doc = useDoc()
  const cure = ability.effect?.cure
  const whole = cure === 'all' || cure === 'harmful' || cure === 'helpful' ? cure : null  // a whole group, not a list
  const list = (Array.isArray(cure) ? cure : []) as string[]
  const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
  return (
    <div className="stack tight">
      <Field label={T.cureWhich} help={T.cureWhichHelp}>
        <Select value={whole ?? 'some'} options={[{ value: 'all', label: T.cureAll }, { value: 'harmful', label: T.cureHarmful },
          { value: 'helpful', label: T.cureHelpful }, { value: 'some', label: T.curePick }]}
          onChange={(m) => change((a) => setCure(a, m === 'some' ? (options[0] ? [options[0].value] : 'all') : (m as 'all' | 'harmful' | 'helpful')))} />
      </Field>
      {!whole && (options.length === 0 && list.length === 0
        ? <p className="help">{T.cureNone}</p>
        : <MultiSelect label={T.curePick} options={options} value={list} onChange={(v) => change((a) => setCure(a, v))} />)}
    </div>
  )
}
