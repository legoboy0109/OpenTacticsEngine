import { useId, type ReactNode } from 'react'

interface Props {
  checked: boolean
  onChange: (v: boolean) => void
  label: string
  help?: ReactNode
  disabled?: boolean
  /** `data-learn` name for the tutorial's pointing ring (docs/TUTORIAL.md). */
  learn?: string
}

export function Toggle({ checked, onChange, label, help, disabled }: Props) {
  const helpId = useId()
  return (
    <label className="toggle">
      <input type="checkbox" role="switch" checked={checked} disabled={disabled} aria-describedby={help ? helpId : undefined}
        onChange={(e) => onChange(e.target.checked)} />
      <span className="track" aria-hidden="true" />
      <span className="text">{label}</span>
      {help && <span className="help" id={helpId}>{help}</span>}
    </label>
  )
}

/** A plain checkbox with a label and optional help line. */
export function Check({ checked, onChange, label, help, disabled, learn }: Props) {
  const helpId = useId()
  return (
    <label className="check" data-learn={learn}>
      <input type="checkbox" checked={checked} disabled={disabled} aria-describedby={help ? helpId : undefined}
        onChange={(e) => onChange(e.target.checked)} />
      <span>{label}{help && <span className="help" id={helpId} style={{ display: 'block' }}>{help}</span>}</span>
    </label>
  )
}
