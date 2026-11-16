// Rules card "Strategy-RPG features": one switch per optional feature that has a file of its own. Switching on makes
// the screen appear in the sidebar; switching off asks first when something would be removed.
import { useState } from 'react'
import type { SrpgFeature } from '../../../model/srpgTypes'
import { SRPG_FEATURES, canSwitchOn, featureCount, srpgFeatureOn, switchOffFeature, switchOnFeature } from '../../../model/srpgFeatures'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { ConfirmDialog } from '../../../ui/Dialog'
import { Toggle } from '../../../ui/Toggle'
import { T } from '../../srpg/text'

export function FeatureSwitchesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const [asking, setAsking] = useState<SrpgFeature | null>(null)

  const flip = (f: SrpgFeature, on: boolean) => {
    if (on) edit((d) => switchOnFeature(d, f))
    else if (featureCount(doc, f) > 0) setAsking(f)
    else edit((d) => switchOffFeature(d, f))
  }
  return (
    <Card title={T.cardTitle} help={T.cardHelp}>
      <div className="features" data-testid="srpg-switches">
        {SRPG_FEATURES.map((f) => (
          <div className="feature" key={f}>
            <Toggle learn={`srpg-${f}`} label={T[f]} help={!srpgFeatureOn(doc, f) && !canSwitchOn(doc, f) ? T.supportsNeedCharacters : T[`${f}Help`]}
              checked={srpgFeatureOn(doc, f)} disabled={!srpgFeatureOn(doc, f) && !canSwitchOn(doc, f)} onChange={(v) => flip(f, v)} />
          </div>
        ))}
      </div>
      <ConfirmDialog open={asking !== null} title={asking ? T.offTitle(T.what[asking]) : ''} confirmLabel={T.offConfirm} danger
        onCancel={() => setAsking(null)} onConfirm={() => { if (asking) edit((d) => switchOffFeature(d, asking)); setAsking(null) }}>
        {asking && <p>{T.offBody(T.what[asking], featureCount(doc, asking))}</p>}
      </ConfirmDialog>
    </Card>
  )
}
