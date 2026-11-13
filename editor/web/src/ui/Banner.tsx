import type { ReactNode } from 'react'
import { Icon } from './Icon'

interface Props {
  tone?: 'info' | 'warn' | 'error' | 'success'
  title?: string
  children?: ReactNode
  actions?: ReactNode
  onDismiss?: () => void
}

export function Banner({ tone = 'info', title, children, actions, onDismiss }: Props) {
  return (
    <div className={`banner banner-${tone}`} role={tone === 'error' || tone === 'warn' ? 'alert' : 'status'}>
      <Icon name={tone === 'info' || tone === 'success' ? 'info' : 'warn'} className="banner-icon" />
      <div className="banner-text">
        {title && <strong>{title}</strong>}
        {children && <div>{children}</div>}
      </div>
      {actions && <div className="banner-actions">{actions}</div>}
      {onDismiss && <button type="button" className="btn ghost sm icon" aria-label="Dismiss" onClick={onDismiss}><Icon name="close" /></button>}
    </div>
  )
}
