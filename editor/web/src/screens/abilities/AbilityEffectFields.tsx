// The number an effect needs: how far a push goes, how much a heal gives.
import { HEAL_MAX, PERCENT_MAX, PUSH_MAX, REFRESH_MAX, effectKind, healKind, refreshPerTurn, setCure, setEffectAmount, setHealKind, setHealValue, setRefreshPerTurn, setSteal, setStrikeWeapon, setUnlock, stealOf, strikeWeapon, unlockOf, type HealKind, type UnlockKinds } from '../../model/abilities'
import { useDoc } from '../../store/context'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import type { Ability } from '../../model/srpgTypes'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export interface FormProps { ability: Ability; change: (fn: (a: Ability) => Ability, key?: string) => void }

export function AbilityEffectFields({ ability, change }: FormProps) {
  const kind = effectKind(ability)
  const doc = useDoc()
  if (kind === 'steal') {
    const s = stealOf(ability)
    return (
      <div className="stack tight">
        <Field label={T.stealIf} help={T.stealIfHelp}>
          <TextInput mono value={s.if ?? ''} onChange={(v) => change((a) => setSteal(a, { if: v }), 'stealIf')} />
        </Field>
        {s.if !== undefined && /\bit\b/.test(s.if) && (
          <Field label={T.stealReason} help={T.stealReasonHelp}>
            <TextInput value={s.reason ?? ''} onChange={(v) => change((a) => setSteal(a, { reason: v }), 'stealReason')} />
          </Field>
        )}
        <Check label={T.stealWeapons} help={T.stealWeaponsHelp} checked={s.weapons === true} onChange={(v) => change((a) => setSteal(a, { weapons: v }))} />
        {s.weapons === true && <Check label={T.stealEquipped} checked={s.equipped === true} onChange={(v) => change((a) => setSteal(a, { equipped: v }))} />}
      </div>
    )
  }
  if (kind === 'unlock') {
    const u = unlockOf(ability)
    const picks = (doc.items ?? []).filter((i) => i.uses !== undefined || i.id === u.needs)
    const options = [{ value: '', label: T.unlockNeedsNone }, ...picks.map((i) => ({ value: i.id, label: i.uses === undefined ? `${i.name} (unlimited uses: not usable)` : i.name }))]
    if (u.needs && !picks.some((i) => i.id === u.needs)) options.push({ value: u.needs, label: `${u.needs} (not an item of this game)` })
    return (
      <div className="stack tight" data-testid="ability-unlock">
        <Field label={T.unlockKinds}>
          <Select value={u.kinds} options={[{ value: 'both', label: T.unlockBoth }, { value: 'door', label: T.unlockDoor }, { value: 'chest', label: T.unlockChest }]}
            onChange={(v) => change((a) => setUnlock(a, { kinds: v as UnlockKinds }))} />
        </Field>
        <Field label={T.unlockNeeds} help={picks.length === 0 && !u.needs ? T.unlockNeedsNoItems : T.unlockNeedsHelp}>
          <Select value={u.needs} options={options} onChange={(v) => change((a) => setUnlock(a, { needs: v }))} />
        </Field>
      </div>
    )
  }
  if (kind === 'refresh') {
    return (
      <Field label={T.refreshPerTurn} help={T.refreshHelp}>
        <NumberInput value={refreshPerTurn(ability)} min={1} max={REFRESH_MAX} onChange={(v) => { if (v !== undefined) change((a) => setRefreshPerTurn(a, v), 'perTurn') }} />
      </Field>
    )
  }
  if (kind === 'cure') {
    const cure = ability.effect?.cure
    const whole = cure === 'all' || cure === 'harmful' || cure === 'helpful' ? cure : null  // a whole group, not a list
    const list = (Array.isArray(cure) ? cure : []) as string[]
    const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
    return (
      <div className="stack tight">
        <Field label={T.cureWhich} help={T.cureWhichHelp}>
          <Select value={whole ?? 'some'} options={[{ value: 'all', label: T.cureAll }, { value: 'harmful', label: T.cureHarmful },
            { value: 'helpful', label: T.cureHelpful }, { value: 'some', label: T.curePick }]}
            onChange={(m) => change((a) => setCure(a, m === 'some' ? (options[0] ? [options[0].value] : 'all') : (m as 'all' | 'harmful' | 'helpful')))} />
        </Field>
        {!whole && (options.length === 0 && list.length === 0
          ? <p className="help">{T.cureNone}</p>
          : <MultiSelect label={T.curePick} options={options} value={list} onChange={(v) => change((a) => setCure(a, v))} />)}
      </div>
    )
  }
  if (kind === 'push') {
    return (
      <Field label={T.pushBy} help={T.pushHelp}>
        <NumberInput value={Number(ability.effect?.push ?? 1)} min={1} max={PUSH_MAX} onChange={(v) => { if (v !== undefined) change((a) => setEffectAmount(a, v), 'amount') }} />
      </Field>
    )
  }
  if (kind === 'strike') {
    const weapons = doc.weapons ?? []
    const cur = strikeWeapon(ability)
    const options = weapons.map((w) => ({ value: w.id, label: w.name }))
    if (cur && !weapons.some((w) => w.id === cur)) options.push({ value: cur, label: `${cur} (not a weapon of this game)` })
    return (
      <div className="stack tight" data-testid="ability-strike">
        <Field label={T.strikeWeapon} help={weapons.length === 0 ? T.strikeNoWeapons : T.strikeWeaponHelp}>
          <Select value={cur} placeholder="—" options={options} onChange={(v) => change((a) => setStrikeWeapon(a, v))} />
        </Field>
        <p className="help">{T.strikeCost}</p>
      </div>
    )
  }
  if (kind === 'heal') {
    const hk = healKind(ability)
    const h = ability.effect?.heal
    return (
      <div className="stack tight" data-testid="ability-heal">
        <Field label={T.healHow}>
          <Select value={hk} options={(Object.keys(T.healKinds) as HealKind[]).map((k) => ({ value: k, label: T.healKinds[k] }))} onChange={(v) => change((a) => setHealKind(a, v as HealKind))} />
        </Field>
        {hk === 'amount' && (
          <Field label={T.healBy}>
            <NumberInput value={typeof h === 'number' ? h : undefined} min={1} max={HEAL_MAX} onChange={(v) => { if (v !== undefined) change((a) => setEffectAmount(a, v), 'amount') }} />
          </Field>
        )}
        {hk === 'percent' && (
          <Field label={T.healPercent} help={T.healPercentHelp}>
            <NumberInput value={(h as { percent: number }).percent} min={1} max={PERCENT_MAX} suffix="%" onChange={(v) => { if (v !== undefined) change((a) => setHealValue(a, v), 'percent') }} />
          </Field>
        )}
        {hk === 'formula' && (
          <Field label={T.healFormula} help={T.healFormulaHelp}>
            <TextInput mono value={String(h)} onChange={(v) => change((a) => setHealValue(a, v), 'formula')} />
          </Field>
        )}
      </div>
    )
  }
  return null
}
