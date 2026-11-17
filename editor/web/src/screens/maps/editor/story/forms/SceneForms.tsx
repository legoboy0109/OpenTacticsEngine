// Story commands about how a scene looks (docs/EVENTS.md "Scene presentation"): the background, fades, pauses, shakes, who stands on
// the stage, the weather and narration. Presentation only: none of them changes the rules. The same forms serve map events and
// campaign scenes (weather is for map events only, so a scene's list never offers it).
import { backgroundIds } from '../../../../../model/backgrounds'
import { MAX_AUTO_MS, MAX_SCENE_MS, MAX_SHAKE_STRENGTH, MAX_WAIT_MS } from '../../../../../model/sceneChecks'
import { STAGE_POSITIONS, type Command, type StagePosition } from '../../../../../model/storyTypes'
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

/** The "how" of a stage command: a cut (the default) or a fade for enter / leave / clear; a slide (the default) or a jump for move. */
function StageHow({ cmd, at, kind }: { cmd: Command; at: CommandRef; kind: 'enter' | 'leave' | 'move' }) {
  const { patch } = useCommandEdit(at)
  const move = kind === 'move'
  const fading = move ? cmd.transition !== 'cut' : cmd.transition === 'fade'
  const fadeLabel = move ? S.sceneTransitionMoveSlide : kind === 'enter' ? S.sceneTransitionEnterFade : S.sceneTransitionLeaveFade
  const cutLabel = move ? S.sceneTransitionMoveCut : S.sceneTransitionEnterCut
  return (
    <>
      <PickSelect label={S.sceneTransitionOnStage} value={fading ? 'fade' : 'cut'} options={[{ value: 'cut', label: cutLabel }, { value: 'fade', label: fadeLabel }]}
        onChange={(v) => patch(move ? (v === 'cut' ? { transition: 'cut' } : { transition: undefined, ms: undefined }) : (v === 'fade' ? { transition: 'fade' } : { transition: undefined, ms: undefined }))} />
      {fading && <MsField label={S.sceneTransitionStageMs} value={cmd.ms} allowEmpty placeholder={move ? '400' : '500'} max={MAX_SCENE_MS} onChange={(v) => patch({ ms: v }, 'ms')} />}
    </>
  )
}

export function LeaveForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <Field label={S.sceneLeave} help={S.sceneLeaveHelp}>
        <TextInput value={cmd.leave ?? ''} onChange={(v) => patch({ leave: v }, 'leave')} />
      </Field>
      <StageHow cmd={cmd} at={at} kind="leave" />
    </div>
  )
}

export function ClearStageForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  return (
    <div className="stack tight">
      <p className="help">{S.sceneClearStageHelp}</p>
      <StageHow cmd={cmd} at={at} kind="leave" />
    </div>
  )
}

const POSITION_LABEL: Record<StagePosition, string> = {
  farLeft: S.scenePositionFarLeft, left: S.scenePositionLeft, midLeft: S.scenePositionMidLeft, center: S.scenePositionCenter,
  midRight: S.scenePositionMidRight, right: S.scenePositionRight, farRight: S.scenePositionFarRight,
}
const positionOptions = () => STAGE_POSITIONS.map((p) => ({ value: p, label: POSITION_LABEL[p] }))

/** enter: a speaker comes on stage without a line. */
export function EnterForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <Field label={S.sceneEnter} help={S.sceneEnterHelp}>
        <TextInput value={cmd.enter ?? ''} onChange={(v) => patch({ enter: v }, 'enter')} />
      </Field>
      <PickSelect label={S.scenePlace} help={S.scenePlaceHelp} value={cmd.position ?? 'midLeft'} options={positionOptions()} onChange={(v) => patch({ position: v as StagePosition })} />
      <Field label={S.sceneEnterPortrait} help={S.sceneEnterPortraitHelp}>
        <TextInput value={cmd.portrait ?? ''} onChange={(v) => patch({ portrait: v === '' ? undefined : v }, 'portrait')} />
      </Field>
      <StageHow cmd={cmd} at={at} kind="enter" />
    </div>
  )
}

/** move: a speaker on the stage slides to another place. */
export function MoveStageForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <Field label={S.sceneMove} help={S.sceneMoveHelp}>
        <TextInput value={cmd.move ?? ''} onChange={(v) => patch({ move: v }, 'move')} />
      </Field>
      <PickSelect label={S.scenePlace} help={S.scenePlaceHelp} value={cmd.position ?? 'right'} options={positionOptions()} onChange={(v) => patch({ position: v as StagePosition })} />
      <StageHow cmd={cmd} at={at} kind="move" />
    </div>
  )
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
      options={[{ value: 'box', label: S.scenePositionBox }, ...positionOptions()]}
      onChange={(v) => patch({ position: v === 'box' ? undefined : (v as StagePosition) })} />
  )
}

/** focus: how long the scroll takes, and whether the next command waits for it. */
export function FocusExtras({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const how = cmd.speed !== undefined ? 'speed' : cmd.ms !== undefined ? 'time' : 'usual'
  return (
    <>
      <PickSelect label={S.focusHow} value={how} options={[{ value: 'usual', label: S.focusHowUsual }, { value: 'time', label: S.focusHowTime }, { value: 'speed', label: S.focusHowSpeed }]}
        onChange={(v) => patch(v === 'time' ? { ms: cmd.ms ?? 600, speed: undefined } : v === 'speed' ? { speed: cmd.speed ?? 4, ms: undefined } : { ms: undefined, speed: undefined, wait: undefined })} />
      {how === 'time' && <MsField label={S.focusMs} help={S.focusMsHelp} value={cmd.ms} max={MAX_SCENE_MS} allowEmpty placeholder="Usual" onChange={(v) => patch({ ms: v }, 'ms')} />}
      {how === 'speed' && (
        <Field label={S.focusSpeed} help={S.focusSpeedHelp}>
          <NumberInput value={cmd.speed ?? 4} min={0.1} max={100} integer={false} suffix="squares a second" onChange={(v) => patch({ speed: v ?? 4 }, 'speed')} />
        </Field>
      )}
      {how !== 'usual' && <Check label={S.focusWait} checked={cmd.wait !== false} onChange={(v) => patch({ wait: v ? undefined : false })} />}
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
