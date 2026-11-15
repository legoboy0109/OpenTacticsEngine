// A list of conditions ("Only if", and the conditions of an If command): each one is a row with its kind and fields
// always showing; add, reorder and delete like every list.
import { listAdd, listMove, listPatch, listRemove, listReplace } from '../../../../model/storyEdit'
import { newCondition } from '../../../../model/storyDefaults'
import { COMPARE_OPS, CONDITION_KINDS, conditionKind, type CompareOp, type Condition, type ConditionKind } from '../../../../model/storyTypes'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { FilterPicker, ItemPick, PickSelect, SwitchPick, VariablePick } from './pickers'
import { RowButtons } from '../../../../ui/RowButtons'
import { useStory } from './StoryContext'
import { S } from './text'

interface Props {
  conditions: Condition[]
  onChange: (next: Condition[], key?: string) => void
  /** Makes the add box's name unique when several lists show at once. */
  name: string
}

const OPS = COMPARE_OPS.map((o) => ({ value: o, label: S.ops[o] }))

export function ConditionList({ conditions, onChange, name }: Props) {
  const story = useStory()
  const patch = (i: number, p: Record<string, unknown>, key?: string) => onChange(listPatch(conditions, i, p), key && `${key}:${i}`)
  return (
    <div className="story-list conditions" data-testid={`conditions-${name}`}>
      {conditions.length === 0 && <p className="help">{S.noConditions}</p>}
      {conditions.map((c, i) => {
        const kind = conditionKind(c)
        return (
          <div className="story-row condition" role="group" aria-label={`${name}: ${S.conditionN(i + 1)}`} key={i}>
            <div className="story-row-head">
              <Field label={S.conditionKind}>
                <Select value={kind ?? 'switch'} options={CONDITION_KINDS.map((k) => ({ value: k, label: S.conditionKinds[k] }))}
                  onChange={(k) => onChange(listReplace(conditions, i, newCondition(story, k as ConditionKind)))} />
              </Field>
              <RowButtons index={i} count={conditions.length} up={`Move ${S.conditionN(i + 1)} up`} down={`Move ${S.conditionN(i + 1)} down`} del={`Delete ${S.conditionN(i + 1)}`}
                onUp={() => onChange(listMove(conditions, i, i - 1))} onDown={() => onChange(listMove(conditions, i, i + 1))} onDelete={() => onChange(listRemove(conditions, i))} />
            </div>
            {kind === 'switch' && (
              <>
                <SwitchPick label={S.conditionSwitch} value={c.switch ?? ''} onChange={(v) => patch(i, { switch: v })} />
                <PickSelect label={S.conditionIs} value={c.is === false ? 'off' : 'on'} options={[{ value: 'on', label: S.on }, { value: 'off', label: S.off }]}
                  onChange={(v) => patch(i, { is: v === 'off' ? false : undefined })} />
              </>
            )}
            {kind === 'variable' && (
              <>
                <VariablePick label={S.conditionVariable} value={c.variable ?? ''} onChange={(v) => patch(i, { variable: v })} />
                <PickSelect label={S.conditionCompare} value={c.op ?? '='} options={OPS} onChange={(v) => patch(i, { op: v as CompareOp })} />
                <Field label={S.conditionValue}><NumberInput value={c.value ?? 0} onChange={(v) => { if (v !== undefined) patch(i, { value: v }, 'value') }} /></Field>
              </>
            )}
            {(kind === 'alive' || kind === 'dead') && (
              <FilterPicker label={S.conditionWho} value={c[kind]} onChange={(f) => patch(i, { [kind]: f ?? {} })} />
            )}
            {kind === 'round' && (
              <>
                <PickSelect label={S.conditionCompare} value={c.round?.op ?? '>='} options={OPS} onChange={(v) => patch(i, { round: { op: v as CompareOp, value: c.round?.value ?? 1 } })} />
                <Field label={S.conditionRound}>
                  <NumberInput value={c.round?.value ?? 1} min={0} onChange={(v) => { if (v !== undefined) patch(i, { round: { op: c.round?.op ?? '>=', value: v } }, 'round') }} />
                </Field>
              </>
            )}
            {kind === 'carries' && (
              <>
                <ItemPick label={S.conditionItem} value={c.carries ?? ''} onChange={(v) => patch(i, { carries: v })} />
                <FilterPicker label={S.conditionCarrier} mode="optional" value={c.unit} onChange={(f) => patch(i, { unit: f })} />
              </>
            )}
          </div>
        )
      })}
      <Select aria-label={`${name}: ${S.addCondition}`} value="" placeholder={S.addCondition} options={CONDITION_KINDS.map((k) => ({ value: k, label: S.conditionKinds[k] }))}
        onChange={(k) => onChange(listAdd(conditions, newCondition(story, k as ConditionKind)))} />
    </div>
  )
}
