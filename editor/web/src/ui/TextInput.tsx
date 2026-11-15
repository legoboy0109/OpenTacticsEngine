import { useFieldProps } from './Field'

interface Props {
  value: string
  onChange?: (v: string) => void
  placeholder?: string
  maxLength?: number
  multiline?: boolean
  rows?: number
  mono?: boolean
  disabled?: boolean
  readOnly?: boolean
  className?: string
  'aria-label'?: string
  /** Names the control for the lessons' pointer (learn/Highlight.tsx). */
  'data-learn'?: string
  autoFocus?: boolean
  /** id of a <datalist> that suggests values. */
  list?: string
  onEnter?: () => void
}

export function TextInput({ value, onChange, multiline, rows = 3, mono, className, onEnter, ...rest }: Props) {
  const f = useFieldProps()
  const cls = `${multiline ? 'textarea' : 'input'}${mono ? ' mono' : ''} ${className ?? ''}`
  if (multiline) {
    return <textarea {...f} {...rest} className={cls} rows={rows} value={value} onChange={(e) => onChange?.(e.target.value)} />
  }
  return (
    <input {...f} {...rest} className={cls} type="text" value={value} onChange={(e) => onChange?.(e.target.value)}
      onKeyDown={onEnter ? (e) => { if (e.key === 'Enter') { e.preventDefault(); onEnter() } } : undefined}
      autoComplete="off" spellCheck={false} />
  )
}
