import { createContext, useContext, useId, type ReactNode } from 'react'

interface FieldCtx { id: string; describedBy?: string; invalid: boolean }
const Ctx = createContext<FieldCtx | null>(null)

/** Props an input should spread to be wired to the surrounding Field. */
export function useFieldProps(): { id?: string; 'aria-describedby'?: string; 'aria-invalid'?: boolean } {
  const f = useContext(Ctx)
  if (!f) return {}
  return { id: f.id, 'aria-describedby': f.describedBy, 'aria-invalid': f.invalid || undefined }
}

interface Props {
  label: string
  help?: ReactNode
  error?: string | null
  children: ReactNode
  className?: string
}

/** Label + control + visible help + error. Help is always visible (phones have no hover). */
export function Field({ label, help, error, children, className }: Props) {
  const id = useId()
  const helpId = help ? `${id}-help` : undefined
  const errId = error ? `${id}-err` : undefined
  const describedBy = [helpId, errId].filter(Boolean).join(' ') || undefined
  return (
    <div className={`field ${className ?? ''}`}>
      <label htmlFor={id}>{label}</label>
      <Ctx.Provider value={{ id, describedBy, invalid: !!error }}>{children}</Ctx.Provider>
      {help && <p className="help" id={helpId}>{help}</p>}
      {error && <p className="error-text" id={errId} role="alert">{error}</p>}
    </div>
  )
}

/** Like Field but for groups of controls (a checkbox list, a table): the label is a legend. */
export function FieldGroup({ label, help, children, className, learn }: { label: string; help?: ReactNode; children: ReactNode; className?: string; learn?: string }) {
  return (
    <fieldset className={`fieldset ${className ?? ''}`} data-learn={learn}>
      <legend>{label}</legend>
      {help && <p className="help">{help}</p>}
      {children}
    </fieldset>
  )
}
