import { useId, useState, type ReactNode } from 'react'
import { slugify } from '../model/ids'
import { FieldGroup } from './Field'
import { Icon } from './Icon'
import { NumberInput } from './NumberInput'

export interface KvRow { key: string; label: string }
export interface KvColumn {
  title: string
  values: Record<string, number> | undefined
  onChange: (key: string, value: number | undefined) => void
  suffix?: string
  min?: number
  max?: number
  integer?: boolean
  placeholder?: string
}

interface Props {
  label: string
  help?: ReactNode
  rows: KvRow[]
  columns: KvColumn[]
  /** Free-form tables: rows can be removed, and new ones typed in or picked. */
  onRemove?: (key: string) => void
  addOptions?: KvRow[]
  onAdd?: (key: string) => void
  addLabel?: string
  emptyText?: string
  /** `data-learn` name for the tutorial's pointing ring. */
  learn?: string
}

/** A table of "name -> number" rows (stats, costs, weapon numbers...). */
export function KeyNumberTable({ label, help, rows, columns, onRemove, addOptions, onAdd, addLabel = 'Add a number', emptyText, learn }: Props) {
  return (
    <FieldGroup label={label} help={help} learn={learn}>
      {rows.length === 0 && emptyText && <p className="help">{emptyText}</p>}
      {rows.length > 0 && (
        <table className="kv">
          <thead>
            <tr>
              <th scope="col"><span className="sr-only">Name</span></th>
              {columns.map((c) => <th scope="col" key={c.title}>{c.title}</th>)}
              {onRemove && <th scope="col"><span className="sr-only">Remove</span></th>}
            </tr>
          </thead>
          <tbody>
            {rows.map((r) => (
              <tr key={r.key}>
                <th scope="row">{r.label}</th>
                {columns.map((c) => (
                  <td key={c.title}>
                    <NumberInput value={c.values?.[r.key]} onChange={(v) => c.onChange(r.key, v)} allowEmpty={!onRemove}
                      min={c.min} max={c.max} integer={c.integer} suffix={c.suffix} placeholder={c.placeholder}
                      aria-label={`${r.label} ${c.title}`} />
                  </td>
                ))}
                {onRemove && (
                  <td><button type="button" className="btn ghost sm icon" aria-label={`Remove ${r.label}`} onClick={() => onRemove(r.key)}><Icon name="close" /></button></td>
                )}
              </tr>
            ))}
          </tbody>
        </table>
      )}
      {onAdd && <AddRow options={addOptions ?? []} onAdd={onAdd} label={addLabel} existing={rows.map((r) => r.key)} />}
    </FieldGroup>
  )
}

function AddRow({ options, onAdd, label, existing }: { options: KvRow[]; onAdd: (key: string) => void; label: string; existing: string[] }) {
  const [text, setText] = useState('')
  const listId = useId()
  const choices = options.filter((o) => !existing.includes(o.key))
  const submit = () => {
    const t = text.trim()
    if (!t) return
    const hit = options.find((o) => o.label.toLowerCase() === t.toLowerCase() || o.key === t)
    const key = hit ? hit.key : slugify(t, { fallback: '' })
    if (key && !existing.includes(key)) onAdd(key)
    setText('')
  }
  return (
    <div className="add-row">
      <input className="input" aria-label={label} placeholder={label} list={choices.length ? listId : undefined} value={text}
        onChange={(e) => setText(e.target.value)} onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); submit() } }} autoComplete="off" />
      {choices.length > 0 && <datalist id={listId}>{choices.map((c) => <option key={c.key} value={c.label} />)}</datalist>}
      <button type="button" className="btn sm" onClick={submit} disabled={!text.trim()}><Icon name="plus" />Add</button>
    </div>
  )
}
