// The arrows of the timeline: one SVG for the paths, plus (as ordinary HTML on top) the labels of arrows that have one and the
// small "add a chapter here" buttons on the order arrows between neighbouring chapters. Only arrows near the screen are drawn.
import { Fragment, memo } from 'react'
import type { TimelineEdge, TimelineGraph, TimelineLayout } from '../../../model/timelineTypes'
import { Icon } from '../../../ui/Icon'
import { TL } from './text'

interface Props {
  graph: TimelineGraph
  layout: TimelineLayout
  /** The arrows to draw now (the others are off screen). */
  visible: TimelineEdge[]
  names: Map<string, string>
  onAddBetween: (beforeChapter: string, afterChapter: string) => void
}

const KIND_WORD = { order: TL.arrowOrder, next: TL.arrowNext, goto: TL.arrowGoto }

export const EdgesLayer = memo(function EdgesLayer({ layout, visible, names, onAddBetween }: Props) {
  return (
    <>
      <svg className="tl-edges" width={layout.width} height={layout.height} role="group" aria-label={TL.legendArrows.replace(/:$/, '')}>
        <defs>
          {(['order', 'next', 'goto'] as const).map((k) => (
            <marker key={k} id={`tl-arrow-${k}`} className={`tl-arrow tl-arrow-${k}`} markerWidth="9" markerHeight="9" refX="8" refY="4.5" orient="auto" markerUnits="userSpaceOnUse">
              <path d="M0 0.5L8.5 4.5L0 8.5z" />
            </marker>
          ))}
        </defs>
        {visible.map((e) => {
          const p = layout.edges[e.id]
          const label = TL.arrowLabel(names.get(e.from) ?? e.from, names.get(e.to) ?? e.to, e.label, KIND_WORD[e.kind])
          return (
            <g key={e.id} role="img" aria-label={label} data-testid={`edge-${e.from}>${e.to}`} data-kind={e.kind} data-back={e.backward || undefined}>
              <path className={`tl-edge tl-edge-${e.kind}${e.backward ? ' tl-edge-back' : ''}`} d={p.path} markerEnd={`url(#tl-arrow-${e.kind})`} fill="none" />
              <title>{label}</title>
            </g>
          )
        })}
      </svg>
      {visible.map((e) => {
        const p = layout.edges[e.id]
        return (
          <Fragment key={e.id}>
            {e.label && p.label && <span className={`tl-edge-label tl-label-${e.kind}`} data-testid={`label-${e.from}>${e.to}`} style={{ left: p.label.x, top: p.label.y }} title={e.label} aria-hidden="true">{e.label}</span>}
            {e.between && p.mid && (
              <button type="button" className="tl-plus" style={{ left: p.mid.x, top: p.mid.y }} data-testid={`add-between-${e.between.before}`}
                aria-label={TL.addBetween(names.get(e.from) ?? e.from, names.get(e.to) ?? e.to)} title={TL.addBetween(names.get(e.from) ?? e.from, names.get(e.to) ?? e.to)}
                onClick={() => onAddBetween(e.between!.before, e.between!.after)}>
                <Icon name="plus" />
              </button>
            )}
          </Fragment>
        )
      })}
    </>
  )
})
