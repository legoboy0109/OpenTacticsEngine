// The door, chest and place pieces of the Events tab (docs/SRPG.md section 10, docs/EVENTS.md): the "A door, chest or place is used" trigger and the
// five commands that name one. The pickers list only the places of this map that fit (a chest for "open a chest", a place that has a ruin for
// "destroyed"); a story name that is no longer there stays visible as "(not there any more)" so it can be seen and changed.
import { firstPlaceForEvent, placeLabel, placesForCommand, placesForEvent } from '../../../../../model/placeStory'
import { patchEvent } from '../../../../../model/storyEdit'
import { PLACE_EVENTS, type Command, type CommandKind, type PlaceEvent, type StoryEvent } from '../../../../../model/storyTypes'
import { patchObj } from '../../../../../model/util'
import { Field } from '../../../../../ui/Field'
import { Select } from '../../../../../ui/Select'
import { FilterPicker, PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { P } from './placeText'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function PlaceTriggerForm({ event }: { event: StoryEvent }) {
  const story = useStory()
  const { apply, doc, map } = story
  const when = event.when ?? {}
  const is: PlaceEvent = PLACE_EVENTS.includes(when.is as PlaceEvent) ? (when.is as PlaceEvent) : 'opened'
  const set = (p: Record<string, unknown>) => apply((m) => patchEvent(m, event.id, { when: patchObj(when, p) }))
  const options = placesForEvent(doc, map, is).map((p) => ({ value: p.id, label: placeLabel(p) }))
  return (
    <div className="stack tight" data-testid="place-trigger">
      <Field label={P.placeEvent} help={P.placeEventHelp}>
        <Select value={is} options={PLACE_EVENTS.map((e) => ({ value: e, label: P.placeEvents[e] }))}
          onChange={(v) => {
            const next = v as PlaceEvent
            const keep = placesForEvent(doc, map, next).some((p) => p.id === when.place)
            set({ is: next, place: keep ? when.place : firstPlaceForEvent(doc, map, next) })
          }} />
      </Field>
      <PickSelect label={P.place} help={P.placeHelp} value={when.place ?? ''} options={options} empty={P.noPlaces} onChange={(v) => set({ place: v })} />
      <FilterPicker label={P.placeWho} mode="optional" value={when.by} onChange={(f) => set({ by: f })} />
      <p className="help">{P.placeWhoHelp}</p>
    </div>
  )
}

const HELP: Partial<Record<CommandKind, string>> = {
  openDoor: P.openDoorHelp, closeDoor: P.closeDoorHelp, lockDoor: P.lockDoorHelp, unlockDoor: P.unlockDoorHelp, openChest: P.openChestHelp,
}
const WHAT: Partial<Record<CommandKind, string>> = { openDoor: 'door', closeDoor: 'door', lockDoor: 'door', unlockDoor: 'door or chest', openChest: 'chest' }
/** An openChest's `by` is a unit id (the numeric `by` of the variable commands is another thing). */
const chestWho = (c: Command): string => { const v = (c as Record<string, unknown>).by; return typeof v === 'string' ? v : '' }
const PLACE_KEYS = ['openDoor', 'closeDoor', 'lockDoor', 'unlockDoor', 'openChest'] as const

/** openDoor / closeDoor / lockDoor / unlockDoor / openChest: which place, and for a chest who gets the contents. */
export function PlaceCommandForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc, map, units } = useStory()
  const kind = PLACE_KEYS.find((k) => cmd[k] !== undefined) ?? 'openDoor'
  const options = placesForCommand(doc, map, kind).map((p) => ({ value: p.id, label: placeLabel(p) }))
  return (
    <div className="stack tight" data-testid="place-command">
      <PickSelect label={P.place} value={String(cmd[kind] ?? '')} options={options} empty={P.noPlacesFor(WHAT[kind] ?? 'door')} onChange={(v) => patch({ [kind]: v })} />
      {kind === 'openChest' && (
        <PickSelect label={P.chestWho} help={P.chestWhoHelp} value={chestWho(cmd)}
          options={[{ value: '', label: P.chestNobody }, ...units.map((u) => ({ value: u.id, label: u.label }))]}
          onChange={(v) => patch({ by: v === '' ? undefined : v })} />
      )}
      <p className="help">{HELP[kind]}</p>
    </div>
  )
}
