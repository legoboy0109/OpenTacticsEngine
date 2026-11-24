// How much HP a revive gives back: a number, everything, a share of the maximum or (when allowed) a formula. One editor shared by the
// ability, the usable item, the staff and the condition; the caller says where the value is stored.
import { REVIVE_MAX, REVIVE_PERCENT_MAX, reviveKindOf, reviveNumber, reviveStart, reviveWithNumber, type ReviveKind, type ReviveValue } from '../../model/downed'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { D } from './text'

interface Props {
  value: unknown
  /** An ability and a staff may use a formula; a usable item and a condition may not. */
  formula: boolean
  /** `joinKey` lets quick edits (typing) join into one undo step. */
  onChange: (v: ReviveValue, joinKey?: string) => void
  testId?: string
}

export function ReviveAmount({ value, formula, onChange, testId = 'revive-amount' }: Props) {
  const kind = reviveKindOf(value)
  const kinds = (Object.keys(D.kinds) as ReviveKind[]).filter((k) => formula || k !== 'formula')
  return (
    <div className="stack tight" data-testid={testId}>
      <Field label={D.how}>
        <Select value={kind} options={kinds.map((k) => ({ value: k, label: D.kinds[k] }))} onChange={(v) => onChange(reviveStart(v as ReviveKind))} />
      </Field>
      {kind === 'amount' && (
        <Field label={D.amount} help={D.amountHelp}>
          <NumberInput value={reviveNumber(value)} min={1} max={REVIVE_MAX} onChange={(v) => { if (v !== undefined) onChange(reviveWithNumber('amount', v), 'amount') }} />
        </Field>
      )}
      {kind === 'percent' && (
        <Field label={D.percent} help={D.percentHelp}>
          <NumberInput value={reviveNumber(value)} min={1} max={REVIVE_PERCENT_MAX} suffix="%" onChange={(v) => { if (v !== undefined) onChange(reviveWithNumber('percent', v), 'percent') }} />
        </Field>
      )}
      {kind === 'formula' && (
        <Field label={D.formula} help={D.formulaHelp}>
          <TextInput mono value={String(value)} onChange={(v) => onChange(v, 'formula')} />
        </Field>
      )}
    </div>
  )
}
