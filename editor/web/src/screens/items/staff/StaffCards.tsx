// The cards of a staff, in the order the form shows them (docs/SRPG.md section 8). The rank and lock cards are the weapons' own
// (screens/weapons/srpg), the conditions are the abilities' (screens/abilities/AbilityConditions), the numbers table is the weapons' too.
import { useMemo } from 'react'
import { staffChecks } from '../../../model/staffChecks'
import { setStaffProp } from '../../../model/staff'
import { updateItem } from '../../../model/items'
import { isLightStaff } from '../../../model/staffFx'
import type { Ability } from '../../../model/srpgTypes'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { KeyNumberTable } from '../../../ui/KeyNumberTable'
import { AbilityConditions } from '../../abilities/AbilityConditions'
import { WeaponLockCard } from '../../weapons/srpg/WeaponLockCard'
import { WeaponRankCard } from '../../weapons/srpg/WeaponRankCard'
import { StaffEffectCard } from './StaffEffectCard'
import { StaffUseCard } from './StaffUseCard'
import { S } from './text'

const titleCase = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

/** The cards that come right after the name: how it is used, what it does, the numbers. */
export function StaffMainCards({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'staff') return null
  const rows = Object.keys(item.props ?? {}).map((k) => ({ key: k, label: titleCase(k) }))
  return (
    <>
      <StaffUseCard id={id} />
      <StaffEffectCard id={id} />
      <Card>
        <KeyNumberTable label={S.numbers} help={S.numbersHelp} rows={rows} emptyText={S.numbersEmpty}
          columns={[{ title: S.numberCol, values: item.props, integer: false, onChange: (k, v) => edit((d) => setStaffProp(d, id, k, v), `item:${id}:prop:${k}`) }]}
          onRemove={(k) => edit((d) => setStaffProp(d, id, k, undefined))}
          addOptions={[]} onAdd={(k) => edit((d) => setStaffProp(d, id, k, 0))} addLabel={S.addNumber} />
      </Card>
    </>
  )
}

/** After the uses and price: rank, character lock, conditions, and what is worth a look. */
export function StaffExtraCards({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  const own = useMemo(() => staffChecks(doc).filter((c) => !c.blocks && c.ids.includes(id)), [doc, id])
  if (!item || item.kind !== 'staff') return null
  // The condition editor is the abilities': it edits `if` of an ability-shaped object, so hand it the staff's `if` that way.
  const asAbility = { id: item.id, name: item.name, target: 'unit', if: item.if } as Ability
  return (
    <>
      <WeaponRankCard id={id} owner="item" />
      <WeaponLockCard id={id} owner="item" />
      {!isLightStaff(item) && (
        <AbilityConditions ability={asAbility} title={S.condTitle} help={S.condHelp} userLabel={S.condUser}
          change={(fn, key) => edit((d) => updateItem(d, id, { if: fn(asAbility).if }), key && `item:${id}:${key}`)} />
      )}
      {own.length > 0 && (
        <Banner tone="warn" title={S.worthALook}><ul className="usage-list" data-testid="staff-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>
      )}
    </>
  )
}
