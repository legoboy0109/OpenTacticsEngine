import type { ReactNode } from 'react'

interface CardProps {
  title?: string
  help?: ReactNode
  actions?: ReactNode
  children?: ReactNode
  className?: string
  id?: string
  /** `data-learn` name for the tutorial's pointing ring. */
  learn?: string
}

/** A titled group of related controls. */
export function Card({ title, help, actions, children, className, id, learn }: CardProps) {
  return (
    <section className={`card ${className ?? ''}`} id={id} data-learn={learn} aria-labelledby={title && id ? `${id}-title` : undefined}>
      {(title || actions) && (
        <header className="card-head">
          {title && <h2 id={id ? `${id}-title` : undefined}>{title}</h2>}
          {actions && <div className="card-actions">{actions}</div>}
        </header>
      )}
      {help && <p className="help card-help">{help}</p>}
      {children && <div className="card-body">{children}</div>}
    </section>
  )
}

/** Collapsible "Advanced" area: the only place ids and raw expressions appear. */
export function Advanced({ children, label = 'Advanced', defaultOpen }: { children: ReactNode; label?: string; defaultOpen?: boolean }) {
  return (
    <details className="advanced" open={defaultOpen}>
      <summary>{label}</summary>
      <div className="advanced-body">{children}</div>
    </details>
  )
}

export function PageHeader({ title, help, actions }: { title: string; help?: ReactNode; actions?: ReactNode }) {
  return (
    <div className="page-head">
      <div>
        <h1>{title}</h1>
        {help && <p className="help">{help}</p>}
      </div>
      {actions && <div className="page-actions">{actions}</div>}
    </div>
  )
}

/** Two-column grid of fields that stacks on phones. */
export function FormGrid({ children, cols = 2 }: { children: ReactNode; cols?: 1 | 2 | 3 }) {
  return <div className={`form-grid cols-${cols}`}>{children}</div>
}
