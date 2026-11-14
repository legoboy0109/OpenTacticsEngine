// A text box for a story name. It keeps what the person types while it is not a usable name (showing why) and only
// reports names that are valid, so the document never holds a half-typed name. Spaces turn into "_" as typed.
import { useEffect, useRef, useState } from 'react'
import { tidyId } from '../model/storyIds'
import { useFieldProps } from './Field'

interface Props {
  value: string
  /** Why this text cannot be the name (null = fine). */
  validate: (text: string) => string | null
  onChange: (v: string) => void
  placeholder?: string
  'aria-label'?: string
}

export function IdInput({ value, validate, onChange, placeholder, ...rest }: Props) {
  const f = useFieldProps()
  const [text, setText] = useState(value)
  const [problem, setProblem] = useState<string | null>(null)
  const last = useRef(value)
  // Follow outside changes (undo, the "fill from the name" button) but not our own echo.
  useEffect(() => {
    if (value !== last.current) { last.current = value; setText(value); setProblem(null) }
  }, [value])
  const type = (raw: string) => {
    const t = tidyId(raw)
    setText(t)
    const why = validate(t)
    setProblem(why)
    if (!why) { last.current = t; onChange(t) }
  }
  return (
    <div className="input-wrap">
      <input {...f} {...rest} className="input" type="text" value={text} placeholder={placeholder} autoComplete="off" spellCheck={false}
        aria-invalid={problem ? true : f['aria-invalid']}
        onChange={(e) => type(e.target.value)} onBlur={() => { setText(last.current); setProblem(null) }} />
      {problem && <span className="error-text" role="alert">{problem}</span>}
    </div>
  )
}
