// The "Events" tab: the map's events as a list (name + one plain sentence), and the open event's editor.
import { useMemo, useState } from 'react'
import { storyChecks } from '../../../../model/storyChecks'
import { addEvent, deleteEvent, moveEvent } from '../../../../model/storyEdit'
import { eventSummary } from '../../../../model/storySummary'
import { Icon } from '../../../../ui/Icon'
import type { MapViewState } from '../viewState'
import { EventEditor } from './EventEditor'
import { RowButtons } from '../../../../ui/RowButtons'
import { StoryChecks } from './StoryChecks'
import { StoryProvider, useStory } from './StoryContext'
import { S } from './text'

export function EventsPanel({ mapId, vs }: { mapId: string; vs: MapViewState }) {
  return <StoryProvider mapId={mapId} vs={vs}><EventsBody /></StoryProvider>
}

function EventsBody() {
  const story = useStory()
  const { doc, map, mapId, apply } = story
  const events = map.events ?? []
  // The open event is remembered by its place in the list, so renaming it keeps it open.
  const [open, setOpen] = useState<number | null>(null)
  const checks = useMemo(() => storyChecks(doc, mapId), [doc, mapId])
  const current = open !== null ? events[open] : undefined

  if (current) {
    return <EventEditor event={current} checks={checks.filter((c) => c.eventId === current.id)} onBack={() => setOpen(null)} />
  }
  return (
    <div className="stack tight" data-testid="events-panel">
      <div className="row space-between">
        <h3>{S.eventsTitle}</h3>
        <button type="button" className="btn sm" onClick={() => { apply((m) => addEvent(m, story)); setOpen(events.length) }}><Icon name="plus" />{S.addEvent}</button>
      </div>
      <p className="help">{S.eventsHelp}</p>
      <StoryChecks checks={checks} />
      {events.length === 0 && <p className="help">{S.noEvents}</p>}
      <ul className="event-list" aria-label={S.eventsTitle}>
        {events.map((e, i) => (
          <li key={i} className="event-item" data-testid={`event-${e.id}`}>
            <button type="button" className="event-open" aria-label={S.openEvent(e.id)} onClick={() => setOpen(i)}>
              <strong className="event-name">{e.id}{checks.some((c) => c.eventId === e.id) && <Icon name="warn" className="event-warn" />}</strong>
              <span className="event-summary">{eventSummary(story, e)}</span>
            </button>
            <RowButtons index={i} count={events.length} up={S.moveEventUp(e.id)} down={S.moveEventDown(e.id)} del={S.deleteEvent(e.id)}
              onUp={() => apply((m) => moveEvent(m, i, i - 1))} onDown={() => apply((m) => moveEvent(m, i, i + 1))} onDelete={() => apply((m) => deleteEvent(m, e.id))} />
          </li>
        ))}
      </ul>
    </div>
  )
}
