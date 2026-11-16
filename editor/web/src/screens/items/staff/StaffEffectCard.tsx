// The "What it does" card of a staff (docs/SRPG.md section 8): the healing (a number, a percentage, all HP or a formula), whether it can
// miss, and the experience its user earns. Only the first effect is edited; later ones (and any a later stage adds) stay as they are.
import { STAFF_HEAL_MAX, hitExample, hitText, healExample, healOf, setHealFormula, setHealKind, setHealNumber, setHealPercent, setHit, setStaffXp, xpExample, xpText, type HealKind } from '../../../model/staff'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { Check } from '../../../ui/Toggle'
import { S } from './text'

export function StaffEffectCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'staff') return null
  const key = `item:${id}`
  const heal = healOf(item)
  const canMiss = item.hit !== undefined

  return (
    <Card title={S.healTitle} id="staff-effect">
      <div className="stack" data-testid="staff-effect">
        {!heal.editable ? <p className="help" data-testid="staff-heal-other">{S.healOther}</p> : (
          <>
            <Field label={S.heal}>
              <Select value={heal.kind} options={(Object.keys(S.healKinds) as HealKind[]).map((k) => ({ value: k, label: S.healKinds[k] }))}
                onChange={(v) => edit((d) => setHealKind(d, id, v as HealKind))} />
            </Field>
            {heal.kind === 'number' && (
              <Field label={S.healNumber}>
                <NumberInput value={heal.value} min={1} max={STAFF_HEAL_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setHealNumber(d, id, v), `${key}:heal`) }} />
              </Field>
            )}
            {heal.kind === 'percent' && (
              <Field label={S.healPercent}>
                <NumberInput value={heal.value} min={1} max={100} suffix="%" onChange={(v) => { if (v !== undefined) edit((d) => setHealPercent(d, id, v), `${key}:heal`) }} />
              </Field>
            )}
            {heal.kind === 'formula' && (
              <Field label={S.healFormula} help={S.healFormulaHelp(healExample(doc))}>
                <TextInput mono value={heal.text} data-learn="staff-heal-formula" onChange={(v) => edit((d) => setHealFormula(d, id, v), `${key}:heal`)} />
              </Field>
            )}
          </>
        )}
        {heal.more > 0 && <p className="help" data-testid="staff-more-effects">{S.healMore(heal.more)}</p>}

        <Check label={S.miss} help={S.missHelp} checked={canMiss} onChange={(on) => edit((d) => setHit(d, id, on ? '80' : undefined))} />
        {canMiss && (
          <Field label={S.hit} help={S.hitHelp(hitExample(doc))}>
            <TextInput mono value={hitText(item)} data-learn="staff-hit" onChange={(v) => edit((d) => setHit(d, id, v), `${key}:hit`)} />
          </Field>
        )}

        <Field label={S.xp} help={S.xpHelp(xpExample)}>
          <TextInput mono value={xpText(item)} placeholder={S.xpPlaceholder} onChange={(v) => edit((d) => setStaffXp(d, id, v), `${key}:xp`)} />
        </Field>
      </div>
    </Card>
  )
}
