// A read-only card on a character: the weapons and staves locked to it (set on the Weapons / Items screens). Nothing when there are none.
import { gearLockedTo } from '../../../model/locks'
import { Link, projectPath } from '../../../router'
import { useDoc, useStore } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { L } from '../../weapons/srpg/lockText'

export function CharacterLockedWeapons({ id }: { id: string }) {
  const doc = useDoc()
  const store = useStore()
  const list = gearLockedTo(doc, id)
  if (list.length === 0) return null
  return (
    <Card title={L.charTitle} help={L.charHelp} id="character-locked-weapons">
      <ul className="usage-list" data-testid="character-locked-weapons">
        {list.map((g) => <li key={`${g.owner}:${g.id}`}><Link to={projectPath(store.id, g.section, g.id)}>{g.name}</Link></li>)}
      </ul>
    </Card>
  )
}
