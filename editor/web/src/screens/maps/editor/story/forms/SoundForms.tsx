// Story commands about sound (docs/SOUND.md): play music (with an optional fade), stop the music, play one sound.
// Presentation only: they change what is heard, never the rules.
import type { Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { MusicPick, PickSelect, SidePick, SoundPickStory } from '../pickers'
import { useStory } from '../StoryContext'
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

/** setPhaseMusic (docs/SOUND.md "Music by situation"): a side's turn music for the rest of the battle. */
export function PhaseMusicForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc } = useStory()
  const p = cmd.setPhaseMusic ?? {}
  const mode = p.music === null ? 'clear' : p.music === '' ? 'none' : 'track'
  return (
    <div className="stack tight">
      <p className="help">{S.phaseMusicHelp}</p>
      <SidePick label={S.phaseMusicSide} help={S.phaseMusicSideHelp} value={p.side ?? ''} onChange={(v) => patch({ setPhaseMusic: { ...p, side: v } }, 'side')} />
      <PickSelect label={S.phaseMusicWhat} value={mode}
        options={[{ value: 'track', label: S.phaseMusicTrackChoice }, { value: 'none', label: S.phaseMusicNone }, { value: 'clear', label: S.phaseMusicClear }]}
        onChange={(v) => patch({ setPhaseMusic: { side: p.side, music: v === 'clear' ? null : v === 'none' ? '' : (p.music || Object.keys(doc.audio?.music ?? {})[0] || '') } })} />
      {mode === 'track' && <MusicPick label={S.phaseMusicTrack} value={p.music ?? ''} onChange={(v) => patch({ setPhaseMusic: { ...p, music: v } }, 'track')} />}
    </div>
  )
}

export function PlaySoundForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return <SoundPickStory label={S.playSoundWhich} help={S.playSoundHelp} value={cmd.playSound ?? ''} onChange={(v) => patch({ playSound: v })} />
}
