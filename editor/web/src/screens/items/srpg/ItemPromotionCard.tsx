// The "Promotion item" switch of a usable item (E3 of docs/SRPG.md). Edits `promotes` of the item; switching it off also takes the item
// out of every promotion that listed it (the loader refuses a promotion naming a non-promotion item).
import { promotionsOf, setItemPromotes } from '../../../model/promotion'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Toggle } from '../../../ui/Toggle'
import { SrpgChecks } from '../../srpg/SrpgChecks'
import { P } from '../../units/srpg/promotionText'

export function ItemPromotionCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  // Only a usable item can promote.
  if (!item || item.kind !== 'usable') return null
  const users = (doc.units ?? []).filter((u) => promotionsOf(u).some((p) => p.items?.includes(id))).map((u) => u.name)
  return (
    <Card title={P.itemTitle} id="item-promotion">
      <div className="stack" data-testid="item-promotion">
        <SrpgChecks section="items" id={id} testId="item-promotion-checks" />
        <Toggle label={P.itemSwitch} help={`${P.itemHelp} ${P.itemSelfOnly}`} checked={item.promotes === true} onChange={(v) => edit((d) => setItemPromotes(d, id, v))} />
        {item.promotes && <p className="help">{users.length ? P.itemUsedBy(users.join(', ')) : P.itemUsedByNone}</p>}
      </div>
    </Card>
  )
}
