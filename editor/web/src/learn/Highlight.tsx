// The pointing ring: a frame drawn around the control named by `data-learn="<name>"`.
// It follows the control (scroll, resize, layout changes: it checks every frame while it is shown), is absent when the
// control is not on screen, never catches clicks, and does not animate for people who asked for less motion (CSS).
import { useEffect, useRef } from 'react'

const PAD = 4

/** The first element with this name that is actually shown. */
function findTarget(name: string): HTMLElement | null {
  const all = document.querySelectorAll<HTMLElement>(`[data-learn="${name.replace(/["\\]/g, '')}"]`)
  for (const el of all) {
    if (el.closest('.learn-panel')) continue
    const r = el.getBoundingClientRect()
    if (r.width > 0 && r.height > 0) return el
  }
  return null
}

export function Highlight({ name }: { name: string | undefined }) {
  const ring = useRef<HTMLDivElement>(null)
  useEffect(() => {
    const node = ring.current
    if (!name || !node) return
    let raf = 0
    let last = ''
    // While a modal dialog (the picture chooser) is open it sits in the browser's top layer, above every z-index, so a plain ring would
    // hide behind it. Then, and only then, the ring is a manual popover (also top layer); it is shown again whenever the set of open
    // dialogs changes, which puts it back on top of a newly opened one. Without popover support it stays an ordinary fixed box.
    const canPopover = typeof node.showPopover === 'function'
    let dialogs = 0
    const syncLayer = () => {
      if (!canPopover) return
      const n = document.querySelectorAll('dialog[open]').length
      if (n === dialogs) return
      dialogs = n
      try {
        if (node.matches(':popover-open')) node.hidePopover()
        if (n > 0) { node.setAttribute('popover', 'manual'); node.showPopover() } else node.removeAttribute('popover')
      } catch { /* not supported after all: stay a plain box */ }
    }
    // A control found below or above the window is scrolled into view once, so the ring is never pointing at nothing.
    // The layout can still be settling (the sheet's height is measured after the first frame, images load), so a control that is still
    // out of view after a scroll gets a few more tries, spaced out so a smooth scroll in progress is never interrupted.
    let scrolled: HTMLElement | null = null
    let tries = 0
    let lastTry = 0
    const frame = () => {
      syncLayer()
      const el = findTarget(name)
      const r = el?.getBoundingClientRect()
      // Off screen counts as absent, so a ring never hangs at the edge pointing at nothing.
      // On a phone the lesson is a sheet over the lower part of the window: a control hidden under it counts as off screen too (and is
      // scrolled up above it), unless it is inside a dialog, which is drawn above the sheet.
      const sheet = el && !el.closest('dialog') ? document.querySelector<HTMLElement>('.learn-panel:not([data-dock])') : null
      const bottom = sheet ? Math.min(window.innerHeight, sheet.getBoundingClientRect().top) : window.innerHeight
      const visible = !!r && r.bottom > 0 && r.right > 0 && r.top < bottom && r.left < window.innerWidth
      const now = performance.now()
      if (el && r && !visible && r.width > 0 && (scrolled !== el || (tries < 4 && now - lastTry > 900))) {
        tries = scrolled === el ? tries + 1 : 0
        scrolled = el
        lastTry = now
        el.scrollIntoView({ block: 'center', inline: 'nearest', behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth' })
      }
      const key = visible && r ? `${Math.round(r.left)},${Math.round(r.top)},${Math.round(r.width)},${Math.round(r.height)}` : ''
      if (key !== last) {
        last = key
        if (visible && r) {
          node.style.display = 'block'
          node.style.left = `${r.left - PAD}px`
          node.style.top = `${r.top - PAD}px`
          node.style.width = `${r.width + PAD * 2}px`
          node.style.height = `${r.height + PAD * 2}px`
        } else node.style.display = 'none'
      }
      raf = requestAnimationFrame(frame)
    }
    frame()
    return () => { cancelAnimationFrame(raf); node.style.display = 'none'; try { if (node.matches(':popover-open')) node.hidePopover() } catch { /* not a popover */ } node.removeAttribute('popover') }
  }, [name])
  return name ? <div ref={ring} className="learn-ring" aria-hidden="true" data-testid="learn-ring" data-target={name} style={{ display: 'none' }} /> : null
}
