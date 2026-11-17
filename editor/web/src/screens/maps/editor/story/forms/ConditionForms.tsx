// "Put a condition on units" and "Cure units of a condition" (docs/EVENTS.md "Status conditions", map events only). Both name a condition and which units;
// the loader needs the unit filter ({} = every unit), so "anyone" is written as {} and never left out. A unit that is immune resists, as with every other route.
import { countsTurns } from '../../../../../model/staffConditions'
import { asFilter, type Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { ConditionPick, FilterPicker } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function AddConditionForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc } = useStory()
  const id = cmd.addCondition ?? ''
  const counts = countsTurns(doc, id)
  return (
    <div className="stack tight">
      <ConditionPick label={S.addConditionPick} value={id} onChange={(v) => patch({ addCondition: v, ...(countsTurns(doc, v) ? {} : { turns: undefined }) })} />
      <FilterPicker label={S.addConditionWho} value={typeof cmd.to === 'object' ? cmd.to : {}} onChange={(f) => patch({ to: f ?? {} })} />
      {counts && (
        <Field label={S.addConditionTurns} help={S.addConditionTurnsHelp}>
          <NumberInput value={cmd.turns} min={1} max={99} allowEmpty onChange={(n) => patch({ turns: n }, 'turns')} />
        </Field>
      )}
      <p className="help">{S.addConditionHelp}</p>
    </div>
  )
}

export function RemoveConditionForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <ConditionPick label={S.removeConditionPick} value={cmd.removeCondition ?? ''} withAll onChange={(v) => patch({ removeCondition: v })} />
      <FilterPicker label={S.removeConditionWho} value={asFilter(cmd.from) ?? {}} onChange={(f) => patch({ from: f ?? {} })} />
      <p className="help">{S.removeConditionHelp}</p>
    </div>
  )
}
