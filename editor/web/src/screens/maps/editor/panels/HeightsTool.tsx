// The Heights tool's panel: raise, lower or set squares. Clicking or dragging on the map applies it (tools.ts).
import { HEIGHT_MAX, HEIGHT_MIN, type HeightMode } from '../../../../model/mapHeights'
import { useCellWords } from '../../../../ui/cellWords'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import type { ToolState } from '../tools'
import { T } from '../text'

const MODES: HeightMode[] = ['raise', 'lower', 'set']

export function HeightsTool({ state, onChange }: { state: ToolState; onChange: (patch: Partial<ToolState>) => void }) {
  const words = useCellWords()
  return (
    <div className="stack tight" data-testid="heights-tool">
      <h3>{T.heightsTitle}</h3>
      <p className="help">{words(T.heightsHelp)}</p>
      <div className="kind-row" role="radiogroup" aria-label={T.heightsTitle}>
        {MODES.map((m) => (
          <button key={m} type="button" role="radio" aria-checked={state.heightMode === m} className="pick pick-inline" onClick={() => onChange({ heightMode: m })}>
            <span className="pick-name">{T.heightModes[m]}</span>
          </button>
        ))}
      </div>
      {state.heightMode === 'set' && (
        <Field label={T.heightValue} help={T.heightRange(HEIGHT_MIN, HEIGHT_MAX)}>
          <NumberInput value={state.heightValue} min={HEIGHT_MIN} max={HEIGHT_MAX} onChange={(v) => { if (v !== undefined) onChange({ heightValue: v }) }} />
        </Field>
      )}
      <p className="help">{words(T.heightsNote)}</p>
    </div>
  )
}
