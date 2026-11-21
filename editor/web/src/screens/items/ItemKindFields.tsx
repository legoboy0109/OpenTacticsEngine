// The settings that belong to one kind of item: usable, weapon or accessory.
import { setHeal, setStatNumber, setTarget, updateItem } from '../../model/items'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { ItemConditionFields } from './ItemConditionFields'
import { ItemKeyPart } from './ItemKeyPart'
import { ItemTorch } from '../fog/LightParts'
import { T } from './text'
import { STAT_LIMIT } from '../../model/dataLimits'

export function ItemKindFields({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items!.find((x) => x.id === id)!
  const key = `item:${id}`
  const statRows = (doc.game?.stats ?? []).map((s) => ({ key: s.id, label: s.name }))

  if (item.kind === 'usable') {
    const full = item.heal === 'full'
    return (
      <div className="stack">
        <Field label={T.target} help={T.targetHelp}>
          <Select value={item.target ?? 'self'} options={[{ value: 'self', label: T.targetSelf }, { value: 'ally', label: T.targetAlly }]}
            onChange={(v) => edit((d) => setTarget(d, id, v as 'self' | 'ally'))} />
        </Field>
        <Field label={T.heals} help={T.healsHelp}>
          <NumberInput data-learn="item-heal" aria-label={T.healsAmount} value={typeof item.heal === 'number' ? item.heal : undefined} min={1} max={1000000} allowEmpty disabled={full}
            onChange={(v) => edit((d) => setHeal(d, id, v), `${key}:heal`)} />
        </Field>
        <Check label={T.healsAll} checked={full} onChange={(v) => edit((d) => setHeal(d, id, v ? 'full' : undefined))} />
        <KeyNumberTable label={T.raises} help={T.raisesHelp} rows={statRows}
          columns={[{ title: T.raisesCol, values: item.raise, min: -STAT_LIMIT, max: STAT_LIMIT, placeholder: '0', onChange: (k, v) => edit((d) => setStatNumber(d, id, 'raise', k, v), `${key}:raise:${k}`) }]} />
        <ItemConditionFields id={id} />
        <ItemKeyPart id={id} />
        <ItemTorch id={id} />
      </div>
    )
  }

  if (item.kind === 'weapon') {
    const weapons = doc.weapons ?? []
    const known = weapons.some((w) => w.id === item.weapon)
    return (
      <Field label={T.weapon} help={weapons.length === 0 ? T.noWeapons : T.weaponHelp}>
        <Select value={item.weapon ?? ''} placeholder={T.weaponNone}
          options={[...(item.weapon && !known ? [{ value: item.weapon, label: item.weapon }] : []), ...weapons.map((w) => ({ value: w.id, label: w.name }))]}
          onChange={(v) => edit((d) => updateItem(d, id, { weapon: v }))} />
      </Field>
    )
  }

  return (
    <KeyNumberTable label={T.bonus} help={T.bonusHelp} rows={statRows}
      columns={[{ title: T.bonusCol, values: item.bonus, min: -STAT_LIMIT, max: STAT_LIMIT, placeholder: '0', onChange: (k, v) => edit((d) => setStatNumber(d, id, 'bonus', k, v), `${key}:bonus:${k}`) }]} />
  )
}
