import { setItemKind, setPrice, setUnlimited, setUses, updateItem } from '../../model/items'
import { carriedWeapons } from '../../model/itemRules'
import type { ItemKind } from '../../model/types'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { SoundsGroup } from '../sound/SoundsGroup'
import { Field, FieldGroup } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { ItemKindFields } from './ItemKindFields'
import { T } from './text'

export function ItemForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const key = `item:${id}`
  const resources = doc.game?.resources ?? []
  const carried = carriedWeapons(doc)
  // The current kind stays choosable even when it is no longer allowed, so the select never shows a wrong value.
  const kinds: Array<{ value: ItemKind; label: string; disabled?: boolean }> = [
    { value: 'usable', label: T.kindUsable },
    { value: 'weapon', label: T.kindWeapon, disabled: !carried && item.kind !== 'weapon' },
    { value: 'accessory', label: T.kindAccessory },
  ]

  return (
    <div className="stack">
      <Card>
        <div className="stack">
          <FormGrid>
            <Field label={T.name} help={T.nameHelp}>
              <TextInput value={item.name} onChange={(v) => edit((d) => updateItem(d, id, { name: v }), `${key}:name`)} />
            </Field>
            <Field label={T.kind} help={T.kindHelp}>
              <Select value={item.kind} options={kinds} onChange={(v) => edit((d) => setItemKind(d, id, v as ItemKind))} />
            </Field>
          </FormGrid>
          {!carried && <p className="help" data-testid="weapon-kind-locked">{T.kindWeaponLocked} <Link to={projectPath(store.id, 'rules')}>{T.rulesLink}</Link></p>}
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput value={item.description ?? ''} onChange={(v) => edit((d) => updateItem(d, id, { description: v === '' ? undefined : v }), `${key}:description`)} />
          </Field>
        </div>
      </Card>

      <Card>
        <ItemKindFields id={id} />
      </Card>

      {item.kind !== 'accessory' && (
        <Card>
          <div className="stack">
            <Toggle label={T.unlimited} help={T.unlimitedHelp} checked={item.uses === undefined} onChange={(v) => edit((d) => setUnlimited(d, id, v))} />
            {item.uses !== undefined && (
              <Field label={T.usesCount} help={T.usesHelp}>
                <NumberInput value={item.uses} min={1} max={999} onChange={(v) => { if (v !== undefined) edit((d) => setUses(d, id, v), `${key}:uses`) }} />
              </Field>
            )}
          </div>
        </Card>
      )}

      <Card>
        {resources.length === 0
          ? <FieldGroup label={T.price}><p className="help">{T.priceNone}</p></FieldGroup>
          : (
            <KeyNumberTable label={T.price} help={`${T.priceHelp}${item.price && Object.keys(item.price).length ? '' : ` ${T.notSold}.`}`}
              rows={resources.map((r) => ({ key: r.id, label: r.name }))}
              columns={[{ title: T.priceCol, values: item.price, min: 0, placeholder: '—', onChange: (k, v) => edit((d) => setPrice(d, id, k, v), `${key}:price:${k}`) }]} />
          )}
      </Card>

      <SoundsGroup owner="items" id={id} />

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={item.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
