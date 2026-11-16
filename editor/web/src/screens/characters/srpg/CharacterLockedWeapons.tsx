// A read-only card on a character: the weapons locked to it (set on the Weapons screen). Nothing when there are none.
import { weaponsLockedTo } from '../../../model/locks'
import { Link, projectPath } from '../../../router'
import { useDoc, useStore } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { L } from '../../weapons/srpg/lockText'

export function CharacterLockedWeapons({ id }: { id: string }) {
  const doc = useDoc()
  const store = useStore()
  const list = weaponsLockedTo(doc, id)
  if (list.length === 0) return null
  return (
    <Card title={L.charTitle} help={L.charHelp} id="character-locked-weapons">
      <ul className="usage-list" data-testid="character-locked-weapons">
        {list.map((w) => <li key={w.id}><Link to={projectPath(store.id, 'weapons', w.id)}>{w.name}</Link></li>)}
      </ul>
    </Card>
  )
}
