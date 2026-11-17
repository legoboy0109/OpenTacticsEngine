// Story commands about how a scene looks (docs/EVENTS.md "Scene presentation"): the background, fades, pauses, shakes, who stands on
// the stage, the weather and narration. Presentation only: none of them changes the rules. The same forms serve map events and
// campaign scenes (weather is for map events only, so a scene's list never offers it).
import { backgroundIds } from '../../../../../model/backgrounds'
import { MAX_AUTO_MS, MAX_SCENE_MS, MAX_SHAKE_STRENGTH, MAX_WAIT_MS } from '../../../../../model/sceneChecks'
import type { Command } from '../../../../../model/storyTypes'
import { ColorInput } from '../../../../../ui/ColorInput'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { TextInput } from '../../../../../ui/TextInput'
import { Check } from '../../../../../ui/Toggle'
import { PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const NONE = '__none__'

/** A length in milliseconds. */
function MsField({ label, help, value, min = 0, max, onChange, allowEmpty = false, placeholder }: {
  label: string; help?: string; value: number | undefined; min?: number; max: number; onChange: (v: number | undefined) => void; allowEmpty?: boolean; placeholder?: string
}) {
  return (
    <Field label={label} help={help}>
      <NumberInput value={value} min={min} max={max} allowEmpty={allowEmpty} placeholder={placeholder} suffix={S.sceneMs} onChange={onChange} />
    </Field>
  )
}

export function BackgroundForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const ids = backgroundIds(doc)
  const clearing = cmd.background === null
  const options = [{ value: NONE, label: S.sceneBackgroundNone }, ...ids.map((id) => ({ value: id, label: id }))]
  const fade = cmd.transition === 'fade'
  return (
    <div className="stack tight">
      {ids.length === 0 && !clearing && cmd.background === '' ? (
        <p className="help">{S.noBackgrounds}</p>
      ) : (
        <PickSelect label={S.sceneBackground} help={S.sceneBackgroundHelp} value={clearing ? NONE : String(cmd.background ?? '')} options={options}
          onChange={(v) => patch({ background: v === NONE ? null : v })} />
      )}
      <PickSelect label={S.sceneTransition} value={fade ? 'fade' : 'cut'} options={[{ value: 'cut', label: S.sceneTransitionCut }, { value: 'fade', label: S.sceneTransitionFade }]}
        onChange={(v) => patch(v === 'fade' ? { transition: 'fade', ms: cmd.ms ?? 500 } : { transition: undefined, ms: undefined })} />
      {fade && <MsField label={S.sceneTransitionMs} value={cmd.ms ?? 500} max={MAX_SCENE_MS} onChange={(v) => patch({ ms: v ?? 500 }, 'ms')} />}
    </div>
  )
}

export function FadeForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const out = cmd.fade !== 'in'
  return (
    <div className="stack tight">
      <PickSelect label={S.sceneFadeWhich} help={S.sceneFadeHelp} value={out ? 'out' : 'in'} options={[{ value: 'out', label: S.sceneFadeOut }, { value: 'in', label: S.sceneFadeIn }]}
        onChange={(v) => patch(v === 'in' ? { fade: 'in', color: undefined } : { fade: 'out', color: cmd.color ?? '#000000' })} />
      <MsField label={S.sceneFadeMs} value={cmd.ms ?? 500} max={MAX_SCENE_MS} onChange={(v) => patch({ ms: v ?? 500 }, 'ms')} />
      {out && (
        <Field label={S.sceneFadeColor}>
          <ColorInput value={cmd.color ?? '#000000'} fallback="#000000" onChange={(v) => patch({ color: v }, 'color')} />
        </Field>
      )}
    </div>
  )
}

export function WaitForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return <MsField label={S.sceneWaitMs} help={S.sceneWaitHelp} value={typeof cmd.wait === 'number' ? cmd.wait : 0} max={MAX_WAIT_MS} onChange={(v) => patch({ wait: v ?? 0 }, 'wait')} />
}

export function ShakeForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const sh = cmd.shake ?? {}
  return (
    <div className="stack tight">
      <p className="help">{S.sceneShakeHelp}</p>
      <MsField label={S.sceneShakeMs} value={sh.ms ?? 400} max={MAX_SCENE_MS} onChange={(v) => patch({ shake: { ...sh, ms: v ?? 400 } }, 'shake-ms')} />
      <Field label={S.sceneShakeStrength}>
        <NumberInput value={sh.strength ?? 3} min={1} max={MAX_SHAKE_STRENGTH} suffix="pixels" onChange={(v) => patch({ shake: { ...sh, strength: v ?? 3 } }, 'shake-strength')} />
      </Field>
    </div>
  )
}

export function LeaveForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <Field label={S.sceneLeave} help={S.sceneLeaveHelp}>
      <TextInput value={cmd.leave ?? ''} onChange={(v) => patch({ leave: v }, 'leave')} />
    </Field>
  )
}

export function ClearStageForm() {
  return <p className="help">{S.sceneClearStageHelp}</p>
}

export function WeatherForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <PickSelect label={S.sceneWeather} help={S.sceneWeatherHelp} value={cmd.weather ?? 'rain'}
      options={[{ value: 'rain', label: S.sceneWeatherRain }, { value: 'snow', label: S.sceneWeatherSnow }, { value: 'none', label: S.sceneWeatherNone }]}
      onChange={(v) => patch({ weather: v })} />
  )
}

/** "Goes on by itself after N milliseconds" (a `say` / `narrate` line). */
export function AutoField({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return <MsField label={S.sceneAuto} help={S.sceneAutoHelp} value={cmd.auto} min={1} max={MAX_AUTO_MS} allowEmpty placeholder="Waits for the player" onChange={(v) => patch({ auto: v }, 'auto')} />
}

export function NarrateForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <Field label={S.sceneNarrate} help={S.sceneNarrateHelp}>
        <TextInput multiline rows={4} value={String(cmd.narrate ?? '')} onChange={(v) => patch({ narrate: v }, 'narrate')} />
      </Field>
      <AutoField cmd={cmd} at={at} />
    </div>
  )
}

/** Where a speaker stands (a `say` line): beside the box as usual, or left / right / middle of the picture. */
export function PositionField({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <PickSelect label={S.scenePosition} help={S.scenePositionHelp} value={cmd.position ?? 'box'}
      options={[{ value: 'box', label: S.scenePositionBox }, { value: 'left', label: S.scenePositionLeft }, { value: 'right', label: S.scenePositionRight }, { value: 'center', label: S.scenePositionCenter }]}
      onChange={(v) => patch({ position: v === 'box' ? undefined : v })} />
  )
}

/** focus: how long the scroll takes, and whether the next command waits for it. */
export function FocusExtras({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <>
      <MsField label={S.focusMs} help={S.focusMsHelp} value={cmd.ms} max={MAX_SCENE_MS} allowEmpty placeholder="Usual" onChange={(v) => patch({ ms: v }, 'ms')} />
      {cmd.ms !== undefined && <Check label={S.focusWait} checked={cmd.wait !== false} onChange={(v) => patch({ wait: v ? undefined : false })} />}
    </>
  )
}

/** moveUnit: walk its way, and whether the next command waits for the walk. */
export function MoveExtras({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <>
      <Check label={S.moveWalk} help={S.moveWalkHelp} checked={cmd.walk === true} onChange={(v) => patch(v ? { walk: true } : { walk: undefined, wait: undefined })} />
      {cmd.walk === true && <Check label={S.moveWait} checked={cmd.wait !== false} onChange={(v) => patch({ wait: v ? undefined : false })} />}
    </>
  )
}
