// "Fog of war" group on the Rules screen: rules.fog.ambush in game.json (docs/ECONOMY.md). Always shown: fog is a
// setting of each map, so the rule is worth choosing before any map uses it.
import { ambushRule, setAmbushRule } from '../../model/optionRules'
import type { AmbushRule } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { T } from './text'

export function FogRuleCard() {
  const doc = useDoc()
  const edit = useEdit()
  return (
    <Card title={T.fogGroup} help={T.fogGroupHelp}>
      <div className="stack" data-testid="fog-rule">
        <Field label={T.ambush} help={T.ambushHelp}>
          <Select value={ambushRule(doc)} onChange={(v) => edit((d) => setAmbushRule(d, v as AmbushRule))}
            options={[{ value: 'endsTurn', label: T.ambushEnds }, { value: 'canAct', label: T.ambushCanAct }]} />
        </Field>
      </div>
    </Card>
  )
}
