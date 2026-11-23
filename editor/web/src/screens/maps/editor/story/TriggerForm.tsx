// "When": the kind of trigger and that kind's fields. Areas are picked on the map as well as typed.
import { placesOf } from '../../../../model/placeStory'
import { patchEvent, setTriggerKind } from '../../../../model/storyEdit'
import { rectOf, triggerKind, type StoryEvent, type TriggerKind } from '../../../../model/storyTypes'
import { patchObj } from '../../../../model/util'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { AreaField } from './AreaField'
import { PairPick, SupportLevelPick } from './forms/SupportForms'
import { FilterPicker, SidePick, UnitPick } from './pickers'
import { useStory } from './StoryContext'
import { PlaceTriggerForm } from './forms/PlaceForms'
import { P } from './forms/placeText'
import { S } from './text'

const GROUPS: Array<{ group: string; kinds: TriggerKind[] }> = [
  { group: S.triggerGroups.battle, kinds: ['start', 'beforePrep', 'won', 'lost', 'end'] },
  { group: S.triggerGroups.turns, kinds: ['turn', 'everyTurn'] },
  { group: S.triggerGroups.units, kinds: ['reach', 'defeated', 'fight', 'talk', 'promoted', 'support', 'place'] },
]

export function TriggerForm({ event }: { event: StoryEvent }) {
  const story = useStory()
  const { apply, map } = story
  const when = event.when ?? {}
  const kind = triggerKind(when)
  const setWhen = (p: Record<string, unknown>, key?: string) => apply((m) => patchEvent(m, event.id, { when: patchObj(when, p) }), key && `event:${event.id}:when:${key}`)
  const firstSide = map.sides?.[0]?.id ?? ''
  const hasPlaces = placesOf(story.doc, map).some((p) => p.kind !== null) || kind === 'place'
  return (
    <div className="stack tight" data-testid="trigger-form">
      <Field label={S.when} help={S.whenHelp}>
        <Select data-learn="event-when" value={kind ?? 'start'} options={GROUPS.map((g) => ({ group: g.group, options: g.kinds.map((k) => (k === 'support' && !story.doc.supports?.pairs?.length ? { value: k, label: `${S.triggerKinds[k]} (no support pairs yet)`, disabled: true } : k === 'place' && !hasPlaces ? { value: k, label: `${S.triggerKinds[k]} (${P.noneYetShort})`, disabled: true } : { value: k, label: S.triggerKinds[k] })) }))}
          onChange={(k) => apply((m) => setTriggerKind(m, event.id, k as TriggerKind, story))} />
      </Field>
      {(kind === 'turn' || kind === 'everyTurn') && (
        <>
          {kind === 'turn' && (
            <Field label={S.turnNumber}>
              <NumberInput value={typeof when.turn === 'number' ? when.turn : 1} min={1} max={100000} onChange={(v) => { if (v !== undefined) setWhen({ turn: v }, 'turn') }} />
            </Field>
          )}
          <SidePick label={S.turnSide} help={kind === 'turn' ? S.turnSideHelp : undefined} value={when.side ?? firstSide} onChange={(v) => setWhen({ side: v })} />
        </>
      )}
      {kind === 'reach' && (
        <>
          <FilterPicker label={S.reachWho} mode="optional" value={when.by} onChange={(f) => setWhen({ by: f })} />
          <AreaField label={`${S.when}: area`} mode="area" value={rectOf(when.reach)} onChange={(r) => setWhen({ reach: r })} help={`${S.reachHelp} ${S.areaHelp}`} />
        </>
      )}
      {kind === 'defeated' && (
        <>
          <FilterPicker label={S.defeatedWho} value={when.defeated} onChange={(f) => setWhen({ defeated: f ?? {} })} />
          <FilterPicker label={S.defeatedBy} mode="optional" value={when.by} onChange={(f) => setWhen({ by: f })} />
        </>
      )}
      {kind === 'fight' && (
        <>
          <FilterPicker label={S.fightOne} value={when.fight} onChange={(f) => setWhen({ fight: f ?? {} })} />
          <FilterPicker label={S.fightOther} mode="optional" value={when.with} onChange={(f) => setWhen({ with: f })} />
          <p className="help">{S.fightHelp}</p>
        </>
      )}
      {kind === 'promoted' && <FilterPicker label={S.promotedWho} value={when.promoted} onChange={(f) => setWhen({ promoted: f ?? {} })} />}
      {kind === 'support' && (
        <>
          <PairPick value={when.support} onChange={(pair) => setWhen({ support: pair, level: undefined })} />
          <SupportLevelPick pair={when.support} value={when.level} onChange={(l) => setWhen({ level: l })} />
        </>
      )}
      {kind === 'place' && <PlaceTriggerForm event={event} />}
      {kind === 'talk' && (
        <>
          <UnitPick label={S.talkWho} value={when.talk ?? ''} onChange={(v) => setWhen({ talk: v })} help={S.talkHelp} />
          <UnitPick label={S.talkTo} value={when.to ?? ''} onChange={(v) => setWhen({ to: v })} />
          <Field label={S.talkLabel} help={S.talkLabelHelp}>
            <TextInput value={when.label ?? ''} placeholder="Talk" onChange={(v) => setWhen({ label: v === '' ? undefined : v }, 'label')} />
          </Field>
        </>
      )}
    </div>
  )
}
