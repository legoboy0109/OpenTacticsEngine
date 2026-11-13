import { useCallback, useState, useSyncExternalStore } from 'react'

export function useMediaQuery(query: string): boolean {
  return useSyncExternalStore(
    (cb) => {
      const mq = window.matchMedia(query)
      mq.addEventListener('change', cb)
      return () => mq.removeEventListener('change', cb)
    },
    () => window.matchMedia(query).matches,
    () => false,
  )
}

/** Same breakpoint the stylesheet uses for the phone layout. */
export const NARROW_QUERY = '(max-width: 719.98px)'
export const useNarrow = (): boolean => useMediaQuery(NARROW_QUERY)

/** useState that remembers its value in localStorage (per browser). Storage can be blocked: then it is just state. */
export function useStored<T>(key: string, initial: T): [T, (v: T) => void] {
  const [value, setValue] = useState<T>(() => {
    try { const s = localStorage.getItem(key); return s === null ? initial : (JSON.parse(s) as T) } catch { return initial }
  })
  const set = useCallback((v: T) => {
    setValue(v)
    try { localStorage.setItem(key, JSON.stringify(v)) } catch { /* blocked or full: keep going without it */ }
  }, [key])
  return [value, set]
}
