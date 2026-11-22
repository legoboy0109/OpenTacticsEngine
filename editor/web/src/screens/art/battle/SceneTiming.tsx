// How long a held pose waits after a blow (data/graphics.json "battle" > "scene"; docs/BATTLE_SCENE.md "Holding a pose"). Shown under the sets; it matters only to
// animations that "wait on a frame".
import { HIT_HOLD_MAX, RESULT_HOLD_MAX, hitHoldOf, sceneTiming, setHitHold, setHitHoldOn, setSceneTiming, type HitHoldKey } from '../../../model/battleHold'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Toggle } from '../../../ui/Toggle'
import { T } from './text'

export function SceneTiming() {
  const doc = useDoc()
  const edit = useEdit()
  const t = sceneTiming(doc)
  const hh = hitHoldOf(doc)
  const hhField = (key: HitHoldKey, label: string, placeholder: number) => (
    <Field key={key} label={label}>
      <NumberInput aria-label={label} integer={false} allowEmpty min={0} max={HIT_HOLD_MAX[key]} value={hh?.[key]} placeholder={String(placeholder)}
        onChange={(v) => edit((d) => setHitHold(d, key, v), `scene:hithold:${key}`)} />
    </Field>
  )
  return (
    <Field label={T.sceneTiming} help={T.sceneTimingHelp}>
      <div className="battle-fields" data-testid="scene-timing">
        <Field label={T.resultHold}>
          <NumberInput aria-label={T.resultHold} min={0} max={RESULT_HOLD_MAX} value={t.resultHoldMs} onChange={(v) => v !== undefined && edit((d) => setSceneTiming(d, { resultHoldMs: v }), 'scene:hold')} />
        </Field>
        <Field label={T.resultHoldAgain}>
          <NumberInput aria-label={T.resultHoldAgain} min={0} max={RESULT_HOLD_MAX} value={t.resultHoldAgainMs} onChange={(v) => v !== undefined && edit((d) => setSceneTiming(d, { resultHoldAgainMs: v }), 'scene:again')} />
        </Field>
      </div>
      <Toggle label={T.hitHold} help={T.hitHoldHelp} checked={hh !== undefined} onChange={(v) => edit((d) => setHitHoldOn(d, v))} />
      {hh !== undefined && (
        <div className="battle-fields" data-testid="hit-hold">
          {hhField('minMs', T.hitHoldMin, t.resultHoldMs)}
          {hhField('baseMs', T.hitHoldBase, 0)}
          {hhField('perHpMs', T.hitHoldPerHp, 0)}
          {hhField('killMs', T.hitHoldKill, 0)}
          {hhField('noChangeMs', T.hitHoldNoChange, hh.minMs ?? t.resultHoldMs)}
        </div>
      )}
    </Field>
  )
}
