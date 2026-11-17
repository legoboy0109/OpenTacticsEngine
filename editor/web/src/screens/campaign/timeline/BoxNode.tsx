// One box of the timeline: a scene (before / after the battle) or a level (the map, with its picture, and its events when expanded).
// The big button opens the box's normal editor; the small ones show the problems, or the events of a level. A box never shows what
// a scene says (no first line, no speakers): only what it changes.
import { memo } from 'react'
import { BOX_H, EVENT_ROW_H, GROUP_HEAD_H, EVENTS_PAD } from '../../../model/timelineLayout'
import type { Rect, TimelineEvent, TimelineNode } from '../../../model/timelineTypes'
import { Icon } from '../../../ui/Icon'
import { MarkerRow, markersText } from './Markers'
import { TL } from './text'

export interface BoxActions {
  open: (n: TimelineNode) => void
  toggle: (chapterId: string) => void
  openEvent: (e: TimelineEvent) => void
  problems: () => void
}

interface Props {
  node: TimelineNode
  rect: Rect
  expanded: boolean
  problems: string[]
  eventProblems: Map<string, number>
  thumb: string | null
  selected: boolean
  /** The one box that is a tab stop (the others are reached with the arrow keys). */
  roving: boolean
  dragging: boolean
  actions: BoxActions
}

export const BoxNode = memo(function BoxNode({ node, rect, expanded, problems, eventProblems, thumb, selected, roving, dragging, actions }: Props) {
  const level = node.kind === 'level'
  const part = level ? TL.partLevel : node.which === 'before' ? TL.sceneBeforeTitle : TL.sceneAfterTitle
  const markers = markersText(node.markers)
  const label = level
    ? (node.mapKnown ? TL.openLevel(node.chapterName, node.mapName ?? '') : TL.openLevelNoMap(node.chapterName))
    : TL.openScene(part, node.chapterName)
  const full = [label, markers, node.unreached ? TL.unreached : '', node.skipped ? TL.skipped : ''].filter(Boolean).join('. ')
  const mapText = !node.mapId ? TL.noMap : node.mapKnown ? node.mapName : TL.mapGone(node.mapId)
  return (
    <div className="tl-node" data-kind={node.kind} data-node-id={node.id} data-chapter={node.chapterId} data-drag-chapter={node.chapterId}
      data-selected={selected || undefined} data-unreached={node.unreached || undefined} data-skipped={node.skipped || undefined} data-dragging={dragging || undefined}
      data-expanded={expanded || undefined} data-problems={problems.length > 0 || undefined} data-testid={`box-${node.id}`}
      style={{ left: rect.x, top: rect.y, width: rect.w, height: rect.h }}>
      <button type="button" className="tl-open" data-node-open={node.id} tabIndex={roving ? 0 : -1} aria-label={full} title={node.skipped ? TL.skipped : undefined}
        onClick={() => actions.open(node)}>
        {level && (thumb ? <img className="tl-thumb" src={thumb} alt="" width={72} height={48} draggable={false} /> : <span className="tl-thumb tl-thumb-none" aria-hidden="true" />)}
        <span className="tl-text">
          {!level && <span className="tl-kicker">{part}</span>}
          <span className="tl-title">{node.chapterName}</span>
          {level && <span className="tl-sub">{mapText}</span>}
          {!level && node.commandCount === 0 && <span className="tl-sub">{TL.emptyScene}</span>}
        </span>
        <MarkerRow markers={node.markers} />
      </button>
      {problems.length > 0 && (
        <button type="button" className="tl-badge" tabIndex={roving ? 0 : -1} data-testid={`badge-${node.id}`} aria-label={TL.badgeLabel(problems.length, problems)}
          title={TL.badgeTitle(problems)} onClick={actions.problems}>
          <Icon name="warn" />{problems.length}
        </button>
      )}
      {level && node.events.length > 0 && (
        <button type="button" className="tl-expand" tabIndex={roving ? 0 : -1} aria-expanded={expanded} data-testid={`expand-${node.chapterId}`}
          aria-label={expanded ? TL.collapse : TL.expand(node.events.length)} title={expanded ? TL.collapse : TL.expand(node.events.length)}
          onClick={() => actions.toggle(node.chapterId)}>
          <Icon name={expanded ? 'up' : 'down'} />{node.events.length}
        </button>
      )}
      {level && expanded && <EventList node={node} eventProblems={eventProblems} roving={roving} actions={actions} />}
    </div>
  )
})

function EventList({ node, eventProblems, roving, actions }: { node: TimelineNode; eventProblems: Map<string, number>; roving: boolean; actions: BoxActions }) {
  if (node.events.length === 0) return <p className="tl-no-events" style={{ top: BOX_H }}>{TL.noEvents}</p>
  let last = ''
  const rows: JSX.Element[] = []
  for (const e of node.events) {
    if (e.group !== last) {
      last = e.group
      rows.push(<li key={`g-${e.group}`} className="tl-group" style={{ height: GROUP_HEAD_H }}>{TL.groups[e.group]}</li>)
    }
    const bad = eventProblems.get(`${node.chapterId}/${e.eventId}`) ?? 0
    rows.push(
      <li key={e.key} className="tl-event-item" style={{ height: EVENT_ROW_H }}>
        <button type="button" className="tl-event" data-testid={`event-${e.key}`} tabIndex={roving ? 0 : -1}
          aria-label={`${TL.openEvent(e.eventId, e.when)}${markersText(e.markers) ? `. ${markersText(e.markers)}` : ''}${bad ? `. ${TL.badge(bad)}` : ''}`}
          onClick={() => actions.openEvent(e)}>
          <span className="tl-event-when">{e.when}</span>
          <span className="tl-event-id">{e.eventId}{bad > 0 && <Icon name="warn" className="tl-event-warn" />}</span>
          <MarkerRow markers={e.markers} />
        </button>
      </li>,
    )
  }
  return <ul className="tl-events" style={{ top: BOX_H, paddingTop: EVENTS_PAD }} aria-label={TL.expand(node.events.length)}>{rows}</ul>
}
