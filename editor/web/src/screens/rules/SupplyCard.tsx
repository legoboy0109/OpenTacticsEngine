// "Repair and supply" on the Rules screen: rules.supply in game.json (docs/ECONOMY.md).
import {
  REPAIR_AMOUNT_MAX, REPAIR_AMOUNT_MIN, repairAmount, repairOn, repairShare, setRepairAmount, setRepairOn, setRepairShare,
  setStructuresResupply, structuresResupply,
} from '../../model/supplyRules'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check, Toggle } from '../../ui/Toggle'
import { EconomyChecks } from '../research/EconomyChecks'
import { T } from './text'

export function SupplyCard() {
  const doc = useDoc()
  const edit = useEdit()
  const on = repairOn(doc)
  return (
    <Card title={T.supply} help={T.supplyHelp}>
      <div className="stack">
        <EconomyChecks section="rules" />
        <Toggle label={T.repair} help={T.repairHelp} checked={on} onChange={(v) => edit((d) => setRepairOn(d, v))} />
        {on && (
          <div className="feature-body stack">
            <div className="form-grid cols-2">
              <Field label={T.repairAmount} help={T.repairAmountHelp}>
                <NumberInput value={repairAmount(doc)} min={REPAIR_AMOUNT_MIN} max={REPAIR_AMOUNT_MAX} suffix="%"
                  onChange={(v) => { if (v !== undefined) edit((d) => setRepairAmount(d, v), 'rules:repairAmount') }} />
              </Field>
              <Field label={T.repairShare} help={T.repairShareHelp}>
                <NumberInput value={repairShare(doc)} min={0} max={100} suffix="%"
                  onChange={(v) => { if (v !== undefined) edit((d) => setRepairShare(d, v), 'rules:repairShare') }} />
              </Field>
            </div>
          </div>
        )}
        <Check label={T.supplyStructures} help={T.supplyStructuresHelp} checked={structuresResupply(doc)}
          onChange={(v) => edit((d) => setStructuresResupply(d, v))} />
      </div>
    </Card>
  )
}
