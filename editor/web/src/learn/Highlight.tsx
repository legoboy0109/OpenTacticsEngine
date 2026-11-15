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
    const frame = () => {
      const el = findTarget(name)
      const r = el?.getBoundingClientRect()
      // Off screen counts as absent, so a ring never hangs at the edge pointing at nothing.
      const visible = !!r && r.bottom > 0 && r.right > 0 && r.top < window.innerHeight && r.left < window.innerWidth
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
