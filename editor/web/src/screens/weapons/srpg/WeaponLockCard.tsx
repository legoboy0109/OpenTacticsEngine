// The "Only these characters can wield it" card of a weapon (docs/SRPG.md "Character-locked weapons"): a checkbox list of the game's
// characters (one column, so it works at 360 px). Shown only when weapons are carried and the game has characters; hidden, the
// weapon's list is left untouched. Names in the file that are not characters stay in the list (and the checks say so).
import { lockChecks } from '../../../model/lockChecks'
import { locksShown, setWielders, wieldersOf } from '../../../model/locks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { MultiSelect } from '../../../ui/MultiSelect'
import { R } from '../../ranks/text'
import { L } from './lockText'

export function WeaponLockCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w || !locksShown(doc)) return null
  const chars = doc.characters ?? []
  const own = lockChecks(doc).filter((c) => c.ids?.includes(id))
  const listed = wieldersOf(w)
  const options = [...chars.map((c) => ({ value: c.id, label: c.name })), ...listed.filter((x) => !chars.some((c) => c.id === x)).map((x) => ({ value: x, label: L.unknown(x) }))]
  return (
    <Card title={L.title} help={L.help} id="weapon-lock">
      <div className="stack" data-testid="weapon-lock">
        <MultiSelect label={L.who} options={options} value={listed}
          onChange={(v) => edit((d) => setWielders(d, id, v), `weapon:${id}:wieldedBy`)} />
        <p className="help" data-testid="weapon-lock-summary">{listed.length ? L.some(listed.length) : L.none}</p>
        {own.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="weapon-lock-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
