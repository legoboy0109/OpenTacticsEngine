// A condition picker with its optional "number of turns" box, shared by the staff, item and weapon forms. The turns box is shown only for a condition that
// counts turns (the loader refuses turns on one that does not).
import { countsTurns } from '../../model/staffConditions'
import { useDoc } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { C } from './siteText'

interface Props {
  condition: string
  turns: number | undefined
  onCondition: (id: string) => void
  onTurns: (n: number | undefined) => void
  learn?: string
}

export function ConditionPick({ condition, turns, onCondition, onTurns, learn }: Props) {
  const doc = useDoc()
  const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
  if (condition && !options.some((o) => o.value === condition)) options.push({ value: condition, label: C.notOne(condition) })
  const counts = countsTurns(doc, condition)
  return (
    <>
      <Field label={C.pick}>
        <Select data-learn={learn} value={condition} options={options} onChange={onCondition} />
      </Field>
      {counts && (
        <Field label={C.turns} help={C.turnsHelp}>
          <NumberInput value={turns} min={1} max={99} allowEmpty placeholder={String(doc.conditions?.find((c) => c.id === condition)?.turns ?? '')} onChange={onTurns} />
        </Field>
      )}
      {!counts && doc.conditions?.some((c) => c.id === condition) && <p className="help">{C.turnsNo}</p>}
    </>
  )
}
