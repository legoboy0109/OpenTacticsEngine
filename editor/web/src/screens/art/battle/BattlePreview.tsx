// The live preview of a battle animation set: one animation looping, or "Preview a fight" (the set against another set or the
// plain map picture, with the game's fallback motion for whatever the set lacks). Drawn crisp on a canvas.
import { useEffect, useRef } from 'react'
import { clockNow, useAnimationClock, useArtState, usePageVisible, useReducedMotion } from '../../../art/useArt'
import { fightPreview, frameAtMs, type FighterPose } from '../../../art/battlePreview'
import type { AnimKey } from '../../../model/battleArt'
import { findSheet } from '../../../model/graphics'
import type { BattleAnimDef, BattleSetDef } from '../../../model/types'
import { useDoc, useStore } from '../../../store/context'
import { T } from './text'

export type PreviewMode = { kind: 'anim'; key: AnimKey } | { kind: 'fight'; opponent: BattleSetDef | undefined }
interface Props { set: BattleSetDef; mode: PreviewMode; label: string }

const W = 320, H = 168, GROUND = 140

export function BattlePreview({ set, mode, label }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const ref = useRef<HTMLCanvasElement>(null)
  const reduced = useReducedMotion()
  const visible = usePageVisible()

  /** Draws one frame of `anim` with its foot point at (x, GROUND); `mirror` turns it to face left. */
  const sprite = (g: CanvasRenderingContext2D, s: BattleSetDef, anim: BattleAnimDef, ms: number, loop: boolean, x: number, dx: number, scale: number, mirror: boolean, alpha = 1) => {
    const sheet = findSheet(doc, anim.sheet)
    const img = sheet ? store.art.image(sheet.file) : null
    if (!sheet || !img || img.status !== 'ready' || !img.img) return
    const cell = anim.frames[Math.min(frameAtMs(anim, ms, loop), anim.frames.length - 1)] ?? [0, 0]
    const ax = s.anchor?.[0] ?? Math.floor(sheet.cellWidth / 2), ay = s.anchor?.[1] ?? sheet.cellHeight
    const flip = mirror !== (s.faces === 'left') && s.mirror !== false
    g.save()
    g.globalAlpha = alpha
    g.translate(x + dx, GROUND)
    if (flip) g.scale(-1, 1)
    g.drawImage(img.img, cell[0] * sheet.cellWidth, cell[1] * sheet.cellHeight, sheet.cellWidth, sheet.cellHeight, -ax * scale, -ay * scale, sheet.cellWidth * scale, sheet.cellHeight * scale)
    g.restore()
  }
  const block = (g: CanvasRenderingContext2D, x: number, dx: number, scale: number, alpha = 1) => {
    g.save()
    g.globalAlpha = alpha
    g.fillStyle = '#c9524a'
    g.fillRect(x + dx - 12 * scale / 2, GROUND - 20 * scale, 12 * scale, 20 * scale)
    g.strokeStyle = '#161620'
    g.strokeRect(x + dx - 12 * scale / 2 + 0.5, GROUND - 20 * scale + 0.5, 12 * scale - 1, 20 * scale - 1)
    g.restore()
  }
  /** One fighter of the fight preview: its set (or the map picture block) with the pose's animation or fallback motion. */
  const fighter = (g: CanvasRenderingContext2D, s: BattleSetDef | undefined, pose: FighterPose, x: number, toward: 1 | -1, scale: number, t: number) => {
    let dx = 0
    if (pose.fallback === 'lunge') dx = toward * Math.sin(Math.min(1, pose.progress) * Math.PI) * 5 * scale
    else if (pose.fallback === 'shake') dx = (Math.floor(pose.progress * 18) % 2 ? 1 : -1) * 2 * scale * (1 - pose.progress)
    else if (pose.fallback === 'sidestep') dx = -toward * Math.sin(Math.min(1, pose.progress) * Math.PI) * 6 * scale
    if (!s) { block(g, x, dx, scale); return }
    const a = pose.fallback === 'none' && s.animations[pose.anim] ? s.animations[pose.anim] : s.animations.stand
    if (a) sprite(g, s, a, pose.fallback === 'none' && pose.anim !== 'stand' ? pose.localMs : t, pose.anim === 'stand' || pose.fallback !== 'none', x, dx, scale, toward === -1)
  }

  const draw = () => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    const t = clockNow(reduced)
    g.imageSmoothingEnabled = false
    g.clearRect(0, 0, W, H)
    g.fillStyle = '#26324f'
    g.fillRect(0, 0, W, GROUND)
    g.fillStyle = '#3f5a36'
    g.fillRect(0, GROUND, W, H - GROUND)
    g.fillStyle = 'rgba(0,0,0,0.25)'
    g.fillRect(0, GROUND, W, 2)
    const stand = set.animations.stand
    const sheet = stand ? findSheet(doc, stand.sheet) : undefined
    const fh = sheet?.cellHeight ?? 24
    const scale = Math.max(1, Math.min(4, Math.floor(120 / fh)))
    if (mode.kind === 'anim') {
      const a = set.animations[mode.key]
      if (a) sprite(g, set, a, t, mode.key === 'stand' || a.loop === true, W / 2, 0, scale, false)
      return
    }
    const p = fightPreview(set, mode.opponent, t)
    fighter(g, set, p.attacker, W * 0.36, 1, scale, t)
    fighter(g, mode.opponent, p.defender, W * 0.64, -1, scale, t)
  }
  // Draw on every render (the canvas has no state of its own) and about 20 times a second while it moves.
  useEffect(draw)
  useAnimationClock(!reduced && visible, draw)

  return <canvas ref={ref} className="battle-preview" width={W} height={H} role="img" aria-label={label} data-testid="battle-preview" title={T.previewTitle} />
}
