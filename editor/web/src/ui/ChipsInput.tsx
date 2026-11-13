import { useId, useState } from 'react'
import { slugify } from '../model/ids'
import { useFieldProps } from './Field'
import { Icon } from './Icon'

interface Props {
  value: string[]
  onChange: (v: string[]) => void
  placeholder?: string
  suggestions?: string[]
  'aria-label'?: string
}

/** Free-form labels: Enter or comma adds one, Backspace on an empty box removes the last. */
export function ChipsInput({ value, onChange, placeholder = 'Type a label and press Enter', suggestions = [], ...rest }: Props) {
  const f = useFieldProps()
  const [text, setText] = useState('')
  const listId = useId()

  const add = (raw: string) => {
    const tag = slugify(raw, { fallback: '' })
    if (tag && !value.includes(tag)) onChange([...value, tag])
    setText('')
  }
  return (
    <div className="chips">
      {value.map((t) => (
        <span className="chip" key={t}>
          {t}
          <button type="button" aria-label={`Remove ${t}`} onClick={() => onChange(value.filter((x) => x !== t))}><Icon name="close" /></button>
        </span>
      ))}
      <input {...f} aria-label={rest['aria-label']} list={suggestions.length ? listId : undefined} value={text} placeholder={value.length ? '' : placeholder}
        onChange={(e) => (e.target.value.endsWith(',') ? add(e.target.value.slice(0, -1)) : setText(e.target.value))}
        onKeyDown={(e) => {
          if (e.key === 'Enter') { e.preventDefault(); add(text) }
          else if (e.key === 'Backspace' && text === '' && value.length) onChange(value.slice(0, -1))
        }}
        onBlur={() => text && add(text)} autoComplete="off" />
      {suggestions.length > 0 && <datalist id={listId}>{suggestions.filter((s) => !value.includes(s)).map((s) => <option key={s} value={s} />)}</datalist>}
    </div>
  )
}
