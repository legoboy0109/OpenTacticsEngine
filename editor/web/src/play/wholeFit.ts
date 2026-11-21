// The Play panel's frame: the player's picture is only ever shown at 1x, 2x, 3x... of the game's view (docs/GRAPHICS.md "Whole-number
// scaling"), so the iframe is made exactly the largest whole multiple of the view that fits the space the panel has, measured in DEVICE
// pixels. The frame's canvas is then exactly k * view device pixels: no border inside the frame and no fractional scaling (a fractional
// scale is what lets thin lines show between map tiles). Pure functions, tested in tests/unit/wholeFit.test.ts; the same arithmetic as
// engine/platform/common/whole_scale.h.
import { useEffect, useState, type CSSProperties, type RefObject } from 'react'

/** The engine's default view when game.json gives none. */
export const DEFAULT_VIEW = { width: 480, height: 270 }

export interface WholeFit {
  /** The whole multiple of the view (>= 1). */
  k: number
  /** The frame's size in device pixels (k * view) and its top-left corner in the space (centred, offsets floor). */
  w: number; h: number; x: number; y: number
}

/** The largest whole multiple of the view that fits `availW x availH` device pixels, centred; null when even 1x does not fit
 *  (the frame then shrinks to fit the space, as before, and the player letterboxes inside it). */
export function wholeFit(view: { width: number; height: number }, availW: number, availH: number): WholeFit | null {
  if (!(view.width >= 1) || !(view.height >= 1) || !(availW >= 1) || !(availH >= 1)) return null
  const k = Math.min(Math.floor(availW / view.width), Math.floor(availH / view.height))
  if (k < 1) return null
  const w = view.width * k, h = view.height * k
  return { k, w, h, x: Math.floor((availW - w) / 2), y: Math.floor((availH - h) / 2) }
}

/** A whole number of CSS pixels whose canvas (css size x devicePixelRatio, rounded: what the player's SDL does) is exactly `device` pixels,
 *  when one exists. A frame is laid out in whole CSS pixels (its inner size is rounded), so a fractional size would leave the canvas a
 *  pixel off, or one short: a canvas SHORT of k x view would show the picture at (k-1)x. When no whole CSS size gives exactly `device`,
 *  the next one up is taken (the canvas is then up to a pixel or two bigger; the picture stays k x, centred, with a black border that thin). */
export function cssFor(device: number, dpr: number): number {
  const ideal = device / dpr
  for (const c of [Math.round(ideal), Math.floor(ideal), Math.ceil(ideal)]) if (c >= 1 && Math.round(c * dpr) === device) return c
  return Math.ceil(ideal)
}

/** The frame's CSS box for a fit at a devicePixelRatio, so one canvas pixel is one device pixel. null = no fit (fill the space, the
 *  style sheet's default). */
export function fitStyle(fit: WholeFit | null, dpr: number): CSSProperties | undefined {
  if (!fit || !(dpr > 0)) return undefined
  return { inset: 'auto', left: `${fit.x / dpr}px`, top: `${fit.y / dpr}px`, width: `${cssFor(fit.w, dpr)}px`, height: `${cssFor(fit.h, dpr)}px` }
}

/** A game's view size from its game.json view block (any part may be missing). */
export function viewSize(view?: { width?: number; height?: number }): { width: number; height: number } {
  const ok = (n: unknown): n is number => typeof n === 'number' && Number.isFinite(n) && n >= 1
  return { width: ok(view?.width) ? Math.round(view.width) : DEFAULT_VIEW.width, height: ok(view?.height) ? Math.round(view.height) : DEFAULT_VIEW.height }
}

/** Sizes the element in `space` (the Play panel's stage) for the player frame: re-measured when the space is resized, when the window
 *  is, and when the devicePixelRatio changes (zoom, moving to another screen). Returns the frame's style (undefined until measured or
 *  when 1x does not fit). */
export function useWholeFit(space: RefObject<HTMLElement | null>, view: { width: number; height: number }): CSSProperties | undefined {
  const [style, setStyle] = useState<CSSProperties>()
  useEffect(() => {
    const el = space.current
    if (!el) return
    let media: MediaQueryList | null = null
    const measure = () => {
      const dpr = window.devicePixelRatio || 1
      const r = el.getBoundingClientRect()
      // (+ a hair: 1000 css px at 1.25 is 1250 device pixels, not 1249.999...)
      setStyle(fitStyle(wholeFit(view, Math.floor(r.width * dpr + 1e-6), Math.floor(r.height * dpr + 1e-6)), dpr))
      watchRatio()
    }
    // A change of devicePixelRatio fires no resize event on its own in every browser: watch the media query for the current ratio.
    const watchRatio = () => {
      media?.removeEventListener('change', measure)
      media = window.matchMedia(`(resolution: ${window.devicePixelRatio || 1}dppx)`)
      media.addEventListener('change', measure)
    }
    const ro = new ResizeObserver(measure)
    ro.observe(el)
    window.addEventListener('resize', measure)
    measure()
    return () => { ro.disconnect(); window.removeEventListener('resize', measure); media?.removeEventListener('change', measure) }
  }, [space, view.width, view.height])
  return style
}
