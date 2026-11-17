// A staff that unlocks (docs/SRPG.md section 10): exactly one `{ "unlock": true | ["door"] | ["chest"] }` effect. Like a light staff it picks a square, not a
// unit, so it has no target, area or "only if"; its reach, chance of missing, experience, rank and lock work as for every staff.
import { patchItem, STAFF_RANGE_MAX } from './staff'
import { putFx, unknownEffectCount } from './staffEffects'
import { fxIndex, fxList, isUnlockStaff } from './staffFx'
import type { Extra, Item, ProjectDoc } from './types'

export type StaffUnlockKinds = 'both' | 'door' | 'chest'

export function unlockStaffOf(item: Item): { on: boolean; kinds: StaffUnlockKinds; editable: boolean } {
  const at = fxIndex(item, ['unlock'])
  if (at < 0) return { on: false, kinds: 'both', editable: true }
  const v = (fxList(item)[at] as Extra).unlock
  if (v === true) return { on: true, kinds: 'both', editable: true }
  if (Array.isArray(v) && v.length === 1 && (v[0] === 'door' || v[0] === 'chest')) return { on: true, kinds: v[0], editable: true }
  if (Array.isArray(v) && v.length === 2 && v.includes('door') && v.includes('chest')) return { on: true, kinds: 'both', editable: true }
  return { on: true, kinds: 'both', editable: false }
}

const valueOf = (k: StaffUnlockKinds): true | string[] => (k === 'both' ? true : [k])

/** Why unlocking cannot be switched on now: the staff has an effect the form cannot show (it would be lost), or '' when it can. */
export const unlockBlock = (item: Item): '' | 'unknown' => (unknownEffectCount(item) > 0 ? 'unknown' : '')

/** Switching it on replaces the other effects and removes target, area and conditions in the same step (one Undo brings them back); the reach stays, or starts at 1
 *  (next to it). Switching it off leaves a small heal so the staff stays valid. */
export function setUnlockStaffOn(doc: ProjectDoc, id: string, on: boolean): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  if (!item || isUnlockStaff(item) === on) return doc
  if (!on) return patchItem(doc, id, { effects: [{ heal: 10 }] })
  if (unlockBlock(item)) return doc
  return patchItem(doc, id, { effects: [{ unlock: true }], target: undefined, area: undefined, if: undefined, range: item.range ?? Math.min(1, STAFF_RANGE_MAX) })
}

export function setUnlockStaffKinds(doc: ProjectDoc, id: string, kinds: StaffUnlockKinds): ProjectDoc {
  const item = doc.items?.find((x) => x.id === id)
  return item && isUnlockStaff(item) && unlockStaffOf(item).kinds !== kinds ? putFx(doc, id, ['unlock'], { unlock: valueOf(kinds) }) : doc
}
