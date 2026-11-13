import { useEffect, useState } from 'react'
import { api, type FontInfo } from '../../api/client'

let cache: Promise<FontInfo[]> | null = null

/** Built-in fonts from GET /api/fonts, fetched once per page load. */
export function useFonts(): FontInfo[] {
  const [fonts, setFonts] = useState<FontInfo[]>([])
  useEffect(() => {
    cache ??= api.listFonts().then((r) => (r.ok ? r.value : []))
    let live = true
    void cache.then((f) => { if (live) setFonts(f) })
    // A failed fetch must not stick for the whole session.
    void cache.then((f) => { if (f.length === 0) cache = null })
    return () => { live = false }
  }, [])
  return fonts
}
