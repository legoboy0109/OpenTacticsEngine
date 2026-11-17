// The number an effect needs: how far a push goes, how much a heal gives.
import { HEAL_MAX, PUSH_MAX, REFRESH_MAX, effectKind, refreshPerTurn, setCure, setEffectAmount, setRefreshPerTurn, setSteal, setUnlock, stealOf, unlockOf, type UnlockKinds } from '../../model/abilities'
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
    const all = ability.effect?.cure === 'all'
    const list = (Array.isArray(ability.effect?.cure) ? ability.effect?.cure : []) as string[]
    const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
    return (
      <div className="stack tight">
        <Field label={T.cureWhich}>
          <Select value={all ? 'all' : 'some'} options={[{ value: 'all', label: T.cureAll }, { value: 'some', label: T.curePick }]}
            onChange={(m) => change((a) => setCure(a, m === 'all' ? 'all' : options[0] ? [options[0].value] : 'all'))} />
        </Field>
        {!all && (options.length === 0 && list.length === 0
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
  if (kind === 'heal') {
    const full = ability.effect?.heal === 'full'
    return (
      <div className="stack tight">
        <Field label={T.healBy}>
          <NumberInput value={typeof ability.effect?.heal === 'number' ? ability.effect.heal : undefined} min={1} max={HEAL_MAX} disabled={full}
            onChange={(v) => { if (v !== undefined) change((a) => setEffectAmount(a, v), 'amount') }} />
        </Field>
        <Check label={T.healFull} checked={full} onChange={(v) => change((a) => setEffectAmount(a, v ? 'full' : 10))} />
      </div>
    )
  }
  return null
}
