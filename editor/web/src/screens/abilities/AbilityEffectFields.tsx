// The number an effect needs: how far a push goes, how much a heal gives.
import { HEAL_MAX, PUSH_MAX, effectKind, setEffectAmount } from '../../model/abilities'
import type { Ability } from '../../model/srpgTypes'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export interface FormProps { ability: Ability; change: (fn: (a: Ability) => Ability, key?: string) => void }

export function AbilityEffectFields({ ability, change }: FormProps) {
  const kind = effectKind(ability)
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
