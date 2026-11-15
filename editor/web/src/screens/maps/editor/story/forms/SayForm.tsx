// "Say something": who speaks (a named unit, nobody, or someone else by name), an optional portrait, the text.
import { useId } from 'react'
import { portraitList } from '../../../../../model/portraits'
import type { Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { Select } from '../../../../../ui/Select'
import { TextInput } from '../../../../../ui/TextInput'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const NOBODY = '__nobody__'
const OTHER = '__other__'

export function SayForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc, units } = useStory()
  const { patch } = useCommandEdit(at)
  const list = useId()
  const speaker = cmd.speaker !== undefined ? cmd.speaker : cmd.speakerName !== undefined ? OTHER : NOBODY
  const known = units.some((u) => u.id === speaker)
  const options = [
    { value: NOBODY, label: S.speakerNobody },
    ...(speaker !== NOBODY && speaker !== OTHER && !known ? [{ value: speaker, label: S.missing(speaker) }] : []),
    ...units.map((u) => ({ value: u.id, label: u.label })),
    { value: OTHER, label: S.speakerOther },
  ]
  const choose = (v: string) => {
    if (v === NOBODY) patch({ speaker: undefined, speakerName: undefined })
    else if (v === OTHER) patch({ speaker: undefined, speakerName: cmd.speakerName ?? '' })
    else patch({ speaker: v, speakerName: undefined })
  }
  return (
    <div className="stack tight">
      <Field label={S.speaker}><Select value={speaker} options={options} onChange={choose} /></Field>
      {speaker === OTHER && (
        <Field label={S.speakerName}><TextInput value={cmd.speakerName ?? ''} onChange={(v) => patch({ speakerName: v }, 'speakerName')} /></Field>
      )}
      <Field label={S.portrait} help={S.portraitHelp}>
        <TextInput list={list} value={cmd.portrait ?? ''} onChange={(v) => patch({ portrait: v === '' ? undefined : v }, 'portrait')} />
        <datalist id={list}>{portraitList(doc).map((p) => <option key={p.id} value={p.id} />)}</datalist>
      </Field>
      <Field label={S.sayText} help={S.sayTextHelp}>
        <TextInput data-learn="say-text" multiline rows={4} value={String(cmd.say ?? '')} onChange={(v) => patch({ say: v }, 'say')} />
      </Field>
    </div>
  )
}
