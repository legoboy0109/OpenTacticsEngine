// A small canvas that shows a preview map (art/preview.ts). Animations play unless the system asks for reduced motion,
// and only while the tab is in front.
import { useEffect, useLayoutEffect, useRef } from 'react'
import { drawPreview, type PreviewSpec } from '../../art/preview'
import { clockNow, useAnimating, useAnimationClock, useReducedMotion } from '../../art/useArt'

interface Props { spec: PreviewSpec; label: string; className?: string }

export function ArtCanvas({ spec, label, className }: Props) {
  const ref = useRef<HTMLCanvasElement>(null)
  const reduced = useReducedMotion()
  const w = spec.cols * spec.scale, h = spec.rows * spec.scale

  const paint = () => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    const dpr = window.devicePixelRatio || 1
    const bw = Math.round(w * dpr), bh = Math.round(h * dpr)
    if (c.width !== bw || c.height !== bh) { c.width = bw; c.height = bh }
    drawPreview(g, spec, dpr, clockNow(reduced))
  }
  useLayoutEffect(paint)
  useAnimationClock(useAnimating(!!spec.scene.art?.animated), paint)
  // The pixel ratio can change (zoom, moving between screens): repaint then.
  useEffect(() => {
    const mq = window.matchMedia(`(resolution: ${window.devicePixelRatio || 1}dppx)`)
    mq.addEventListener('change', paint)
    return () => mq.removeEventListener('change', paint)
  })
  return <canvas ref={ref} className={`art-canvas ${className ?? ''}`} style={{ width: w, height: h }} role="img" aria-label={label} />
}
