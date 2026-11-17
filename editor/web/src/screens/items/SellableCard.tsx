import { gameHasBuyingShop, setSellable } from '../../model/sell'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Toggle } from '../../ui/Toggle'
import { S } from './sellText'

/** "Can be sold": only for an item with a price, and only when a shop buys items back (or the item already says no). */
export function SellableCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const priced = !!item.price && Object.keys(item.price).length > 0
  if (!((priced && gameHasBuyingShop(doc)) || item.sellable === false)) return null
  return (
    <Card>
      <Toggle label={S.sellable} help={S.sellableHelp} checked={item.sellable !== false} onChange={(v) => edit((d) => setSellable(d, id, v))} />
    </Card>
  )
}
