import { useState, type ReactNode } from 'react'
import type { Named } from '../../model/types'
import { Card } from '../../ui/Card'
import { Icon } from '../../ui/Icon'
import { TextInput } from '../../ui/TextInput'

interface Props {
  title: string
  help: string
  /** Singular, lower case: "stat". */
  noun: string
  items: Named[]
  emptyText?: string
  onAdd: (name: string) => void
  onRename: (id: string, name: string) => void
  onMove: (from: number, to: number) => void
  onDelete: (id: string) => void
  /** Extra controls under the list (the health/movement pickers). */
  children?: ReactNode
}

/** An editable list of named things: rename in place, reorder, delete, add. */
export function NamedListCard({ title, help, noun, items, emptyText, onAdd, onRename, onMove, onDelete, children }: Props) {
  const [text, setText] = useState('')
  const add = () => {
    const name = text.trim()
    if (!name) return
    onAdd(name)
    setText('')
  }
  return (
    <Card title={title} help={help}>
      <div className="stack">
        {items.length === 0 && <p className="help">{emptyText ?? `No ${noun}s yet.`}</p>}
        <ul className="named-list" aria-label={title}>
          {items.map((it, i) => (
            <li key={it.id}>
              <TextInput value={it.name} aria-label={`Name of ${noun} ${i + 1}`} onChange={(v) => onRename(it.id, v)} />
              <button type="button" className="btn ghost sm icon" aria-label={`Move ${it.name} up`} disabled={i === 0} onClick={() => onMove(i, i - 1)}><Icon name="up" /></button>
              <button type="button" className="btn ghost sm icon" aria-label={`Move ${it.name} down`} disabled={i === items.length - 1} onClick={() => onMove(i, i + 1)}><Icon name="down" /></button>
              <button type="button" className="btn ghost sm icon" aria-label={`Delete ${it.name}`} onClick={() => onDelete(it.id)}><Icon name="trash" /></button>
            </li>
          ))}
        </ul>
        <div className="add-row">
          <input className="input" aria-label={`Add a ${noun}`} placeholder={`Add a ${noun}`} value={text} onChange={(e) => setText(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); add() } }} autoComplete="off" />
          <button type="button" className="btn sm" disabled={!text.trim()} onClick={add}><Icon name="plus" />Add {noun}</button>
        </div>
        {children}
      </div>
    </Card>
  )
}
