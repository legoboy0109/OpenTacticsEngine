// A tiny History-API router: "/" and "/p/:id/:section[/:itemId]".
import { useSyncExternalStore, type AnchorHTMLAttributes, type MouseEvent } from 'react'
import { SECTIONS, type Section } from './model/types'

export type Route =
  | { kind: 'picker' }
  | { kind: 'project'; id: string; section: Section; itemId?: string }
  | { kind: 'notfound' }

export function parseRoute(path: string): Route {
  const parts = path.split('/').filter(Boolean).map(decodeURIComponent)
  if (parts.length === 0) return { kind: 'picker' }
  if (parts[0] === 'p' && parts[1]) {
    const section = (parts[2] ?? 'game') as Section
    if (!SECTIONS.includes(section)) return { kind: 'notfound' }
    return { kind: 'project', id: parts[1], section, itemId: parts[3] }
  }
  return { kind: 'notfound' }
}

export const projectPath = (id: string, section: Section, itemId?: string): string =>
  `/p/${encodeURIComponent(id)}/${section}${itemId ? `/${encodeURIComponent(itemId)}` : ''}`

const EVENT = 'ote:navigate'

export function navigate(to: string, opts: { replace?: boolean } = {}): void {
  if (to === window.location.pathname) return
  if (opts.replace) window.history.replaceState(null, '', to)
  else window.history.pushState(null, '', to)
  window.dispatchEvent(new Event(EVENT))
  window.scrollTo?.(0, 0)
}

function subscribe(cb: () => void): () => void {
  window.addEventListener('popstate', cb)
  window.addEventListener(EVENT, cb)
  return () => {
    window.removeEventListener('popstate', cb)
    window.removeEventListener(EVENT, cb)
  }
}

export function useRoute(): Route {
  const path = useSyncExternalStore(subscribe, () => window.location.pathname)
  return parseRoute(path)
}

/** An ordinary link that navigates without a page load. */
export function Link({ to, onClick, ...rest }: { to: string } & AnchorHTMLAttributes<HTMLAnchorElement>) {
  const handle = (e: MouseEvent<HTMLAnchorElement>) => {
    onClick?.(e)
    if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return
    e.preventDefault()
    navigate(to)
  }
  return <a {...rest} href={to} onClick={handle} />
}
