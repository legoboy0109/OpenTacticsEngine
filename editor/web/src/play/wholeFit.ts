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

/** What the Play panel sets, as inline styles (undefined = the style sheet's own sizes). */
export interface PlayLayout { dialog?: CSSProperties; stage?: CSSProperties; frame?: CSSProperties }

/** The dialog is at least this wide (CSS px), so its header controls have room; a frame narrower than that is centred on the dialog's own
 *  background. */
export const MIN_DIALOG_WIDTH = 600
/** Space kept free around the dialog (CSS px, each side). */
export const OUTER_MARGIN = 16
/** Below this window width the dialog is the style sheet's full-screen phone layout: only the frame is fitted, in the space it has. */
export const PHONE_WIDTH = 720

export interface Chrome { borderW: number; borderH: number; head: number; foot: number }

/** The largest whole multiple of the view that fits the window minus the margins and the chrome (0 = not even 1x). */
export function maxMultiple(view: { width: number; height: number }, win: { w: number; h: number }, chrome: Chrome, dpr: number): number {
  const availW = Math.floor((win.w - 2 * OUTER_MARGIN - chrome.borderW) * dpr + 1e-6)
  const availH = Math.floor((win.h - 2 * OUTER_MARGIN - chrome.borderH - chrome.head - chrome.foot) * dpr + 1e-6)
  return wholeFit(view, availW, availH)?.k ?? 0
}

/** The layout for a frame of k x view: the dialog is as wide as the frame (not narrower than MIN_DIALOG_WIDTH) and as tall as
 *  header + frame + footer. */
export function dialogLayout(view: { width: number; height: number }, k: number, win: { w: number }, chromeW: number, dpr: number): PlayLayout {
  const fw = cssFor(view.width * k, dpr), fh = cssFor(view.height * k, dpr)
  const stageW = Math.max(fw, Math.min(MIN_DIALOG_WIDTH, win.w - 2 * OUTER_MARGIN - chromeW))
  return {
    dialog: { width: `${stageW + chromeW}px`, height: 'fit-content', maxWidth: 'none', maxHeight: 'none' },
    stage: { flex: 'none', height: `${fh}px`, display: 'flex', justifyContent: 'center', background: 'var(--surface)' },
    frame: { position: 'static', inset: 'auto', flex: 'none', width: `${fw}px`, height: `${fh}px` },
  }
}

/** The Play panel's layout for a window (CSS px) and the measured chrome of the dialog (border, header and footer heights): the frame is
 *  the largest whole multiple of the view that fits the window minus the chrome and the margins. null when even 1x does not fit. */
export function dialogFit(view: { width: number; height: number }, win: { w: number; h: number }, chrome: Chrome, dpr: number): PlayLayout | null {
  const k = maxMultiple(view, win, chrome, dpr)
  return k >= 1 ? dialogLayout(view, k, win, chrome.borderW, dpr) : null
}

/** Lays out the Play panel (see PlayLayout): re-measured when the window or the header / footer change size (the header wraps to more
 *  rows at narrow widths: its height is measured, not assumed), and when the devicePixelRatio changes. Phone-width windows keep the
 *  full-screen dialog and only the frame is fitted into the stage. */
export function useWholeFit(
  parts: { dialog: RefObject<HTMLElement | null>; head: RefObject<HTMLElement | null>; stage: RefObject<HTMLElement | null>; foot: RefObject<HTMLElement | null> },
  view: { width: number; height: number },
): PlayLayout {
  const [layout, setLayout] = useState<PlayLayout>({})
  useEffect(() => {
    const { dialog, head, stage, foot } = parts
    if (!dialog.current || !head.current || !stage.current || !foot.current) return
    let media: MediaQueryList | null = null
    const measure = () => {
      const d = dialog.current!, dpr = window.devicePixelRatio || 1
      const win = { w: document.documentElement.clientWidth, h: document.documentElement.clientHeight }
      let next: PlayLayout
      if (win.w < PHONE_WIDTH) {
        const r = stage.current!.getBoundingClientRect()  // (+ a hair: 1000 css px at 1.25 is 1250 device pixels, not 1249.999...)
        const fit = wholeFit(view, Math.floor(r.width * dpr + 1e-6), Math.floor(r.height * dpr + 1e-6))
        next = fit ? { frame: { inset: 'auto', left: `${fit.x / dpr}px`, top: `${fit.y / dpr}px`, width: `${cssFor(fit.w, dpr)}px`, height: `${cssFor(fit.h, dpr)}px` } } : {}
      } else {
        // The header wraps to more rows in a narrower dialog, so its height depends on the frame's size: try the multiples from the biggest
        // down, each at the dialog width it would have (set for the probe only, then put back), and take the first whose dialog fits the window.
        const saved = d.style.cssText
        const borderW = d.offsetWidth - d.clientWidth, borderH = d.offsetHeight - d.clientHeight
        let k = maxMultiple(view, win, { borderW, borderH, head: 0, foot: 0 }, dpr)
        next = {}
        for (; k >= 1; k--) {
          Object.assign(d.style, dialogLayout(view, k, win, borderW, dpr).dialog)
          const need = cssFor(view.height * k, dpr) + head.current!.offsetHeight + foot.current!.offsetHeight + borderH
          if (need <= win.h - 2 * OUTER_MARGIN) { next = dialogLayout(view, k, win, borderW, dpr); break }
        }
        d.style.cssText = saved
      }
      setLayout((old) => (JSON.stringify(old) === JSON.stringify(next) ? old : next))
      watchRatio()
    }
    // A change of devicePixelRatio fires no resize event on its own in every browser: watch the media query for the current ratio.
    const watchRatio = () => {
      media?.removeEventListener('change', measure)
      media = window.matchMedia(`(resolution: ${window.devicePixelRatio || 1}dppx)`)
      media.addEventListener('change', measure)
    }
    const ro = new ResizeObserver(measure)
    ro.observe(head.current); ro.observe(foot.current); ro.observe(stage.current)
    window.addEventListener('resize', measure)
    measure()
    return () => { ro.disconnect(); window.removeEventListener('resize', measure); media?.removeEventListener('change', measure) }
  }, [parts.dialog, parts.head, parts.stage, parts.foot, view.width, view.height])
  return layout
}
