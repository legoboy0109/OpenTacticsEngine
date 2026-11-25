// Forms for the dialogue extras (docs/EVENTS.md "Speech bubbles, notices, names, faces and loops"): a line's portrait face, a notice, a
// question's notice window and starting answer, a loop and its break, and asking for a character's name. Placeholder wording (dialogueText.ts).
import { DEFAULT_LOOP, DEFAULT_NAME_LETTERS, MAX_FACE, MAX_LOOP, MAX_NAME_LETTERS } from '../../../../../model/storyDialogue'
import type { Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { TextInput } from '../../../../../ui/TextInput'
import { Check } from '../../../../../ui/Toggle'
import { UnitPick } from '../pickers'
import { AutoField } from './SceneForms'
import { DX } from './dialogueText'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

/** "Face (expression)": the N-th expression of the speaker's portrait (a say line or a question). */
export function FaceField({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <Field label={DX.face} help={DX.faceHelp}>
      <NumberInput aria-label={DX.face} value={cmd.face} min={1} max={MAX_FACE} allowEmpty placeholder={DX.facePlaceholder} onChange={(v) => patch({ face: v }, 'face')} />
    </Field>
  )
}

/** A question's notice window switch and its starting answer. */
export function ChoiceLookFields({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const count = Math.max(1, cmd.answers?.length ?? 0)
  return (
    <>
      <Check label={DX.choiceNotice} help={DX.choiceNoticeHelp} checked={cmd.notice === true}
        // A notice question has no speaker, name or portrait (the loader refuses them): switching it on clears them.
        onChange={(v) => patch(v ? { notice: true, speaker: undefined, speakerName: undefined, portrait: undefined, face: undefined } : { notice: undefined })} />
      <Field label={DX.choiceCursor} help={DX.choiceCursorHelp}>
        <NumberInput aria-label={DX.choiceCursor} value={cmd.cursor} min={1} max={count} allowEmpty placeholder={DX.choiceCursorPlaceholder} onChange={(v) => patch({ cursor: v }, 'cursor')} />
      </Field>
    </>
  )
}

export function NoticeForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <Field label={DX.noticeText} help={DX.noticeTextHelp}>
        <TextInput multiline rows={3} value={typeof cmd.notice === 'string' ? cmd.notice : ''} onChange={(v) => patch({ notice: v }, 'notice')} />
      </Field>
      <AutoField cmd={cmd} at={at} />
    </div>
  )
}

/** The commands inside a loop are the nested list under it (CommandList.tsx); this form holds the limit. */
export function LoopForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <p className="help">{DX.loopHelp}</p>
      <Field label={DX.loopMax} help={DX.loopMaxHelp}>
        <NumberInput aria-label={DX.loopMax} value={cmd.max} min={1} max={MAX_LOOP} allowEmpty placeholder={String(DEFAULT_LOOP)} onChange={(v) => patch({ max: v }, 'max')} />
      </Field>
    </div>
  )
}

export function BreakForm() {
  return <p className="help">{DX.breakHelp}</p>
}

/** "Ask for a character's name": campaign scenes only. */
export function AskNameForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <UnitPick label={DX.nameWho} help={DX.nameWhoHelp} value={cmd.askName ?? ''} onChange={(v) => patch({ askName: v })} />
      <Field label={DX.nameDefault} help={DX.nameDefaultHelp}>
        <TextInput value={cmd.default ?? ''} onChange={(v) => patch({ default: v === '' ? undefined : v }, 'default')} />
      </Field>
      <Field label={DX.nameMax} help={DX.nameMaxHelp}>
        <NumberInput aria-label={DX.nameMax} value={cmd.max} min={1} max={MAX_NAME_LETTERS} allowEmpty placeholder={String(DEFAULT_NAME_LETTERS)} onChange={(v) => patch({ max: v }, 'max')} />
      </Field>
      <Field label={DX.namePrompt} help={DX.namePromptHelp}>
        <TextInput placeholder={DX.namePromptPlaceholder} value={cmd.prompt ?? ''} onChange={(v) => patch({ prompt: v === '' ? undefined : v }, 'prompt')} />
      </Field>
    </div>
  )
}
