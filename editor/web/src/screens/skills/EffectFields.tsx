// The blanks of one effect row, by kind (docs/SRPG.md "EFFECTS"). Each change hands back the whole new effect.
import { combatStatIds, conditionStatIds, statIds } from '../../model/skillBlocks'
import { amountKind, amountNumber, amountOf, amountPercent, withAmount, type AmountKind } from '../../model/skillAmount'
import { countsTurns } from '../../model/staffConditions'
import { FLAG_EFFECTS, FOE_EFFECTS, NUMBER_RANGE, effectKindOf, type EffectKind, type When } from '../../model/skillRules'
import type { Extra, ProjectDoc } from '../../model/types'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { Num } from './Num'
import { StatRows } from './StatRows'
import { T } from './text'

interface Props { doc: ProjectDoc; effect: Extra; when: When; aura: boolean; change: (e: Extra, key?: string) => void; /** The rules of a status condition: a stat bonus may not change hit points. */ forCondition?: boolean }

const PERCENT: EffectKind[] = ['xpPercent', 'damagePercent', 'damageTakenPercent', 'drain']

export function EffectFields({ doc, effect, when, aura, change, forCondition }: Props) {
  const kind = effectKindOf(effect)
  if (!kind) return <p className="help">{T.effectKept}</p>
  const v = effect[kind]
  const foeOk = FOE_EFFECTS.includes(kind) && when === 'combat' && !aura
  const target = foeOk && (
    <Field label={T.toWho}>
      <Select value={effect.to === 'foe' ? 'foe' : 'self'} options={[{ value: 'self', label: T.toSelf }, { value: 'foe', label: T.toFoe }]}
        onChange={(x) => { const { to: _t, ...rest } = effect; change(x === 'foe' ? { ...rest, to: 'foe' } : rest) }} />
    </Field>
  )

  if (kind === 'stats' || kind === 'growths') {
    const choices = kind === 'stats' && when === 'combat' ? combatStatIds(doc) : kind === 'stats' && forCondition ? conditionStatIds(doc) : statIds(doc)
    return <>{target}<StatRows doc={doc} value={(v && typeof v === 'object' ? v : {}) as Record<string, number>} choices={choices} label={kind === 'growths' ? T.effectKinds.growths : T.effectKinds.stats}
      onChange={(m, key) => change({ ...effect, [kind]: m }, key)} /></>
  }
  if (kind === 'ability') {
    const id = String(v ?? '')
    const options = (doc.abilities ?? []).map((a) => ({ value: a.id, label: a.name }))
    if (id && !options.some((o) => o.value === id)) options.push({ value: id, label: `${id} (not an ability)` })
    return <Field label={T.abilityPick}><Select value={id} options={options} onChange={(x) => change({ ...effect, ability: x })} /></Field>
  }
  if (kind === 'heal') return <HealFields effect={effect} change={change} />
  if (kind === 'hurt') return <HurtFields effect={effect} when={when} change={change} />
  if (kind === 'immune') return <ImmuneFields doc={doc} effect={effect} change={change} />
  if (kind === 'inflict') return <InflictFields doc={doc} effect={effect} change={change} />
  if (kind === 'cannotAct' || kind === 'cannotMove' || kind === 'forbid' || kind === 'berserk') return <p className="help">{T.conditionOnlyHere}</p>
  const range = NUMBER_RANGE[kind]
  if (range) {
    return <>{target}<Field label={T.amount}><Num value={Number(v ?? 0)} min={range[0]} max={range[1]} suffix={PERCENT.includes(kind) ? '%' : undefined} onChange={(n) => change({ ...effect, [kind]: n }, kind)} /></Field></>
  }
  return FLAG_EFFECTS.includes(kind) || kind === 'pass' ? null : <p className="help">{T.effectKept}</p>
}

const AMOUNT_KINDS: AmountKind[] = ['number', 'percent', 'formula']

/** The amount of a heal / hurt: a number, a percent of the maximum, or a formula (a = the unit). */
function AmountFields({ effect, which, change, labels }: { effect: Extra; which: 'heal' | 'hurt'; change: (e: Extra, key?: string) => void; labels: { how: string; kinds: Record<AmountKind, string>; formula: string; formulaHelp: string } }) {
  const a = amountOf(effect, which)
  const set = (next: typeof a, key?: string) => change(withAmount(effect, which, next), key)
  return (
    <>
      <Field label={labels.how}>
        <Select value={a.kind} options={AMOUNT_KINDS.map((k) => ({ value: k, label: labels.kinds[k] }))} onChange={(k) => set(amountKind(a, k as AmountKind))} />
      </Field>
      {a.kind === 'formula'
        ? <Field label={labels.formula} help={labels.formulaHelp}><TextInput mono value={a.text} onChange={(v) => set({ ...a, text: v }, which)} /></Field>
        : <Field label={T.amount}><Num value={a.value} min={1} max={a.kind === 'percent' ? 100 : 1000000} suffix={a.kind === 'percent' ? '%' : undefined}
            onChange={(n) => set(a.kind === 'percent' ? amountPercent(a, n) : amountNumber(a, n), which)} /></Field>}
    </>
  )
}

