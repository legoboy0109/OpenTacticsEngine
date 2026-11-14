import { addEntry, entryDrops, entryId, entryUses, moveEntry, patchEntryAt, removeEntryAt, setEntryItem } from '../model/itemGrants'
import type { Item, ItemEntry } from '../model/types'
import { FieldGroup } from './Field'
import { Icon } from './Icon'
import { NumberInput } from './NumberInput'
import { Select } from './Select'
import { Check } from './Toggle'

/** Wording of the item list editor (shared by the Units screen, the map's units and the map's side storage). */
export const L = {
  add: 'Add an item…',
  none: 'No items.',
  noItems: 'The game has no items yet. Create some on the Items screen.',
  item: 'Item',
  usesLeft: 'Uses left',
  usesPlaceholder: (n: number) => `${n} (full)`,
  unlimited: 'Unlimited',
  drops: 'Drops when defeated',
  full: (n: number, max: number) => `${n} of ${max} places used`,
  unknown: (id: string) => `${id} (not an item any more)`,
  remove: (n: string) => `Remove ${n}`,
  up: (n: string) => `Move ${n} up`,
  down: (n: string) => `Move ${n} down`,
}

interface Props {
  label: string
  help?: string
  entries: ItemEntry[]
  items: Item[]
  /** Called with the new list; `key` is set while typing so quick edits make one undo step. */
  onChange: (next: ItemEntry[], key?: string) => void
  /** Units can drop items when defeated; a side's storage cannot. */
  allowDrop?: boolean
  /** Most entries allowed (the inventory size); the add box is disabled when it is reached. */
  max?: number
  testId?: string
}

/** An ordered list of the game's items; each row can say how many uses are left and whether it drops. */
export function ItemListEditor({ label, help, entries, items, onChange, allowDrop, max, testId }: Props) {
  const full = max !== undefined && entries.length >= max
  const options = items.map((i) => ({ value: i.id, label: i.name }))
  const defOf = (id: string) => items.find((i) => i.id === id)
  return (
    <FieldGroup label={label} help={help}>
      <div className="item-rows" data-testid={testId}>
        {entries.length === 0 && <p className="help">{items.length === 0 ? L.noItems : L.none}</p>}
        {entries.map((e, i) => {
          const id = entryId(e)
          const def = defOf(id)
          const name = def?.name ?? id
          return (
            <div className="item-row" key={i}>
              <Select aria-label={`${L.item} ${i + 1}`} value={id}
                options={def ? options : [{ value: id, label: L.unknown(id) }, ...options]}
                onChange={(v) => onChange(setEntryItem(entries, i, v))} />
              {def?.uses !== undefined && (
                <NumberInput aria-label={`${L.usesLeft}: ${name}`} value={entryUses(e)} min={1} max={def.uses} allowEmpty
                  placeholder={L.usesPlaceholder(def.uses)} onChange={(v) => onChange(patchEntryAt(entries, i, { uses: v }), `items:uses:${i}`)} />
              )}
              {allowDrop && <Check label={L.drops} checked={entryDrops(e)} onChange={(v) => onChange(patchEntryAt(entries, i, { drop: v }))} />}
              <span className="item-row-buttons">
                <button type="button" className="btn ghost sm icon" aria-label={L.up(name)} disabled={i === 0} onClick={() => onChange(moveEntry(entries, i, i - 1))}><Icon name="up" /></button>
                <button type="button" className="btn ghost sm icon" aria-label={L.down(name)} disabled={i === entries.length - 1} onClick={() => onChange(moveEntry(entries, i, i + 1))}><Icon name="down" /></button>
                <button type="button" className="btn ghost sm icon" aria-label={L.remove(name)} onClick={() => onChange(removeEntryAt(entries, i))}><Icon name="close" /></button>
              </span>
            </div>
          )
        })}
        {items.length > 0 && (
          // Always back at the placeholder: choosing an item adds it, the box does not stay on it.
          <Select aria-label={`${label}: ${L.add}`} value="" placeholder={L.add} disabled={full}
            options={options} onChange={(v) => onChange(addEntry(entries, v))} />
        )}
        {max !== undefined && <p className="help">{L.full(entries.length, max)}</p>}
      </div>
    </FieldGroup>
  )
}
