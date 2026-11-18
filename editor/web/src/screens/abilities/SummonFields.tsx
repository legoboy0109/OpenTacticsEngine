// The card for a summon's own fields: which unit type appears and how it behaves (docs/SRPG.md "Summoning").
import { LIFETIME_MAX, LIMIT_MAX, setSummon, summonOf } from '../../model/abilities'
import { useDoc } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import type { FormProps } from './AbilityEffectFields'
import { SummonItems } from './SummonItems'
import { T } from './text'

export function SummonFields({ ability, change }: FormProps) {
  const doc = useDoc()
  const s = summonOf(ability)
  const units = doc.units ?? []
  const known = units.some((u) => u.id === s.type)
  const maxLevel = doc.game?.rules?.progression?.maxLevel ?? 20
  const fixed = typeof s.level === 'number'
  const set = (p: Parameters<typeof setSummon>[1], key?: string) => change((a) => setSummon(a, p), key)

  return (
    <Card title={T.summonCard}>
      <div className="stack" data-testid="summon-fields">
        <FormGrid>
          <Field label={T.summonType} help={units.length === 0 ? T.summonNoTypes : T.summonTypeHelp}>
            <Select value={s.type ?? ''} placeholder="—" options={[...(s.type && !known ? [{ value: s.type, label: `${s.type} (not a unit type)` }] : []), ...units.map((u) => ({ value: u.id, label: u.name }))]}
              onChange={(v) => set({ type: v })} />
          </Field>
          <Field label={T.summonLevel}>
            <Select value={fixed ? 'fixed' : 'summoner'} options={[{ value: 'summoner', label: T.levelSummoner }, { value: 'fixed', label: T.levelFixed }]}
              onChange={(v) => set({ level: v === 'fixed' ? 1 : 'summoner' })} />
          </Field>
          {fixed && <Field label={T.levelNumber}><NumberInput value={s.level as number} min={1} max={maxLevel} onChange={(v) => { if (v !== undefined) set({ level: v }, 'level') }} /></Field>}
          <Field label={T.summonLimit} help={T.summonLimitHelp}>
            <NumberInput value={s.limit ?? 1} min={1} max={LIMIT_MAX} onChange={(v) => { if (v !== undefined) set({ limit: v }, 'limit') }} />
          </Field>
          <Field label={T.summonLifetime} help={T.lifeHelp}>
            <Select value={s.lifetime ? 'turns' : 'forever'} options={[{ value: 'forever', label: T.lifeForever }, { value: 'turns', label: T.lifeTurns }]}
              onChange={(v) => set({ lifetime: v === 'turns' ? 3 : 0 })} />
          </Field>
          {s.lifetime ? <Field label={T.lifeTurnsNumber}><NumberInput value={s.lifetime} min={1} max={LIFETIME_MAX} onChange={(v) => { if (v !== undefined) set({ lifetime: v }, 'lifetime') }} /></Field> : null}
        </FormGrid>
        <Check label={T.summonVanish} help={T.summonVanishHelp} checked={s.vanish !== false} onChange={(v) => set({ vanish: v })} />
        <Check label={T.summonCanAct} help={T.summonCanActHelp} checked={s.canAct === true} onChange={(v) => set({ canAct: v })} />
        <Check label={T.summonXp} help={T.summonXpHelp} checked={s.givesXp !== false} onChange={(v) => set({ givesXp: v })} />
        <SummonItems ability={ability} change={change} />
      </div>
    </Card>
  )
}
