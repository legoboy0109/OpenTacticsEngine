// The "Only these characters can wield it" card of a weapon or a staff (docs/SRPG.md "Character-locked weapons", section 8): a checkbox list of
// the game's characters (one column, so it works at 360 px). Shown when the game has characters (and, for a weapon, carries weapons);
// hidden, the list is left untouched. Names in the file that are not characters stay in the list (and the checks say so).
import { gearOf, type GearOwner } from '../../../model/gear'
import { lockChecks } from '../../../model/lockChecks'
import { locksShown, setGearWielders, wieldersOf } from '../../../model/locks'
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
  if (!g || !locksShown(doc, owner)) return null
  const staff = owner === 'item'
  const chars = doc.characters ?? []
  const own = lockChecks(doc).filter((c) => c.section === g.section && c.ids?.includes(id))
  const listed = wieldersOf(g.keys)
  const options = [...chars.map((c) => ({ value: c.id, label: c.name })), ...listed.filter((x) => !chars.some((c) => c.id === x)).map((x) => ({ value: x, label: L.unknown(x) }))]
  return (
    <Card title={L.title} help={staff ? L.helpStaff : L.help} id={staff ? 'staff-lock' : 'weapon-lock'}>
      <div className="stack" data-testid={staff ? 'staff-lock' : 'weapon-lock'}>
        <MultiSelect label={L.who} options={options} value={listed}
          onChange={(v) => edit((d) => setGearWielders(d, owner, id, v), `${staff ? 'item' : 'weapon'}:${id}:wieldedBy`)} />
        <p className="help" data-testid="weapon-lock-summary">{listed.length ? L.some(listed.length) : staff ? L.noneStaff : L.none}</p>
        {own.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="weapon-lock-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
