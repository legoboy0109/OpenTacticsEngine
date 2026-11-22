// "Ability points" card on the Rules screen: rules.abilityPoints (docs/JOBS.md "Learning from equipment"). Offered once some item teaches
// something; an existing block always shows so it can be changed or switched off.
import { AP_MAX, AP_NAME_MAX, AP_NAME_DEFAULT, abilityPointsChecks, abilityPointsOf, setAbilityPointsOn, updateAbilityPoints } from '../../model/abilityPoints'
import { anyTeaches } from '../../model/teaches'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { AP } from './unitResourcesText'

export function AbilityPointsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const rules = abilityPointsOf(doc)
  const on = rules !== undefined
  const teaches = anyTeaches(doc)
  const problems = abilityPointsChecks(doc)
  return (
    <Card title={AP.title} help={AP.help}>
      <div className="stack" data-testid="ability-points">
        {!on && !teaches && <p className="help" data-testid="ability-points-none">{AP.none}</p>}
        {(on || teaches) && <Toggle label={AP.on} checked={on} onChange={(v) => edit((d) => setAbilityPointsOn(d, v))} />}
        {on && (
          <FormGrid>
            <Field label={AP.name} help={AP.nameHelp}>
              <TextInput value={rules.name ?? AP_NAME_DEFAULT} maxLength={AP_NAME_MAX} onChange={(v) => edit((d) => updateAbilityPoints(d, { name: v }), 'rules:apName')} />
            </Field>
            <Field label={AP.perBattle} help={AP.perBattleHelp}>
              <NumberInput value={rules.perBattle ?? 10} min={0} max={AP_MAX} onChange={(v) => { if (v !== undefined) edit((d) => updateAbilityPoints(d, { perBattle: v }), 'rules:apBattle') }} />
            </Field>
            <Field label={AP.perAction} help={AP.perActionHelp}>
              <NumberInput value={rules.perAction ?? 0} min={0} max={AP_MAX} onChange={(v) => { if (v !== undefined) edit((d) => updateAbilityPoints(d, { perAction: v }), 'rules:apAction') }} />
            </Field>
            <Field label={AP.perDefeat} help={AP.perDefeatHelp}>
              <NumberInput value={rules.perDefeat ?? 0} min={0} max={AP_MAX} onChange={(v) => { if (v !== undefined) edit((d) => updateAbilityPoints(d, { perDefeat: v }), 'rules:apDefeat') }} />
            </Field>
          </FormGrid>
        )}
        {problems.length > 0 && <Banner tone="warn" title={AP.checksTitle}><ul className="usage-list" data-testid="ability-points-checks">{problems.map((p, i) => <li key={i}>{p}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
