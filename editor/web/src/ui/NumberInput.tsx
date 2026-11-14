import { useEffect, useRef, useState } from 'react'
import { useFieldProps } from './Field'

interface Props {
  value: number | undefined
  onChange: (v: number | undefined) => void
  min?: number
  max?: number
  /** Whole numbers only (default). */
  integer?: boolean
  /** Only even numbers are accepted. */
  even?: boolean
  /** An empty box means "no value" (reports undefined) instead of being refused. */
  allowEmpty?: boolean
  placeholder?: string
  suffix?: string
  disabled?: boolean
  'aria-label'?: string
  className?: string
}

const fmt = (v: number | undefined) => (v === undefined || Number.isNaN(v) ? '' : String(v))

/** Parses what the user typed; undefined for empty, null when it is not a number. */
export function parseNumber(text: string, integer: boolean): number | undefined | null {
  const t = text.trim()
  if (t === '') return undefined
  const re = integer ? /^-?\d+$/ : /^-?(\d+\.?\d*|\.\d+)$/
  return re.test(t) ? Number(t) : null
}

/** A text box that only ever reports real numbers (never NaN). Half-typed
 *  input ("-", "1.") is allowed while typing and tidied on blur. */
export function NumberInput({ value, onChange, min, max, integer = true, even, allowEmpty, placeholder, suffix, disabled, className, ...rest }: Props) {
  const f = useFieldProps()
  const [text, setText] = useState(fmt(value))
  const [problem, setProblem] = useState<string | null>(null)
  const lastEmitted = useRef(value)

  // Follow outside changes (undo, reload) but not our own echo.
  useEffect(() => {
    if (value !== lastEmitted.current) { lastEmitted.current = value; setText(fmt(value)); setProblem(null) }
  }, [value])

  const emit = (v: number | undefined) => { lastEmitted.current = v; onChange(v) }

  const onType = (t: string) => {
    setText(t)
    const n = parseNumber(t, integer)
    if (n === undefined) {
      if (allowEmpty) { setProblem(null); emit(undefined) } else setProblem('Enter a number.')
    } else if (n === null) {
      setProblem(integer ? 'Whole numbers only.' : 'Enter a number.')
    } else if (min !== undefined && n < min) setProblem(`At least ${min}.`)
    else if (max !== undefined && n > max) setProblem(`At most ${max}.`)
    else if (even && n % 2 !== 0) setProblem('Use an even number.')
    else { setProblem(null); emit(n) }
  }

  const onBlur = () => { setText(fmt(lastEmitted.current)); setProblem(null) }
  return (
    <div className="input-wrap">
      <input {...f} {...rest} className={`input num ${className ?? ''}`} type="text" inputMode={integer ? 'numeric' : 'decimal'}
        value={text} placeholder={placeholder} disabled={disabled} autoComplete="off"
        aria-invalid={problem ? true : f['aria-invalid']}
        onChange={(e) => onType(e.target.value)} onBlur={onBlur} />
      {suffix && <span className="suffix">{suffix}</span>}
      {problem && <span className="error-text" role="alert">{problem}</span>}
    </div>
  )
}
