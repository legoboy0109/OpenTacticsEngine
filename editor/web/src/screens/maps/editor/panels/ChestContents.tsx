// What is inside a placed chest (docs/SRPG.md section 10): one to eight things, each an item (optionally with fewer uses left) or an amount of a resource.
import { MAX_CHEST_LOOT } from '../../../../model/mapPlaces'
import type { ChestLoot, ProjectDoc } from '../../../../model/types'
import { Field, FieldGroup } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { Pl } from './placeText'

interface Props { doc: ProjectDoc; value: ChestLoot[]; onChange: (list: ChestLoot[], key?: string) => void }

const isResource = (e: ChestLoot): e is { resource: string; amount: number } => typeof e === 'object' && 'resource' in e
const itemId = (e: ChestLoot): string => (typeof e === 'string' ? e : 'item' in e ? e.item : '')

export function ChestContents({ doc, value, onChange }: Props) {
  const items = doc.items ?? []
  const resources = doc.game?.resources ?? []
  const set = (i: number, e: ChestLoot, key?: string) => onChange(value.map((x, j) => (j === i ? e : x)), key)
  return (
    <FieldGroup label={Pl.contents} help={Pl.contentsHelp}>
      <div className="stack tight" data-testid="chest-contents">
        {value.length === 0 && <p className="help" data-testid="chest-empty">{Pl.contentsEmpty}</p>}
        {value.map((e, i) => {
          if (isResource(e)) {
            return (
              <div key={i} className="form-grid panel-form" data-testid="loot-resource">
                <Field label={Pl.resource(i + 1)}>
                  <Select value={e.resource} options={[...(resources.some((r) => r.id === e.resource) ? [] : [{ value: e.resource, label: e.resource }]), ...resources.map((r) => ({ value: r.id, label: r.name }))]}
                    onChange={(v) => set(i, { resource: v, amount: e.amount })} />
                </Field>
                <Field label={Pl.amount}>
                  <NumberInput value={e.amount} min={1} max={1000000000} onChange={(v) => { if (v !== undefined) set(i, { resource: e.resource, amount: v }, `loot:${i}`) }} />
                </Field>
                <button type="button" className="btn sm" aria-label={Pl.removeThing(i + 1)} onClick={() => onChange(value.filter((_, j) => j !== i))}>Remove</button>
              </div>
            )
          }
          const id = itemId(e)
          const def = items.find((x) => x.id === id)
          const uses = typeof e === 'object' && 'item' in e ? e.uses : undefined
          return (
            <div key={i} className="form-grid panel-form" data-testid="loot-item">
              <Field label={Pl.item(i + 1)}>
                <Select value={id} options={[...(def ? [] : [{ value: id, label: id }]), ...items.map((x) => ({ value: x.id, label: x.name }))]}
                  onChange={(v) => set(i, v)} />
              </Field>
              {def?.uses !== undefined && (
                <Field label={Pl.uses} help={Pl.usesHelp}>
                  <NumberInput value={uses} min={1} max={def.uses} allowEmpty placeholder={String(def.uses)}
                    onChange={(v) => set(i, v === undefined ? id : { item: id, uses: v }, `loot:${i}`)} />
                </Field>
              )}
              <button type="button" className="btn sm" aria-label={Pl.removeThing(i + 1)} onClick={() => onChange(value.filter((_, j) => j !== i))}>Remove</button>
            </div>
          )
        })}
        <div className="row">
          <button type="button" className="btn sm" disabled={items.length === 0 || value.length >= MAX_CHEST_LOOT} title={items.length === 0 ? Pl.noItems : undefined}
            onClick={() => onChange([...value, items[0].id])}>{Pl.addItem}</button>
          <button type="button" className="btn sm" disabled={resources.length === 0 || value.length >= MAX_CHEST_LOOT} title={resources.length === 0 ? Pl.noResources : undefined}
            onClick={() => onChange([...value, { resource: resources[0].id, amount: 10 }])}>{Pl.addResource}</button>
        </div>
        {value.length >= MAX_CHEST_LOOT && <p className="help">{Pl.full}</p>}
      </div>
    </FieldGroup>
  )
}
