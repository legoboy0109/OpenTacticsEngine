import { featureEnabled, setFeature, setFeatureField, type FeatureKey } from '../../model/rules'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check, Toggle } from '../../ui/Toggle'
import { FormulaEditor } from './FormulaEditor'
import { T } from './text'

/** Feature switches. Each reveals its settings when on; off keeps them in the file. */
export function FeaturesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const rules = doc.game!.rules
  const on = (k: FeatureKey) => featureEnabled(doc, k)
  const flip = (k: FeatureKey) => (v: boolean) => edit((d) => setFeature(d, k, v))
  const hasResources = (doc.game!.resources ?? []).length > 0

  return (
    <Card title={T.features} help={T.featuresHelp}>
      <div className="features">
        <div className="feature">
          <Toggle label={T.economy} help={T.economyHelp} checked={on('economy')} onChange={flip('economy')} />
          {on('economy') && !hasResources && <p className="help feature-body">{T.economyNoResources}</p>}
        </div>

        <div className="feature">
          <Toggle label={T.production} help={T.productionHelp} checked={on('production')} onChange={flip('production')} />
          {on('production') && (
            <div className="feature-body">
              <Check label={T.builtCanAct} help={T.builtCanActHelp} checked={rules?.production?.builtUnitsCanAct === true}
                onChange={(v) => edit((d) => setFeatureField(d, 'production', 'builtUnitsCanAct', v))} />
            </div>
          )}
        </div>

        <div className="feature">
          <Toggle label={T.capture} help={T.captureHelp} checked={on('capture')} onChange={flip('capture')} />
          {on('capture') && <div className="feature-body"><FormulaEditor fkey="capture" label={T.captureGroup} help="How quickly buildings change hands." /></div>}
        </div>

        <div className="feature">
          <Toggle label={T.progression} help={T.progressionHelp} checked={on('progression')} onChange={flip('progression')} />
          {on('progression') && (
            <div className="feature-body stack">
              <div className="form-grid cols-2">
                <Field label={T.xpPerLevel} help={T.xpPerLevelHelp}>
                  <NumberInput value={rules?.progression?.xpPerLevel} min={1} max={100000}
                    onChange={(v) => { if (v !== undefined) edit((d) => setFeatureField(d, 'progression', 'xpPerLevel', v), 'rules:xpPerLevel') }} />
                </Field>
                <Field label={T.maxLevel} help={T.maxLevelHelp}>
                  <NumberInput value={rules?.progression?.maxLevel} min={1} max={999}
                    onChange={(v) => { if (v !== undefined) edit((d) => setFeatureField(d, 'progression', 'maxLevel', v), 'rules:maxLevel') }} />
                </Field>
              </div>
              <FormulaEditor fkey="xpHit" label={T.xpHit} />
              <FormulaEditor fkey="xpKill" label={T.xpKill} />
            </div>
          )}
        </div>
      </div>
    </Card>
  )
}
