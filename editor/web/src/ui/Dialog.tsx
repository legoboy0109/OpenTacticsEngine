import { useEffect, useRef, type ReactNode } from 'react'

interface DialogProps {
  open: boolean
  title: string
  onClose: () => void
  children: ReactNode
  footer?: ReactNode
  /** Submit handler when the dialog content is a form. */
  onSubmit?: () => void
  /** A roomy dialog (the picture picker). */
  wide?: boolean
}

/** A modal dialog on the native <dialog> element: the rest of the page is
 *  inert (that is the focus trap), Escape closes it, and focus returns to
 *  whatever opened it. */
export function Dialog({ open, title, onClose, children, footer, onSubmit, wide }: DialogProps) {
  if (!open) return null
  return <OpenDialog title={title} onClose={onClose} footer={footer} onSubmit={onSubmit} wide={wide}>{children}</OpenDialog>
}

function OpenDialog({ title, onClose, children, footer, onSubmit, wide }: Omit<DialogProps, 'open'>) {
  const ref = useRef<HTMLDialogElement>(null)
  useEffect(() => {
    const el = ref.current!
    const opener = document.activeElement as HTMLElement | null
    if (!el.open) el.showModal()
    return () => { el.close(); opener?.focus?.() }
  }, [])
  const body = (
    <>
      <h2 className="dialog-title">{title}</h2>
      <div className="dialog-body">{children}</div>
      {footer && <div className="dialog-footer">{footer}</div>}
    </>
  )
  return (
    <dialog ref={ref} className={`dialog${wide ? ' wide' : ''}`} aria-label={title}
      onCancel={(e) => { e.preventDefault(); onClose() }}
      onMouseDown={(e) => { if (e.target === ref.current) onClose() }}>
      {onSubmit
        ? <form method="dialog" onSubmit={(e) => { e.preventDefault(); onSubmit() }}>{body}</form>
        : body}
    </dialog>
  )
}

interface ConfirmProps {
  open: boolean
  title: string
  children: ReactNode
  confirmLabel: string
  onConfirm: () => void
  onCancel: () => void
  danger?: boolean
  /** Set when the action is not allowed: only a close button shows. */
  blocked?: boolean
}

/** Cancel comes first in tab order (and gets the initial focus), so a stray Enter never confirms a delete. */
export function ConfirmDialog({ open, title, children, confirmLabel, onConfirm, onCancel, danger, blocked }: ConfirmProps) {
  return (
    <Dialog open={open} title={title} onClose={onCancel}
      footer={blocked
        ? <button type="button" className="btn primary" onClick={onCancel}>OK</button>
        : (
          <>
            <button type="button" className="btn" onClick={onCancel}>Cancel</button>
            <button type="button" className={`btn ${danger ? 'danger solid' : 'primary'}`} onClick={onConfirm}>{confirmLabel}</button>
          </>
        )}>
      {children}
    </Dialog>
  )
}
