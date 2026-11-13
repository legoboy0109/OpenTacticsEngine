import { useState } from 'react'
import { Dialog } from './Dialog'
import { Field } from './Field'
import { TextInput } from './TextInput'

interface Props {
  open: boolean
  title: string
  label: string
  help?: string
  confirmLabel?: string
  initial?: string
  onSubmit: (name: string) => void
  onCancel: () => void
}

/** Asks for a name (new unit, new weapon...). Enter confirms. */
export function NameDialog({ open, ...rest }: Props) {
  if (!open) return null
  return <NameForm {...rest} />
}

function NameForm({ title, label, help, confirmLabel = 'Create', initial = '', onSubmit, onCancel }: Omit<Props, 'open'>) {
  const [name, setName] = useState(initial)
  const ok = name.trim() !== ''
  const submit = () => { if (ok) onSubmit(name.trim()) }
  return (
    <Dialog open title={title} onClose={onCancel} onSubmit={submit}
      footer={(
        <>
          <button type="button" className="btn" onClick={onCancel}>Cancel</button>
          <button type="submit" className="btn primary" disabled={!ok}>{confirmLabel}</button>
        </>
      )}>
      <Field label={label} help={help}>
        <TextInput value={name} onChange={setName} />
      </Field>
    </Dialog>
  )
}
