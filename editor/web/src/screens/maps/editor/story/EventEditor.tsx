// One open event: its name, "When", "Only if", Repeat and the "Then" list of commands. While it is open the map draws
// its area and the squares its commands use.
import { useEffect, useState } from 'react'
import type { StoryCheck } from '../../../../model/storyChecks'
import { eventIds, renameEvent, setEventConditions, setEventRepeat } from '../../../../model/storyEdit'
import { idProblem } from '../../../../model/storyIds'
import { eventOverlays } from '../../../../model/storyOverlays'
import { triggerKind, type StoryEvent } from '../../../../model/storyTypes'
import { Field } from '../../../../ui/Field'
import { Icon } from '../../../../ui/Icon'
import { Check } from '../../../../ui/Toggle'
import { CommandList } from './CommandList'
import { ConditionList } from './ConditionList'
import { IdInput } from '../../../../ui/IdInput'
import { StoryChecks } from './StoryChecks'
import { useStory } from './StoryContext'
import { S } from './text'
import { TriggerForm } from './TriggerForm'

export function EventEditor({ event, checks, onBack }: { event: StoryEvent; checks: StoryCheck[]; onBack: () => void }) {
  const { apply, map, vs } = useStory()
  const [selected, setSelected] = useState<string | null>(null)
  const kind = triggerKind(event.when ?? {})
  const others = eventIds(map).filter((id) => id !== event.id)

  // The map draws the trigger's area and the squares of this event's commands while it is open.
  useEffect(() => { vs.set({ overlays: eventOverlays(event) }) }, [vs, event])
  useEffect(() => () => vs.set({ overlays: [] }), [vs])

  return (
    <div className="event-editor stack tight" data-testid="event-editor">
      <button type="button" className="btn ghost sm back-link" data-learn="events-back" onClick={onBack}><Icon name="back" />{S.allEvents}</button>
      <Field label={S.eventName} help={S.eventNameHelp}>
        <IdInput value={event.id} validate={(t) => idProblem(t, others)} onChange={(v) => apply((m) => renameEvent(m, event.id, v), `event:${event.id}:name`)} />
      </Field>
      <StoryChecks checks={checks} testId="event-checks" compact />
      <section className="story-section"><TriggerForm event={event} /></section>
      <section className="story-section">
        <h3>{S.onlyIf}</h3>
        <p className="help">{S.onlyIfHelp}</p>
        <ConditionList name={S.onlyIf} conditions={event.if ?? []} onChange={(list, k) => apply((m) => setEventConditions(m, event.id, list), k && `event:${event.id}:if:${k}`)} />
      </section>
      <Check label={S.repeat} help={kind === 'everyTurn' ? S.repeatAlways : S.repeatHelp} checked={kind === 'everyTurn' || event.repeat === true} disabled={kind === 'everyTurn'}
        onChange={(v) => apply((m) => setEventRepeat(m, event.id, v))} />
      <section className="story-section">
        <h3>{S.then}</h3>
        <p className="help">{S.thenHelp}</p>
        <CommandList eventId={event.id} path={[]} commands={event.do ?? []} talk={kind === 'talk'} selected={selected} onSelect={setSelected} name={S.then} />
      </section>
    </div>
  )
}
