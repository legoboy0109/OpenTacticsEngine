// The pointing ring: a frame drawn around the control named by `data-learn="<name>"`.
// It follows the control (scroll, resize, layout changes: it checks every frame while it is shown), is absent when the
// control is not on screen, never catches clicks, and does not animate for people who asked for less motion (CSS).
import { useEffect, useRef } from 'react'

const PAD = 4

/** The first element with this name that is actually shown. `name` may list alternatives ("add-event|map-tab-events"):
 *  the first name that has a shown element wins, so one step can ring the tab until the maker has opened it and the
 *  button inside the tab after that. */
function findTarget(name: string): HTMLElement | null {
  for (const one of name.split('|')) {
    const all = document.querySelectorAll<HTMLElement>(`[data-learn="${one.replace(/["\\]/g, '')}"]`)
    for (const el of all) {
      if (el.closest('.learn-panel')) continue
      const r = el.getBoundingClientRect()
      if (r.width > 0 && r.height > 0) return el
    }
  }
  return null
}

/** Where the visible part of the window ends: the top of the lesson sheet while it is a bottom sheet (phones, narrow windows),
 *  else the bottom of the window. A control under the sheet counts as hidden, so the ring scrolls it up into view. */
function visibleBottom(): number {
  if (document.documentElement.dataset.learnLayout !== 'sheet') return window.innerHeight
  const sheet = document.querySelector('.learn-panel')
  return sheet ? Math.min(window.innerHeight, sheet.getBoundingClientRect().top) : window.innerHeight
}

export function Highlight({ name }: { name: string | undefined }) {
  const ring = useRef<HTMLDivElement>(null)
  useEffect(() => {
    const node = ring.current
    if (!name || !node) return
    let raf = 0
    let last = ''
    // A control found below or above the window is scrolled into view once, so the ring is never pointing at nothing.
    // A smooth scroll can be interrupted by the page settling, so it is tried again (a few times) while the control is still hidden.
    let scrolled: HTMLElement | null = null
    let tries = 0
    let at = 0
    const frame = () => {
      const el = findTarget(name)
      const r = el?.getBoundingClientRect()
      // Off screen counts as absent, so a ring never hangs at the edge pointing at nothing.
      const limit = visibleBottom()
      const visible = !!r && r.bottom > 0 && r.right > 0 && r.top < limit - 8 && r.left < window.innerWidth
      if (el && r && !visible && r.width > 0 && (scrolled !== el || (tries < 3 && performance.now() - at > 700))) {
        tries = scrolled === el ? tries + 1 : 0
        scrolled = el
        at = performance.now()
        const smooth = window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth'
        if (limit < window.innerHeight) {
          // Under a bottom sheet: put the control in the upper part of what the sheet leaves free (a margin read at the call).
          const prev = el.style.scrollMarginTop
          el.style.scrollMarginTop = `${Math.round(limit * 0.2)}px`
          el.scrollIntoView({ block: 'start', inline: 'nearest', behavior: smooth })
          el.style.scrollMarginTop = prev
        } else el.scrollIntoView({ block: 'center', inline: 'nearest', behavior: smooth })
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
    return () => { cancelAnimationFrame(raf); node.style.display = 'none' }
  }, [name])
  return name ? <div ref={ring} className="learn-ring" aria-hidden="true" data-testid="learn-ring" data-target={name} style={{ display: 'none' }} /> : null
}
