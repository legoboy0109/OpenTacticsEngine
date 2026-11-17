// Story commands about sound (docs/SOUND.md): play music (with an optional fade), stop the music, play one sound.
// Presentation only: they change what is heard, never the rules.
import type { Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { MusicPick, SoundPickStory } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

/** Seconds, 0 to 60, or empty for "the player's default". */
function FadeField({ cmd, at, help }: { cmd: Command; at: CommandRef; help: string }) {
  const { patch } = useCommandEdit(at)
  return (
    <Field label={S.fadeLabel} help={help}>
      <NumberInput value={typeof cmd.fade === 'number' ? cmd.fade : undefined} min={0} max={60} integer={false} allowEmpty placeholder="Default" suffix="seconds"
        onChange={(v) => patch({ fade: v }, 'fade')} />
    </Field>
  )
}

export function PlayMusicForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <MusicPick label={S.playMusicTrack} help={S.playMusicHelp} value={cmd.playMusic ?? ''} onChange={(v) => patch({ playMusic: v })} />
      <FadeField cmd={cmd} at={at} help={S.fadeInHelp} />
    </div>
  )
}

export function StopMusicForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  return (
    <div className="stack tight">
      <p className="help">{S.stopMusicHelp}</p>
      <FadeField cmd={cmd} at={at} help={S.fadeOutHelp} />
    </div>
  )
}

export function PlaySoundForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return <SoundPickStory label={S.playSoundWhich} help={S.playSoundHelp} value={cmd.playSound ?? ''} onChange={(v) => patch({ playSound: v })} />
}
