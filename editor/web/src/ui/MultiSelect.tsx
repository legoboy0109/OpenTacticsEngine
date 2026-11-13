import type { Option } from './Select'
import { FieldGroup } from './Field'

interface Props {
  label: string
  help?: string
  options: Option[]
  value: string[]
  onChange: (v: string[]) => void
  emptyText?: string
}

/** A checkbox list; the order of `value` follows the order of `options`. */
export function MultiSelect({ label, help, options, value, onChange, emptyText }: Props) {
  const toggle = (v: string, on: boolean) => {
    const set = new Set(value)
    if (on) set.add(v); else set.delete(v)
    // Ids the file has that are not among the options stay (at the end).
    const known = options.map((o) => o.value).filter((x) => set.has(x))
    const unknown = value.filter((x) => !options.some((o) => o.value === x) && set.has(x))
    onChange([...known, ...unknown])
  }
  return (
    <FieldGroup label={label} help={help}>
      {options.length === 0 && <p className="help">{emptyText ?? 'Nothing to choose from yet.'}</p>}
      <div className="check-list">
        {options.map((o) => (
          <label key={o.value} className="check">
            <input type="checkbox" checked={value.includes(o.value)} onChange={(e) => toggle(o.value, e.target.checked)} />
            <span>{o.label}</span>
          </label>
        ))}
      </div>
    </FieldGroup>
  )
}