function HealFields({ effect, change }: { effect: Extra; change: (e: Extra, key?: string) => void }) {
  const allies = effect.to === 'allies'
  const set = (patch: Extra, key?: string) => change({ ...effect, ...patch }, key)
  return (
    <>
      <AmountFields effect={effect} which="heal" change={change}
        labels={{ how: T.healAmount, kinds: { number: T.healHp, percent: T.healPercent, formula: T.healFormula }, formula: T.healFormulaLabel, formulaHelp: T.healFormulaHelp }} />
      <Field label={T.healTo}>
        <Select value={allies ? 'allies' : 'self'} options={[{ value: 'self', label: T.healSelf }, { value: 'allies', label: T.healAllies }]}
          onChange={(t) => { const { to: _t, within: _w, ...rest } = effect; change(t === 'allies' ? { ...rest, to: 'allies', within: 1 } : rest) }} />
      </Field>
      {allies && <Field label={T.squares}><Num value={Number(effect.within ?? 1)} min={1} max={10} onChange={(n) => set({ within: n }, 'within')} /></Field>}
    </>
  )
}

/** After a fight: the foe it fought. At the start of a turn: the enemies in reach (the default) or the unit itself. */
function HurtFields({ effect, when, change }: { effect: Extra; when: When; change: (e: Extra, key?: string) => void }) {
  const set = (patch: Extra, key?: string) => change({ ...effect, ...patch }, key)
  const turn = when === 'turnStart'
  const a = amountOf(effect, 'hurt')
  const target = effect.to === 'self' ? 'self' : 'enemies'
  return (
    <>
      {turn && (
        <Field label={T.hurtTo}>
          <Select value={target} options={[{ value: 'enemies', label: T.hurtEnemies }, { value: 'self', label: T.hurtSelf }]}
            onChange={(t) => { const { to: _t, within: _w, ...rest } = effect; change(t === 'self' ? { ...rest, to: 'self' } : rest) }} />
        </Field>
      )}
      <AmountFields effect={effect} which="hurt" change={change}
        labels={{ how: T.hurtAmount, kinds: { number: T.hurtHp, percent: T.hurtPercent, formula: T.hurtFormula }, formula: T.hurtFormulaLabel, formulaHelp: T.hurtFormulaHelp }} />
      {turn && target === 'enemies' && <Field label={T.squares}><Num value={Number(effect.within ?? 1)} min={1} max={10} onChange={(n) => set({ within: n }, 'within')} /></Field>}
      <Check label={T.hurtCanKill} help={T.hurtCanKillHelp} checked={a.canKill} onChange={(on) => change(withAmount(effect, 'hurt', { ...a, canKill: on }))} />
      {!(turn && target === 'self') && <p className="help">{turn ? T.hurtEnemies : T.hurtFoe}</p>}
    </>
  )
}

/** The conditions the unit can never get. */
function ImmuneFields({ doc, effect, change }: { doc: ProjectDoc; effect: Extra; change: (e: Extra, key?: string) => void }) {
  const list = (Array.isArray(effect.immune) ? effect.immune : []) as string[]
  const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
  if (options.length === 0 && list.length === 0) return <p className="help">{T.conditionNone}</p>
  return <MultiSelect label={T.immuneList} help={T.immuneHelp} options={options} value={list} onChange={(v) => change({ ...effect, immune: v })} />
}

/** The condition a strike puts on the unit it fought, and optionally for how many turns (counted conditions only). */
function InflictFields({ doc, effect, change }: { doc: ProjectDoc; effect: Extra; change: (e: Extra, key?: string) => void }) {
  const v = (effect.inflict && typeof effect.inflict === 'object' ? effect.inflict : {}) as Extra
  const id = String(v.condition ?? '')
  const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
  if (id && !options.some((o) => o.value === id)) options.push({ value: id, label: `${id} (not a condition)` })
  if (options.length === 0) return <p className="help">{T.conditionNone}</p>
  const counts = countsTurns(doc, id)
  const put = (next: Extra, key?: string) => change({ ...effect, inflict: next }, key)
  return (
    <>
      <Field label={T.conditionPick}>
        <Select value={id} options={options} onChange={(c) => put({ condition: c, ...(countsTurns(doc, c) && v.turns !== undefined ? { turns: v.turns } : {}) })} />
      </Field>
      {counts
        ? <Field label={T.inflictTurns} help={T.inflictTurnsHelp}><NumberInput value={typeof v.turns === 'number' ? v.turns : undefined} min={1} max={99} allowEmpty
            onChange={(n) => put({ condition: id, ...(n !== undefined ? { turns: n } : {}) }, 'turns')} /></Field>
        : id ? <p className="help">{T.inflictTurnsNo}</p> : null}
    </>
  )
}
