// "Ask a question" (dialogue choices, docs/EVENTS.md): who asks, the question, the variable that remembers the answer, and the
// two to four answers (words, an optional "only offered if" list, order). What each answer DOES is its own command list, shown
// under the question in the command list (CommandList.tsx), like the branches of an If.
import { listAdd, listMove, listPatch, listRemove } from '../../../../../model/storyEdit'
import { MAX_ANSWERS, MIN_ANSWERS, type ChoiceAnswer, type Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { Icon } from '../../../../../ui/Icon'
import { RowButtons } from '../../../../../ui/RowButtons'
import { Select } from '../../../../../ui/Select'
import { TextInput } from '../../../../../ui/TextInput'
import { ConditionList } from '../ConditionList'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { ChoiceLookFields, FaceField } from './DialogueForms'
import { SpeakerFields } from './SayForm'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const NONE = '__none__'

export function ChoiceForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const answers = cmd.answers ?? []
  const set = (next: ChoiceAnswer[], key?: string) => patch({ answers: next }, key && `answers:${key}`)
  const variables = doc.game?.variables ?? []
  const store = cmd.store ?? ''
  const options = [
    { value: NONE, label: S.choiceStoreNone },
    ...(store !== '' && !variables.some((v) => v.id === store) ? [{ value: store, label: S.missing(store) }] : []),
    ...variables.map((v) => ({ value: v.id, label: v.name || v.id })),
  ]
  return (
    <div className="stack tight" data-testid="choice-form">
      {cmd.notice !== true && <><SpeakerFields cmd={cmd} at={at} /><FaceField cmd={cmd} at={at} /></>}
      <ChoiceLookFields cmd={cmd} at={at} />
      <Field label={S.choiceQuestion} help={S.choiceQuestionHelp}>
        <TextInput multiline rows={3} value={String(cmd.choice ?? '')} onChange={(v) => patch({ choice: v }, 'choice')} />
      </Field>
      <Field label={S.choiceStore} help={variables.length ? S.choiceStoreHelp : S.noVariables}>
        <Select value={store === '' ? NONE : store} options={options} onChange={(v) => patch({ store: v === NONE ? undefined : v })} />
      </Field>
      <h4>{S.answersTitle}</h4>
      <p className="help">{S.answersHelp}</p>
      {answers.map((a, k) => (
        <div className="story-row choice-answer" role="group" aria-label={S.answerN(k + 1)} key={k}>
          <div className="story-row-head">
            <Field label={`${S.answerText} (${S.answerN(k + 1)})`}>
              <TextInput value={a.text ?? ''} onChange={(v) => set(listPatch(answers, k, { text: v }), `${k}:text`)} />
            </Field>
            <RowButtons index={k} count={answers.length} up={S.moveAnswerUp(k + 1)} down={S.moveAnswerDown(k + 1)} del={S.deleteAnswer(k + 1)}
              canDelete={answers.length > MIN_ANSWERS}
              onUp={() => set(listMove(answers, k, k - 1))} onDown={() => set(listMove(answers, k, k + 1))} onDelete={() => set(listRemove(answers, k))} />
          </div>
          <p className="help"><strong>{S.answerOnlyIf}</strong> {S.answerOnlyIfHelp}</p>
          <ConditionList name={`Answer ${k + 1}`} conditions={a.if ?? []}
            onChange={(list, key) => set(listPatch(answers, k, { if: list.length ? list : undefined }), key && `${k}:if:${key}`)} />
        </div>
      ))}
      <button type="button" className="btn sm" disabled={answers.length >= MAX_ANSWERS} onClick={() => set(listAdd(answers, { text: '' }))}>
        <Icon name="plus" />{S.addAnswer}
      </button>
      {answers.length >= MAX_ANSWERS && <p className="help">{S.answersFull}</p>}
    </div>
  )
}
