import type { ReactNode } from 'react'
import { useFieldProps } from './Field'

export interface Option { value: string; label: string; disabled?: boolean }
export interface OptionGroup { group: string; options: Option[] }

interface Props {
  value: string
  onChange: (v: string) => void
  options: Array<Option | OptionGroup>
  disabled?: boolean
  'aria-label'?: string
  className?: string
  /** Shown as a disabled first choice when nothing is selected. */
  placeholder?: string
}

const isGroup = (o: Option | OptionGroup): o is OptionGroup => 'group' in o
const opt = (o: Option): ReactNode => <option key={o.value} value={o.value} disabled={o.disabled}>{o.label}</option>

export function Select({ value, onChange, options, placeholder, className, ...rest }: Props) {
  const f = useFieldProps()
  return (
    <select {...f} {...rest} className={`select ${className ?? ''}`} value={value} onChange={(e) => onChange(e.target.value)}>
      {placeholder !== undefined && <option value="" disabled>{placeholder}</option>}
      {options.map((o) => (isGroup(o)
        ? <optgroup key={o.group} label={o.group}>{o.options.map(opt)}</optgroup>
        : opt(o)))}
    </select>
  )
}
