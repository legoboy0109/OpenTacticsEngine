// A usable item's "It opens" part (docs/SRPG.md section 10): it makes the item a key. Shown on every usable item once the game has a door or chest kind,
// or when the item already has the key `opens`.
import { gameHasLocks, isKeyItem, opensChoices, setKeyOn, setKeyOpens } from '../../model/itemKeys'
import { useDoc, useEdit } from '../../store/context'
import { MultiSelect } from '../../ui/MultiSelect'
import { Check } from '../../ui/Toggle'
import { K } from './keyText'

export function ItemKeyPart({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item || item.kind !== 'usable') return null
  const key = isKeyItem(item)
  if (!key && !gameHasLocks(doc) && item.opens === undefined) return <p className="help" data-testid="item-key-none">{K.none}</p>
  const options = opensChoices(doc).map((o) => ({ value: o.value, label: o.label }))
  for (const v of item.opens ?? []) if (!options.some((o) => o.value === v)) options.push({ value: v, label: K.notOne(v) })
  return (
    <div className="stack" data-testid="item-key">
      <Check label={K.on} help={K.onHelp} checked={key} onChange={(v) => edit((d) => setKeyOn(d, id, v))} />
      {key && (
        <div className="indent stack">
          <MultiSelect label={K.opens} help={K.opensHelp} options={options} value={item.opens ?? []} onChange={(v) => edit((d) => setKeyOpens(d, id, v))} />
          <p className="help">{K.usesHelp}</p>
        </div>
      )}
    </div>
  )
}
