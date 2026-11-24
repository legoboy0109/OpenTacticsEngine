// The form of one ability: name, what it does and on whom, cost and limits, conditions, and the sentence it adds up to.
import { EFFECT_KINDS, NO_EFFECT, targetsFor, effectKind, hasFixedRange, rangeOf, setAfterMove, setCooldown, setCost, setEffectKind, setRange, setTarget, setUses, targetOf, updateAbility } from '../../model/abilities'
import { setSpendsItem, spendsItemProblems } from '../../model/abilityItem'
import { abilitySummary } from '../../model/abilityText'
import { withAbilities } from '../../model/doc'
import type { Ability, AbilityTarget } from '../../model/srpgTypes'
import { updateById } from '../../model/util'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Banner } from '../../ui/Banner'
import { Check } from '../../ui/Toggle'
import { hasArea, kindAllowsArea } from '../../model/abilityArea'
import { AbilityArea } from './AbilityArea'
import { AbilityConditions } from './AbilityConditions'
import { AbilityEffectFields } from './AbilityEffectFields'
import { AbilityExtras } from './AbilityExtras'
import { AbilitySpend } from './AbilitySpend'
import { SummonFields } from './SummonFields'
import { T } from './text'

export function AbilityForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const ability = doc.abilities?.find((a) => a.id === id)
  if (!ability) return null
  const key = `ability:${id}`
  /** One edit of this ability; `k` joins quick edits (typing a number) into one undo step. */
  const change = (fn: (a: Ability) => Ability, k?: string) => edit((d) => withAbilities(d, (list) => updateById(list, id, fn)), k && `${key}:${k}`)

  const kind = effectKind(ability)
  const target = targetOf(ability)
  const [rmin, rmax] = rangeOf(ability)
  const area = hasArea(ability)
  const far = rmax > 1
  // Offer only the targets the effect accepts; the one the file has stays even when it is wrong, so the select shows it.
  const allowed = (kind ? targetsFor(kind) : (Object.keys(T.targets) as AbilityTarget[])).filter((t) => !area || (t !== 'self' && t !== 'cell'))  // an area says who in it is affected
  const targetOptions = [...new Set([...allowed, target])].map((t) => ({ value: t, label: kind === 'unlock' && t === 'cell' ? (far ? T.unlockTarget : T.unlockTargetNext) : far && T.targetsFar[t] ? T.targetsFar[t] : T.targets[t] }))
  const showRange = kind !== null && !hasFixedRange(kind) && target !== 'self'
  const strike = kind === 'strike'  // the weapon gives a strike its targets and reach, and it always uses the action

  return (
    <div className="stack" data-testid="ability-form">
      <Card>
        <div className="stack">
          <FormGrid>
            <Field label={T.name} help={T.nameHelp}>
              <TextInput value={ability.name} onChange={(v) => edit((d) => updateAbility(d, id, { name: v }), `${key}:name`)} />
            </Field>
            <Field label={T.description} help={T.descriptionHelp}>
              <TextInput value={ability.description ?? ''} onChange={(v) => edit((d) => updateAbility(d, id, { description: v === '' ? undefined : v }), `${key}:description`)} />
            </Field>
          </FormGrid>
        </div>
      </Card>

      <Card title={T.doesCard}>
        <div className="stack">
          <FormGrid>
            <Field label={T.effect} help={kind ? T.effectHelp[kind] : undefined}>
              <Select value={kind ?? ''} placeholder={kind ? undefined : '—'} options={[...EFFECT_KINDS, ...(kind === NO_EFFECT || (doc.conditions?.length ?? 0) > 0 ? [NO_EFFECT] : [])].map((k) => ({ value: k, label: T.effects[k] }))}
                onChange={(v) => change((a) => setEffectKind(doc, a, v as (typeof EFFECT_KINDS)[number]))} />
            </Field>
            {!strike && (
              <Field label={T.target} help={T.targetHelp}>
                <Select value={target} options={targetOptions} onChange={(v) => change((a) => setTarget(a, v as AbilityTarget))} />
              </Field>
            )}
          </FormGrid>
          <AbilityEffectFields ability={ability} change={change} />
          {showRange && (
            <FormGrid>
              <Field label={T.rangeFrom}><NumberInput value={rmin} min={area ? 0 : 1} max={20} onChange={(v) => { if (v !== undefined) change((a) => setRange(a, v, rangeOf(a)[1]), 'rangeMin') }} /></Field>
              <Field label={T.rangeTo} help={area ? T.areaRangeHelp : T.rangeHelp}><NumberInput value={rmax} min={area ? 0 : 1} max={20} onChange={(v) => { if (v !== undefined) change((a) => setRange(a, rangeOf(a)[0], v), 'rangeMax') }} /></Field>
            </FormGrid>
          )}
        </div>
      </Card>

      {kindAllowsArea(kind) && <AbilityArea ability={ability} change={change} />}

      {kind === 'summon' && <SummonFields ability={ability} change={change} />}

      <Card title={T.limitsCard}>
        <div className="stack">
          <FormGrid>
            <Field label={T.cost} help={T.costHelp}>
              <Select value={ability.cost === 'free' && !strike ? 'free' : 'action'} options={strike ? [{ value: 'action', label: T.costAction }] : [{ value: 'action', label: T.costAction }, { value: 'free', label: T.costFree }]}
                onChange={(v) => change((a) => setCost(a, v === 'free'))} />
            </Field>
            <Field label={T.uses} help={T.usesHelp}>
              <Select value={ability.uses === undefined ? 'none' : 'n'} options={[{ value: 'none', label: T.usesUnlimited }, { value: 'n', label: T.usesNumber }]}
                onChange={(v) => change((a) => setUses(a, v === 'none' ? undefined : 2))} />
            </Field>
            {ability.uses !== undefined && (
              <Field label={T.usesNumber}><NumberInput value={ability.uses} min={1} max={99} onChange={(v) => { if (v !== undefined) change((a) => setUses(a, v), 'uses') }} /></Field>
            )}
            <Field label={T.wait} help={T.waitHelp}>
              <Select value={ability.cooldown === undefined ? 'none' : 'n'} options={[{ value: 'none', label: T.waitNone }, { value: 'n', label: T.waitNumber }]}
                onChange={(v) => change((a) => setCooldown(a, v === 'none' ? undefined : 2))} />
            </Field>
            {ability.cooldown !== undefined && (
              <Field label={T.waitNumber}><NumberInput value={ability.cooldown} min={1} max={99} onChange={(v) => { if (v !== undefined) change((a) => setCooldown(a, v), 'cooldown') }} /></Field>
            )}
          </FormGrid>
          <Check label={T.spendsItem} help={T.spendsItemHelp} checked={ability.spendsItem === true} onChange={(v) => change((a) => setSpendsItem(a, v))} />
          {spendsItemProblems(doc, ability).map((p) => <Banner key={p} tone="warn" title={p} />)}
          <Check label={T.beforeMoving} help={T.beforeMovingHelp} checked={ability.afterMove === false} onChange={(v) => change((a) => setAfterMove(a, !v))} />
        </div>
      </Card>

      <AbilitySpend ability={ability} change={change} />

      <AbilityExtras ability={ability} change={change} />

      <AbilityConditions ability={ability} change={change} />

      <Card title={T.summaryTitle}>
        <p data-testid="ability-summary">{abilitySummary(doc, ability)}</p>
      </Card>

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={ability.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
