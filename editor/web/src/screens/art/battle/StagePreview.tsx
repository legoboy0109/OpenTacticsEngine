// A small live preview of the fixed stage: the default backdrop (when it has one), a placeholder box for each fighter standing on its foot
// point, and the platforms at their places (the right one mirrored). Plain SVG shapes in stage pixels; a point outside the stage is drawn
// anyway (in red) so a maker sees what is wrong.
import { backgroundList, defaultBackground } from '../../../model/battleArt'
import { stageAlign, stageOf } from '../../../model/battleStage'
import { findSheet } from '../../../model/graphics'
import type { StagePair } from '../../../model/types'
import { useArtState } from '../../../art/useArt'
import { useDoc, useStore } from '../../../store/context'
import { ST } from './stageText'

const BOX_W = 20, BOX_H = 36, PLAT_W = 56, PLAT_H = 16

export function StagePreview({ far }: { far: boolean }) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const s = stageOf(doc)
  if (!s) return null
  const feet: StagePair = far && s.feetFar ? s.feetFar : s.feet
  const plat: StagePair | undefined = far && s.platformFar ? s.platformFar : s.platform
  const bg = backgroundList(doc).find((b) => b.id === defaultBackground(doc))
  const sheet = bg && findSheet(doc, bg.sheet)
  const img = sheet ? store.art.image(sheet.file) : null
  const src = img && img.status === 'ready' && img.img instanceof HTMLImageElement ? img.img.src : ''
  const ready = !!(sheet && bg && img && src)
  const align = stageAlign(s)
  const cw = sheet?.cellWidth ?? 0, ch = sheet?.cellHeight ?? 0
  const ox = Math.floor((s.width - cw) / 2)
  const oy = align === 'top' ? 0 : align === 'bottom' ? s.height - ch : Math.floor((s.height - ch) / 2)
  const inside = (p: [number, number]) => p[0] >= 0 && p[1] >= 0 && p[0] < s.width && p[1] < s.height
  const plW = PLAT_W
  return (
    <svg className="stage-preview" data-testid="stage-preview" role="img" aria-label={ST.previewLabel} viewBox={`0 0 ${s.width} ${s.height}`}
      style={{ width: '100%', maxWidth: Math.max(160, Math.min(480, s.width * 2)), height: 'auto', display: 'block', background: '#141a33', imageRendering: 'pixelated' }}>
      {ready && sheet && bg && img && (
        <svg x={0} y={0} width={s.width} height={s.height} viewBox={`0 0 ${s.width} ${s.height}`}>
          <svg x={ox} y={oy} width={cw} height={ch} viewBox={`${bg.cell[0] * cw} ${bg.cell[1] * ch} ${cw} ${ch}`} preserveAspectRatio="none">
            <image href={src} x={0} y={0} width={img.width} height={img.height} preserveAspectRatio="none" style={{ imageRendering: 'pixelated' }} />
          </svg>
        </svg>
      )}
      {!ready && <rect x={0} y={s.height * 0.6} width={s.width} height={s.height * 0.4} fill="#26402d" />}
      {(['left', 'right'] as const).map((side) => {
        const f = feet[side]
        const p = plat?.[side]
        const bad = !inside(f)
        return (
          <g key={side} data-testid={`stage-preview-${side}`}>
            {p && (
              <g>
                <rect x={p[0]} y={p[1]} width={plW} height={PLAT_H} fill="#b07a38" stroke={inside(p) ? '#e8c27a' : '#ff5c5c'} strokeWidth={1} />
                {/* the red / blue ends show the right-hand one is the left one turned round */}
                <rect x={side === 'left' ? p[0] : p[0] + plW - 4} y={p[1]} width={4} height={4} fill="#d33" />
                <rect x={side === 'left' ? p[0] + plW - 4 : p[0]} y={p[1]} width={4} height={4} fill="#36d" />
              </g>
            )}
            {!p && <rect x={f[0] - plW / 2} y={f[1] - PLAT_H / 2} width={plW} height={PLAT_H} fill="#b07a38" opacity={0.55} />}
            <rect x={f[0] - BOX_W / 2} y={f[1] - BOX_H} width={BOX_W} height={BOX_H} fill={side === 'left' ? '#c44' : '#46c'} stroke={bad ? '#ff5c5c' : '#fff'} strokeWidth={1} opacity={0.9} />
            <rect x={f[0] - 1} y={f[1] - 1} width={2} height={2} fill="#f0f" />
          </g>
        )
      })}
      <rect x={0.5} y={0.5} width={s.width - 1} height={s.height - 1} fill="none" stroke="#7a84b8" strokeWidth={1} />
    </svg>
  )
}
