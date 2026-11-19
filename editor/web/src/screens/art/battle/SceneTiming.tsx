// How long a held pose waits after a blow (data/graphics.json "battle" > "scene"; docs/BATTLE_SCENE.md "Holding a pose"). Shown under the sets; it matters only to
// animations that "wait on a frame".
import { RESULT_HOLD_MAX, sceneTiming, setSceneTiming } from '../../../model/battleHold'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { T } from './text'

export function SceneTiming() {
  const doc = useDoc()
  const edit = useEdit()
  const t = sceneTiming(doc)
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
    </Field>
  )
}
