// React hooks for pictures: the image library state, the drawable scene of the current document, the animation clock.
import { useEffect, useMemo, useRef, useSyncExternalStore } from 'react'
import { useMediaQuery } from '../ui/hooks'
import { useDoc, useStore } from '../store/context'
import type { ArtState } from '../store/art'
import { buildArtScene, type ArtScene } from './scene'

/** The image files and a version number that changes whenever a list or picture did. */
export function useArtState(): ArtState {
  const art = useStore().art
  return useSyncExternalStore(art.subscribe, art.getState)
}

/** Everything needed to draw the art of the open game in one visual style (undefined = the base pictures). */
export function useArtScene(style?: string): ArtScene | null {
  const doc = useDoc()
  const store = useStore()
  const { version } = useArtState()
  // `version` is part of the key on purpose: a picture finishing loading must rebuild the scene.
  return useMemo(() => buildArtScene(doc, style, store.art), [doc.graphics, doc.terrain, doc.units, doc.structures, style, store, version]) // eslint-disable-line react-hooks/exhaustive-deps
}

export const useReducedMotion = (): boolean => useMediaQuery('(prefers-reduced-motion: reduce)')

/** The tab is in front. */
export function usePageVisible(): boolean {
  return useSyncExternalStore(
    (cb) => { document.addEventListener('visibilitychange', cb); return () => document.removeEventListener('visibilitychange', cb) },
    () => document.visibilityState === 'visible',
    () => true,
  )
}

/** Milliseconds shown to animated pictures: running time, or 0 (first frame only) under reduced motion. */
const T0 = typeof performance !== 'undefined' ? performance.now() : 0
export const clockNow = (reduced: boolean): number => (reduced ? 0 : Math.round(performance.now() - T0))

/** Calls `redraw` about 20 times a second while `active` (an animation is on screen, the tab is visible, motion is allowed). */
export function useAnimationClock(active: boolean, redraw: () => void): void {
  const cb = useRef(redraw)
  cb.current = redraw
  useEffect(() => {
    if (!active) return
    let raf = 0, last = -1
    const tick = () => {
      const phase = Math.floor(performance.now() / 50)
      if (phase !== last) { last = phase; cb.current() }
      raf = requestAnimationFrame(tick)
    }
    raf = requestAnimationFrame(tick)
    return () => cancelAnimationFrame(raf)
  }, [active])
}

/** True when animations should run: motion is allowed and the tab is in front. */
export function useAnimating(wanted: boolean): boolean {
  const reduced = useReducedMotion()
  const visible = usePageVisible()
  return wanted && !reduced && visible
}
