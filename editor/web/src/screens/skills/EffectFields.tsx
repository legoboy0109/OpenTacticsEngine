// The blanks of one effect row, by kind (docs/SRPG.md "EFFECTS"). Each change hands back the whole new effect.
import { combatStatIds, statIds } from '../../model/skillBlocks'
import { FLAG_EFFECTS, FOE_EFFECTS, NUMBER_RANGE, effectKindOf, type EffectKind, type When } from '../../model/skillRules'
import type { Extra, ProjectDoc } from '../../model/types'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { Num } from './Num'
import { StatRows } from './StatRows'
import { T } from './text'

interface Props { doc: ProjectDoc; effect: Extra; when: When; aura: boolean; change: (e: Extra, key?: string) => void }

const PERCENT: EffectKind[] = ['xpPercent', 'damagePercent', 'damageTakenPercent', 'drain']

export function EffectFields({ doc, effect, when, aura, change }: Props) {
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
    const choices = kind === 'stats' && when === 'combat' ? combatStatIds(doc) : statIds(doc)
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
  const range = NUMBER_RANGE[kind]
  if (range) {
    return <>{target}<Field label={T.amount}><Num value={Number(v ?? 0)} min={range[0]} max={range[1]} suffix={PERCENT.includes(kind) ? '%' : undefined} onChange={(n) => change({ ...effect, [kind]: n }, kind)} /></Field></>
  }
  return FLAG_EFFECTS.includes(kind) || kind === 'pass' ? null : <p className="help">{T.effectKept}</p>
}

function HealFields({ effect, change }: { effect: Extra; change: (e: Extra, key?: string) => void }) {
  const pct = effect.heal && typeof effect.heal === 'object'
  const amount = pct ? Number((effect.heal as Extra).percent ?? 20) : Number(effect.heal ?? 5)
  const allies = effect.to === 'allies'
  const set = (patch: Extra, key?: string) => change({ ...effect, ...patch }, key)
  return (
    <>
      <Field label={T.healAmount}>
        <Select value={pct ? 'percent' : 'hp'} options={[{ value: 'hp', label: T.healHp }, { value: 'percent', label: T.healPercent }]}
          onChange={(m) => set({ heal: m === 'percent' ? { percent: Math.min(100, amount) } : amount })} />
      </Field>
      <Field label={T.amount}>
        <Num value={amount} min={1} max={pct ? 100 : 1000000} suffix={pct ? '%' : undefined} onChange={(n) => set({ heal: pct ? { percent: n } : n }, 'heal')} />
      </Field>
      <Field label={T.healTo}>
        <Select value={allies ? 'allies' : 'self'} options={[{ value: 'self', label: T.healSelf }, { value: 'allies', label: T.healAllies }]}
          onChange={(t) => { const { to: _t, within: _w, ...rest } = effect; change(t === 'allies' ? { ...rest, to: 'allies', within: 1 } : rest) }} />
      </Field>
      {allies && <Field label={T.squares}><Num value={Number(effect.within ?? 1)} min={1} max={10} onChange={(n) => set({ within: n }, 'within')} /></Field>}
    </>
  )
}

/** The foe it fought after a fight, or the enemies in reach at the start of a turn (the default of each point is not written). */
function HurtFields({ effect, when, change }: { effect: Extra; when: When; change: (e: Extra, key?: string) => void }) {
  const set = (patch: Extra, key?: string) => change({ ...effect, ...patch }, key)
  const turn = when === 'turnStart'
  return (
    <>
      <Field label={T.amount}><Num value={Number(effect.hurt ?? 5)} min={1} max={1000000} onChange={(n) => set({ hurt: n }, 'hurt')} /></Field>
      {turn
        ? <Field label={T.squares}><Num value={Number(effect.within ?? 1)} min={1} max={10} onChange={(n) => set({ within: n }, 'within')} /></Field>
        : null}
      <p className="help">{turn ? T.hurtEnemies : T.hurtFoe}</p>
    </>
  )
}
