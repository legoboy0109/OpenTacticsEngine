// "Sounds on frames" of one battle animation (docs/BATTLE_SCENE.md "Sounds"): a sound that plays when a frame begins. One row per
// sound: the frame, the sound, a remove button; "Add a sound" adds the next frame that has none.
import { animSounds, setAnimSound } from '../../../model/audioSituations'
import { soundIds } from '../../../model/audio'
import type { BattleAnimDef } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { T } from './text'

export function AnimSounds({ setId, animKey, label, anim }: { setId: string; animKey: string; label: string; anim: BattleAnimDef }) {
  const doc = useDoc()
  const edit = useEdit()
  const ids = soundIds(doc)
  const rows = animSounds(anim)
  const used = new Set(rows.map(([f]) => f))
  const nextFrame = Array.from({ length: anim.frames.length }, (_, i) => i).find((f) => !used.has(f))
  const group = (frame: number) => `${setId}:${animKey}:sound${frame}`
  if (ids.length === 0 && rows.length === 0) return null  // no sounds in the game yet: nothing to pick (the Sound screen adds them)
  return (
    <Field label={T.sounds} help={T.soundsHelp}>
      <div className="stack tight" data-testid={`anim-sounds-${animKey}`}>
        {rows.map(([frame, id]) => (
          <div className="row" key={frame}>
            <NumberInput aria-label={`${label}: ${T.soundFrame}`} min={1} max={anim.frames.length} value={frame + 1}
              onChange={(v) => v !== undefined && v - 1 !== frame && !used.has(v - 1) && edit((d) => setAnimSound(setAnimSound(d, setId, animKey, frame, undefined), setId, animKey, v - 1, id), group(frame))} />
            <Select aria-label={`${label}: ${T.soundWhich(frame + 1)}`} value={id}
              options={[...(ids.includes(id) ? [] : [{ value: id, label: T.soundMissing(id) }]), ...ids.map((s) => ({ value: s, label: s }))]}
              onChange={(v) => edit((d) => setAnimSound(d, setId, animKey, frame, v), group(frame))} />
            <button type="button" className="btn sm" aria-label={T.soundRemove(frame + 1)} onClick={() => edit((d) => setAnimSound(d, setId, animKey, frame, undefined))}>{T.removeShort}</button>
          </div>
        ))}
        {nextFrame !== undefined && ids.length > 0 && (
          <div><button type="button" className="btn sm" onClick={() => edit((d) => setAnimSound(d, setId, animKey, nextFrame, ids[0]))}>{T.soundAdd}</button></div>
        )}
      </div>
    </Field>
  )
}
