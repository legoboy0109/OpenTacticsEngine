import { useEffect, useState } from 'react'
import { useFieldProps } from './Field'

const HEX = /^#[0-9a-fA-F]{6}$/
const withHash = (t: string) => (t.startsWith('#') ? t : `#${t}`)

interface Props {
  value: string | undefined
  onChange: (v: string) => void
  'aria-label'?: string
  /** What the picker shows when there is no valid value. */
  fallback?: string
}

/** Reads and writes "#rrggbb" only. */
export function ColorInput({ value, onChange, fallback = '#808080', ...rest }: Props) {
  const f = useFieldProps()
  const valid = value !== undefined && HEX.test(value)
  const [text, setText] = useState(value ?? '')
  useEffect(() => setText(value ?? ''), [value])
  const commit = (t: string) => {
    setText(t)
    if (HEX.test(withHash(t))) onChange(withHash(t).toLowerCase())
  }
  const label = rest['aria-label']
  return (
    <div className="color">
      <input type="color" id={f.id} aria-label={label} aria-describedby={f['aria-describedby']}
        value={valid ? value!.toLowerCase() : fallback} onChange={(e) => onChange(e.target.value)} />
      <input type="text" className="input" value={text} onChange={(e) => commit(e.target.value)}
        onBlur={() => setText(value ?? '')} aria-label={label ? `${label} (hex code)` : 'Hex code'}
        maxLength={7} spellCheck={false} aria-invalid={(text !== '' && !HEX.test(withHash(text))) || undefined} />
    </div>
  )
}
