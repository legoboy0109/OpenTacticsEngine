// The "What it does" card of a staff (docs/SRPG.md section 8): the parts a maker switches on (healing, moving a unit, repairing an item,
// or being a light staff), whether it can miss, and the experience its user earns. Which parts may go together is model/staffEffects.ts;
// the choices the loader would refuse are greyed out here, with the reason beside them. Effects the editor cannot show are kept.
import { STAFF_HEAL_MAX, hitExample, hitText, healExample, setHit, setStaffXp, setStaffXpMiss, xpExample, xpMissText, xpText } from '../../../model/staff'
import { canDropEffect, healBlock, healOf, setHealFormula, setHealKind, setHealNumber, setHealOn, setHealPercent, type HealKind } from '../../../model/staffEffects'
import { isLightStaff, isUnlockStaff, keptEffectCount } from '../../../model/staffFx'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { Check } from '../../../ui/Toggle'
import { gameHasDowned, isReviveStaff, setStaffReviveOn, setStaffReviveValue, staffRevive } from '../../../model/downed'
import { D } from '../../downed/text'
import { ReviveAmount } from '../../downed/ReviveAmount'
import { StaffConditionParts } from './StaffConditionParts'
import { StaffLightPart, StaffMovePart, StaffRepairPart } from './StaffOtherParts'
import { StaffUnlockPart } from './StaffUnlockPart'
import { S } from './text'

export function StaffEffectCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'staff') return null
  const key = `item:${id}`
  const heal = healOf(item)
  const light = isLightStaff(item)
  const unlock = isUnlockStaff(item)
  const canMiss = item.hit !== undefined
  const kept = keptEffectCount(item)
  const block = healBlock(item)
  const reviving = isReviveStaff(item)
  const showRevive = reviving || (gameHasDowned(doc) && !light && !unlock)

  return (
    <Card title={S.healTitle} help={S.effectsHelp} id="staff-effect">
      <div className="stack" data-testid="staff-effect">
        {showRevive && <Check label={D.staffOn} help={D.staffOnHelp} checked={reviving} onChange={(v) => edit((d) => setStaffReviveOn(d, id, v))} />}
        {reviving ? <ReviveAmount formula value={staffRevive(item)} testId="staff-revive" onChange={(v, k) => edit((d) => setStaffReviveValue(d, id, v), k && `${key}:revive:${k}`)} /> : light ? <StaffLightPart id={id} /> : unlock ? <StaffUnlockPart id={id} /> : (
          <>
            <Check label={S.healOn} checked={heal.on} disabled={heal.on ? !canDropEffect(item) : block !== ''}
              help={!heal.on && block === 'foe' ? S.healNoFoe : !heal.on && block === 'full' ? S.fourMax : undefined}
              onChange={(on) => edit((d) => setHealOn(d, id, on))} />
            {heal.on && !heal.editable && <p className="help" data-testid="staff-heal-other">{S.healOther}</p>}
            {heal.on && heal.editable && (
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
            <StaffMovePart id={id} />
            <StaffRepairPart id={id} />
            <StaffConditionParts id={id} />
            <StaffLightPart id={id} />
            <StaffUnlockPart id={id} />
          </>
        )}
        {kept > 0 && <p className="help" data-testid="staff-more-effects">{S.keptEffects(kept)}</p>}

        {!light && (
          <>
            <Check label={S.miss} help={S.missHelp} checked={canMiss} onChange={(on) => edit((d) => setHit(d, id, on ? '80' : undefined))} />
            {canMiss && (
              <Field label={S.hit} help={S.hitHelp(hitExample(doc))}>
                <TextInput mono value={hitText(item)} data-learn="staff-hit" onChange={(v) => edit((d) => setHit(d, id, v), `${key}:hit`)} />
              </Field>
            )}
          </>
        )}

        <Field label={S.xp} help={S.xpHelp(xpExample)}>
          <TextInput mono value={xpText(item)} placeholder={S.xpPlaceholder} onChange={(v) => edit((d) => setStaffXp(d, id, v), `${key}:xp`)} />
        </Field>
        {canMiss && !light && (
          <Field label={S.xpMiss} help={S.xpMissHelp}>
            <TextInput mono value={xpMissText(item)} placeholder={S.xpMissPlaceholder} onChange={(v) => edit((d) => setStaffXpMiss(d, id, v), `${key}:xpMiss`)} />
          </Field>
        )}
      </div>
    </Card>
  )
}
