// The objectives form: the text, "Win when any of these happens" and "Lose when any of these happens". Used by the
// Objectives tab and by the "Change the objectives" command (which holds the same thing).
import { newRule } from '../../../../model/storyDefaults'
import { listAdd, listMove, listPatch, listRemove } from '../../../../model/storyEdit'
import { rectOf, ruleKind, type ObjectiveRule, type Objectives, type RuleKind } from '../../../../model/storyTypes'
import { ruleText } from '../../../../model/storySummary'
import { Field, FieldGroup } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { AreaField } from './AreaField'
import { FilterPicker } from './pickers'
import { RowButtons } from '../../../../ui/RowButtons'
import { useStory } from './StoryContext'
import { S } from './text'

interface Props {
  value: Objectives
  /** The new objectives; `key` is set while typing so quick edits make one undo step. */
  onChange: (next: Objectives, key?: string) => void
  /** Makes names unique when two editors can show at once. */
  name: string
}

const WIN_KINDS: RuleKind[] = ['defeated', 'reach', 'round', 'survive']
const LOSE_KINDS: RuleKind[] = ['defeated', 'reach', 'round']

export function ObjectiveEditor({ value, onChange, name }: Props) {
  return (
    <div className="stack tight" data-testid={`objectives-${name}`}>
      <Field label={S.objectiveText} help={S.objectiveTextHelp}>
        <TextInput data-learn="objective-text" multiline rows={2} value={value.text ?? ''} onChange={(v) => onChange({ ...value, text: v }, 'text')} />
      </Field>
      <RuleList win value={value.win ?? []} onChange={(l, k) => onChange({ ...value, win: l }, k)} name={name} />
      <RuleList win={false} value={value.lose ?? []} onChange={(l, k) => onChange({ ...value, lose: l }, k)} name={name} />
    </div>
  )
}

interface ListProps { win: boolean; value: ObjectiveRule[]; onChange: (next: ObjectiveRule[], key?: string) => void; name: string }

function RuleList({ win, value, onChange, name }: ListProps) {
  const story = useStory()
  const kinds = win ? WIN_KINDS : LOSE_KINDS
  const title = win ? S.winWhen : S.loseWhen
  const label = (i: number) => (win ? S.winN(i + 1) : S.loseN(i + 1))
  const patch = (i: number, p: Record<string, unknown>, key?: string) => onChange(listPatch(value, i, p), key && `${win ? 'win' : 'lose'}:${i}:${key}`)
  return (
    <FieldGroup label={title}>
      <div className="story-list" data-testid={`${win ? 'win' : 'lose'}-rules-${name}`}>
        {value.length === 0 && <p className="help">{S.noRules}</p>}
        {value.map((r, i) => {
          const kind = ruleKind(r)
          return (
            <div className="story-row rule" role="group" aria-label={`${name}: ${label(i)}`} key={i}>
              <div className="story-row-head">
                <strong className="story-sentence">{ruleText(story, r)}</strong>
                <RowButtons index={i} count={value.length} up={S.moveRuleUp(label(i))} down={S.moveRuleDown(label(i))} del={S.deleteRule(label(i))}
                  onUp={() => onChange(listMove(value, i, i - 1))} onDown={() => onChange(listMove(value, i, i + 1))} onDelete={() => onChange(listRemove(value, i))} />
              </div>
              {kind === 'defeated' && (
                <>
                  <FilterPicker label={S.ruleDefeatedWho} value={r.defeated} onChange={(f) => patch(i, { defeated: f ?? {} })} />
                  <p className="help">{S.ruleDefeatedHelp}</p>
                </>
              )}
              {kind === 'reach' && (
                <>
                  <FilterPicker label={S.ruleReachWho} mode="default" defaultLabel={win ? S.ruleReachDefaultWin : S.ruleReachDefaultLose} value={r.by} onChange={(f) => patch(i, { by: f })} />
                  <AreaField label={`${label(i)}: area`} mode="area" value={rectOf(r.reach)} onChange={(a) => patch(i, { reach: a })} />
                </>
              )}
              {kind === 'round' && (
                <Field label={S.ruleRound}><NumberInput value={r.round} min={1} max={100000} onChange={(v) => { if (v !== undefined) patch(i, { round: v }, 'round') }} /></Field>
              )}
              {kind === 'survive' && (
                <Field label={S.ruleSurvive}><NumberInput value={r.survive} min={1} max={100000} onChange={(v) => { if (v !== undefined) patch(i, { survive: v }, 'survive') }} /></Field>
              )}
            </div>
          )
        })}
        <Select data-learn={win ? 'add-win' : undefined} aria-label={`${name}: ${win ? S.addWin : S.addLose}`} value="" placeholder={win ? S.addWin : S.addLose}
          options={kinds.map((k) => ({ value: k, label: S.ruleKinds[k] }))} onChange={(k) => onChange(listAdd(value, newRule(story, k as RuleKind)))} />
      </div>
    </FieldGroup>
  )
}
