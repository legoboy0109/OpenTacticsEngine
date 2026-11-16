// Which kinds of view a hexagon game may use, as the helper (and so the engine build it runs with) says: GET /api/info
// `hexViews`. The Game screen offers exactly those. Asked once; until the answer arrives, or from a helper that does
// not know the question, it is top-down only.
import { useEffect, useState } from 'react'
import { HEX_TOP_DOWN_ONLY } from '../model/styleView'
import { api } from './client'

let known: string[] | null = null
let pending: Promise<string[]> | null = null

/** Asks the helper (once); a failed request is not remembered, the next call asks again. */
export function loadHexViews(): Promise<string[]> {
  if (known) return Promise.resolve(known)
  pending ??= api.info().then((r) => {
    pending = null
    if (!r.ok) return HEX_TOP_DOWN_ONLY
    known = Array.isArray(r.value.hexViews) ? r.value.hexViews : HEX_TOP_DOWN_ONLY
    return known
  })
  return pending
}

export function useHexViews(): string[] {
  const [views, setViews] = useState<string[]>(known ?? HEX_TOP_DOWN_ONLY)
  useEffect(() => {
    let live = true
    void loadHexViews().then((v) => { if (live) setViews(v) })
    return () => { live = false }
  }, [])
  return views
}
