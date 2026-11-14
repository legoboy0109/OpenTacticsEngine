import { SAVING_MODES, savingMode, setSavingMode } from '../../model/saving'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { T } from './text'

/** Who may save: anywhere (Continue + slots), only the automatic Continue save, or nothing. */
export function SavingCard() {
  const mode = savingMode(useDoc())
  const edit = useEdit()
  return (
    <Card title={T.saving} help={T.savingHelp}>
      <div className="kind-row" role="radiogroup" aria-label={T.saving}>
        {SAVING_MODES.map((m) => (
          <button key={m} type="button" role="radio" aria-checked={mode === m} className="pick pick-inline" onClick={() => edit((d) => setSavingMode(d, m))}>
            <span className="pick-name">{T.savingChoices[m].label}</span>
          </button>
        ))}
      </div>
      <p className="help">{T.savingChoices[mode].help}</p>
    </Card>
  )
}
