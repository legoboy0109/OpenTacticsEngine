// The "Only if" card of an ability: rows of conditions, each a kind and its few blanks (model/abilityConditions.ts).
import { COND_KINDS, OPS, canJudgeTarget, changeKind, condKind, newCondition, setWho, type CondKind } from '../../model/abilityConditions'
import { conditionText } from '../../model/abilityConditions'
import { setConditions, targetOf } from '../../model/abilities'
import type { Extra } from '../../model/types'
import { moveItem } from '../../model/util'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import type { FormProps } from './AbilityEffectFields'
import { T } from './text'

const opOptions = OPS.map((o) => ({ value: o, label: T.ops[o] }))

export function AbilityConditions({ ability, change }: FormProps) {
  const list = (ability.if ?? []) as Extra[]
  const set = (next: Extra[], key?: string) => change((a) => setConditions(a, next), key)
  const patch = (i: number, c: Extra, key?: string) => set(list.map((x, j) => (j === i ? c : x)), key && `${key}:${i}`)
  // A square has no unit to judge, so "the target" is not offered there.
  const canTarget = targetOf(ability) !== 'cell'

  return (
    <Card title={T.condCard} help={T.condHelp}>
      <div className="story-list conditions" data-testid="ability-conditions">
        {list.length === 0 && <p className="help">{T.condNone}</p>}
        {list.map((c, i) => {
          const kind = condKind(c)
          const o = (kind ? c[kind] : undefined) as Extra | undefined
          const n = T.condN(i + 1)
          return (
            <div className="story-row condition" role="group" aria-label={n} key={i}>
              <div className="story-row-head">
                <Field label={T.condKind}>
                  {kind
                    ? <Select value={kind} options={COND_KINDS.map((k) => ({ value: k, label: T.condKinds[k] }))} onChange={(k) => patch(i, changeKind(c, k as CondKind))} />
                    : <p className="help">{T.condKept} ({conditionText(c)})</p>}
                </Field>
                <RowButtons index={i} count={list.length} up={`Move ${n} up`} down={`Move ${n} down`} del={`Delete ${n}`}
                  onUp={() => set(moveItem(list, i, i - 1))} onDown={() => set(moveItem(list, i, i + 1))} onDelete={() => set(list.filter((_, j) => j !== i))} />
              </div>
              {kind && canJudgeTarget(kind) && canTarget && (
                <Field label={T.condWho}>
                  <Select value={c.who === 'foe' ? 'foe' : 'self'} options={[{ value: 'self', label: T.condWhoSelf }, { value: 'foe', label: T.condWhoTarget }]}
                    onChange={(v) => patch(i, setWho(c, v === 'foe'))} />
                </Field>
              )}
              {kind === 'hp' && (
                <>
                  <Field label={T.condCompare}><Select value={String(o?.op ?? '<=')} options={opOptions} onChange={(v) => patch(i, { ...c, hp: { ...o, op: v } })} /></Field>
                  <Field label={T.condPercent}><NumberInput value={Number(o?.percent ?? 50)} min={0} max={100} suffix="%" onChange={(v) => { if (v !== undefined) patch(i, { ...c, hp: { ...o, percent: v } }, 'hp') }} /></Field>
                </>
              )}
              {kind === 'near' && (
                <>
                  <Field label={T.condNear}><Select value={c.near === 'enemy' ? 'enemy' : 'ally'} options={[{ value: 'ally', label: T.condNearAlly }, { value: 'enemy', label: T.condNearEnemy }]} onChange={(v) => patch(i, { ...c, near: v })} /></Field>
                  <Field label={T.condCompare}><Select value={String(c.op ?? '>=')} options={opOptions} onChange={(v) => patch(i, { ...c, op: v })} /></Field>
                  <Field label={T.condCount}><NumberInput value={Number(c.count ?? 1)} min={0} max={99} onChange={(v) => { if (v !== undefined) patch(i, { ...c, count: v }, 'count') }} /></Field>
                  <Field label={T.condWithin}><NumberInput value={Number(c.within ?? 1)} min={1} max={10} onChange={(v) => { if (v !== undefined) patch(i, { ...c, within: v }, 'within') }} /></Field>
                </>
              )}
              {kind === 'unit' && (
                <Field label={T.condUnit} help={T.condUnitHelp}><TextInput value={String(c.unit ?? '')} onChange={(v) => patch(i, { ...c, unit: v }, 'unit')} /></Field>
              )}
              {kind === 'carrying' && (
                <Field label={T.condCarrying}>
                  <Select value={c.carrying === false ? 'no' : 'yes'} options={[{ value: 'yes', label: T.condCarrying }, { value: 'no', label: T.condCarryingNo }]}
                    onChange={(v) => patch(i, { ...c, carrying: v === 'yes' })} />
                </Field>
              )}
              {kind === 'round' && (
                <>
                  <Field label={T.condCompare}><Select value={String(o?.op ?? '>=')} options={opOptions} onChange={(v) => patch(i, { ...c, round: { ...o, op: v } })} /></Field>
                  <Field label={T.condRoundNumber}><NumberInput value={Number(o?.value ?? 2)} min={1} max={9999} onChange={(v) => { if (v !== undefined) patch(i, { ...c, round: { ...o, value: v } }, 'round') }} /></Field>
                </>
              )}
            </div>
          )
        })}
        <Select aria-label={T.condAdd} value="" placeholder={T.condAdd} options={COND_KINDS.map((k) => ({ value: k, label: T.condKinds[k] }))}
          onChange={(k) => set([...list, newCondition(k as CondKind)])} />
      </div>
    </Card>
  )
}
