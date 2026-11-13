import { useSyncExternalStore } from 'react'

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
