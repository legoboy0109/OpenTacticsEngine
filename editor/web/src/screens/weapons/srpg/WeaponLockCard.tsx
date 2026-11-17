// The "Who can wield it" card of a weapon or a staff (docs/SRPG.md "Character-locked weapons", section 8): a checkbox list of the game's
// classes (`wieldedByTypes`) and one of its characters (`wieldedBy`), one column each so it works at 360 px. A unit passes by either list;
// no ticks at all = no lock. The class list shows whenever weapons are carried (a staff: always), the character list when the game has
// characters; hidden, a list is left untouched. Names in the file that are not in the game stay in the list (and the checks say so).
import { classWord } from '../../../model/characters'
import { gearOf, type GearOwner } from '../../../model/gear'
import { lockChecks } from '../../../model/lockChecks'
import { locksShown, setGearWielderTypes, setGearWielders, typeLocksShown, wielderTypesOf, wieldersOf } from '../../../model/locks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { MultiSelect } from '../../../ui/MultiSelect'
import { R } from '../../ranks/text'
import { L } from './lockText'

export function WeaponLockCard({ id, owner = 'weapon' }: { id: string; owner?: GearOwner }) {
  const doc = useDoc()
  const edit = useEdit()
  const g = gearOf(doc, owner, id)
  if (!g) return null
  const showChars = locksShown(doc, owner)
  const showTypes = typeLocksShown(doc, owner)
  if (!showChars && !showTypes) return null
  const staff = owner === 'item'
  const key = `${staff ? 'item' : 'weapon'}:${id}`
  const chars = doc.characters ?? []
  const types = doc.units ?? []
  const own = lockChecks(doc).filter((c) => c.section === g.section && c.ids?.includes(id))
  const listed = wieldersOf(g.keys)
  const listedTypes = wielderTypesOf(g.keys)
  const charOptions = [...chars.map((c) => ({ value: c.id, label: c.name })), ...listed.filter((x) => !chars.some((c) => c.id === x)).map((x) => ({ value: x, label: L.unknown(x) }))]
  const typeOptions = [...types.map((u) => ({ value: u.id, label: u.name })), ...listedTypes.filter((x) => !types.some((u) => u.id === x)).map((x) => ({ value: x, label: L.unknownType(x, classWord(doc)) }))]
  return (
    <Card title={L.title} help={staff ? L.helpStaff : L.help} id={staff ? 'staff-lock' : 'weapon-lock'}>
      <div className="stack" data-testid={staff ? 'staff-lock' : 'weapon-lock'}>
        {showTypes && (
          <MultiSelect label={L.whoTypes(classWord(doc, true))} options={typeOptions} value={listedTypes}
            onChange={(v) => edit((d) => setGearWielderTypes(d, owner, id, v), `${key}:wieldedByTypes`)} />
        )}
        {showChars && (
          <MultiSelect label={L.who} options={charOptions} value={listed}
            onChange={(v) => edit((d) => setGearWielders(d, owner, id, v), `${key}:wieldedBy`)} />
        )}
        <p className="help" data-testid="weapon-lock-summary">
          {listed.length + listedTypes.length === 0 ? (staff ? L.noneStaff : L.none) : L.some(listed.length, listedTypes.length, classWord(doc, listedTypes.length !== 1))}
        </p>
        {own.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="weapon-lock-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
