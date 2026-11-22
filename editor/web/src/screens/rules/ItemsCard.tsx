import { useState } from 'react'
import { INVENTORY_MAX, INVENTORY_MIN, carriedWeapons, inventorySize, itemDrops, setInventorySize, setItemDrops, setStorageMode, setWeaponsMode, storageMode, weaponItemCount, type ItemDrops } from '../../model/itemRules'
import type { StorageMode } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { SellPercentField } from './SellPercentField'
import { T } from './text'

const STORAGE: Array<{ value: StorageMode; label: string }> = [
  { value: 'off', label: T.storageOff },
  { value: 'anywhere', label: T.storageAnywhere },
  { value: 'structure', label: T.storageStructure },
  { value: 'leader', label: T.storageLeader },
]

/** Weapons fixed or carried, how many items a unit holds, and the side's shared storage. */
export function ItemsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const [asking, setAsking] = useState(false)
  const carried = carriedWeapons(doc)
  const weaponItems = weaponItemCount(doc)
  const choose = (v: string) => {
    if (v === 'fixed' && weaponItems > 0) setAsking(true)
    else edit((d) => setWeaponsMode(d, v as 'fixed' | 'carried'))
  }
  return (
    <Card title={T.items} help={T.itemsHelp}>
      <div className="stack">
        <Field label={T.itemWeapons} help={carried ? T.itemWeaponsCarriedHelp : T.itemWeaponsFixedHelp}>
          <Select value={carried ? 'carried' : 'fixed'} onChange={choose}
            options={[{ value: 'fixed', label: T.weaponsFixed }, { value: 'carried', label: T.weaponsCarried }]} />
        </Field>
        {!carried && weaponItems > 0 && <p className="attention" role="alert" data-testid="weapon-items-fixed">{T.weaponItemsFixed(weaponItems)}</p>}
        <Field label={T.inventorySize} help={T.inventorySizeHelp}>
          <NumberInput value={inventorySize(doc)} min={INVENTORY_MIN} max={INVENTORY_MAX}
            onChange={(v) => { if (v !== undefined) edit((d) => setInventorySize(d, v), 'rules:inventorySize') }} />
        </Field>
        <Field label={T.storage} help={T.storageHelp}>
          <Select value={storageMode(doc)} options={STORAGE} onChange={(v) => edit((d) => setStorageMode(d, v as StorageMode))} />
        </Field>
        <Field label={T.drops} help={T.dropsHelp}>
          <Select value={itemDrops(doc)} options={[{ value: 'marked', label: T.dropsMarked }, { value: 'last', label: T.dropsLast }]} onChange={(v) => edit((d) => setItemDrops(d, v as ItemDrops))} />
        </Field>
        <SellPercentField />
      </div>
      <ConfirmDialog open={asking} title={T.fixedAskTitle} confirmLabel={T.fixedAskConfirm} danger
        onCancel={() => setAsking(false)}
        onConfirm={() => { edit((d) => setWeaponsMode(d, 'fixed')); setAsking(false) }}>
        <p>{T.fixedAskBody(weaponItems)}</p>
        <p className="help">{T.fixedAskUndo}</p>
      </ConfirmDialog>
    </Card>
  )
}
