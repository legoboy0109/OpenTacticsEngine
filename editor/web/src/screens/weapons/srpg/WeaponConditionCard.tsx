// "May inflict a condition on a hit" (docs/SRPG.md section 9): which condition, how often (every hit, a percent, or a formula that reads the wielder as a and the unit
// struck as b), and optional turns. Shown when the game has conditions or the weapon already has the key.
import { conditionList } from '../../../model/conditions'
import {
  setWeaponChanceFormula, setWeaponChanceKind, setWeaponChancePercent, setWeaponInflictCondition, setWeaponInflictOn, setWeaponInflictTurns, weaponChanceExample, weaponInflictOf, type ChanceKind,
} from '../../../model/conditionSites'
import { siteChecks } from '../../../model/conditionChecks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { Check } from '../../../ui/Toggle'
import { ConditionPick } from '../../conditions/ConditionPick'
import { C } from '../../conditions/siteText'

export function WeaponConditionCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w) return null
  const s = weaponInflictOf(w)
  const has = conditionList(doc).length > 0
  if (!has && !s.on) return doc.conditions === null ? null : <Card title={C.weaponTitle}><p className="help" data-testid="weapon-conditions-none">{C.none}</p></Card>
  const own = siteChecks(doc).filter((c) => c.section === 'weapons' && c.ids.includes(id))
  const key = `weapon:${id}`
  return (
    <Card title={C.weaponTitle} help={C.weaponHelp} id="weapon-inflict">
      <div className="stack" data-testid="weapon-inflict">
        {own.length > 0 && <Banner tone="error" title="Not valid yet"><ul className="usage-list">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
        <Check label={C.weaponOn} checked={s.on} disabled={!s.on && !has} onChange={(on) => edit((d) => setWeaponInflictOn(d, id, on))} />
        {s.on && (
          <>
            <ConditionPick condition={s.condition} turns={s.turns} learn="weapon-inflict-condition"
              onCondition={(c) => edit((d) => setWeaponInflictCondition(d, id, c))} onTurns={(n) => edit((d) => setWeaponInflictTurns(d, id, n), `${key}:inflictTurns`)} />
            <Field label={C.weaponChance}>
              <Select value={s.chanceKind} options={(Object.keys(C.weaponChanceKinds) as ChanceKind[]).map((k) => ({ value: k, label: C.weaponChanceKinds[k] }))}
                onChange={(k) => edit((d) => setWeaponChanceKind(d, id, k as ChanceKind))} />
            </Field>
            {s.chanceKind === 'percent' && (
              <Field label={C.weaponPercent}>
                <NumberInput value={s.percent} min={1} max={99} suffix="%" onChange={(v) => { if (v !== undefined) edit((d) => setWeaponChancePercent(d, id, v), `${key}:inflictChance`) }} />
              </Field>
            )}
            {s.chanceKind === 'formula' && (
              <Field label={C.weaponFormula} help={C.weaponFormulaHelp(weaponChanceExample(doc))}>
                <TextInput mono value={s.formula} onChange={(v) => edit((d) => setWeaponChanceFormula(d, id, v), `${key}:inflictFormula`)} />
              </Field>
            )}
          </>
        )}
      </div>
    </Card>
  )
}
