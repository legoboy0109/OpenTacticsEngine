// What the summoned unit carries (docs/SRPG.md "Summoning"): candidate items with the summoner's level each needs, which one is picked,
// and "replace the oldest at the limit". A part of the summon card.
import { addSummonItem, MAX_SUMMON_ITEMS, patchSummonItem, removeSummonItem, setSummonPick, setSummonReplace, summonItemRows } from '../../model/summonItems'
import { summonOf } from '../../model/abilities'
import { useDoc } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import type { FormProps } from './AbilityEffectFields'
import { T } from './text'

export function SummonItems({ ability, change }: FormProps) {
  const doc = useDoc()
  const s = summonOf(ability)
  const rows = summonItemRows(ability)
  const items = (doc.items ?? []).map((i) => ({ value: i.id, label: i.name }))
  const maxLevel = doc.game?.rules?.progression?.maxLevel ?? 20
  return (
    <div className="stack tight" data-testid="summon-items">
      <Check label={T.summonReplace} help={T.summonReplaceHelp} checked={s.replace === true} onChange={(v) => change((a) => setSummonReplace(a, v))} />
      <Field label={T.summonItems} help={items.length === 0 ? T.summonNoItems : T.summonItemsHelp}>
        <div className="stack tight">
          {rows.map((r, i) => (
            <div key={i} className="row" data-testid={`summon-item-${i + 1}`}>
              <Select aria-label={T.summonItemPick(i + 1)} value={r.item} placeholder="—" options={[...(r.item && !items.some((x) => x.value === r.item) ? [{ value: r.item, label: `${r.item} (not an item)` }] : []), ...items]}
                onChange={(v) => change((a) => patchSummonItem(a, i, { item: v }))} />
              <NumberInput aria-label={T.summonItemLevel(i + 1)} value={r.minLevel} min={1} max={maxLevel} onChange={(v) => { if (v !== undefined) change((a) => patchSummonItem(a, i, { minLevel: v }), `summonItemLevel${i}`) }} />
              <button type="button" className="btn sm" aria-label={T.summonItemRemove(i + 1)} onClick={() => change((a) => removeSummonItem(a, i))}>{T.summonItemRemoveText}</button>
            </div>
          ))}
          <button type="button" className="btn sm" disabled={rows.length >= MAX_SUMMON_ITEMS || items.length === 0} onClick={() => change((a) => addSummonItem(doc, a))}>{T.summonItemAdd}</button>
        </div>
      </Field>
      {rows.length > 0 && (
        <Field label={T.summonPick} help={T.summonPickHelp}>
          <Select value={s.pick ?? 'highest'} options={[{ value: 'highest', label: T.pickHighest }, { value: 'random', label: T.pickRandom }]} onChange={(v) => change((a) => setSummonPick(a, v as 'highest' | 'random'))} />
        </Field>
      )}
    </div>
  )
}
