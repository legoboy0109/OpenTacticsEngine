// "It unlocks" part of a staff's "What it does" card (docs/SRPG.md section 10): doors, chests or both. A staff that unlocks does nothing else (it picks a
// square of a door or chest), so switching it on replaces the other effects; the form says so and greys it out while an effect it cannot show would be lost.
import { setUnlockStaffKinds, setUnlockStaffOn, unlockBlock, unlockStaffOf, type StaffUnlockKinds } from '../../../model/staffUnlock'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Select } from '../../../ui/Select'
import { Check } from '../../../ui/Toggle'
import { S } from './text'

export function StaffUnlockPart({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const u = unlockStaffOf(item)
  const blocked = !u.on && unlockBlock(item) !== ''
  return (
    <>
      <Check label={S.unlockOn} checked={u.on} disabled={blocked} help={blocked ? S.unlockUnknown : S.unlockOnHelp}
        onChange={(on) => edit((d) => setUnlockStaffOn(d, id, on))} />
      {u.on && u.editable && (
        <Field label={S.unlockKinds}>
          <Select value={u.kinds} options={[{ value: 'both', label: S.unlockBoth }, { value: 'door', label: S.unlockDoor }, { value: 'chest', label: S.unlockChest }]}
            onChange={(v) => edit((d) => setUnlockStaffKinds(d, id, v as StaffUnlockKinds))} />
        </Field>
      )}
      {u.on && !u.editable && <p className="help">{S.unlockOther}</p>}
    </>
  )
}
