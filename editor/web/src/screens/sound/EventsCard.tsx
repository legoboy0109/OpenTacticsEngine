// "When things happen": the game's standard set. Every moment is a row with a picker ("Nothing" or a track / sound) and a
// play button for the choice. Music first, then the sound events in groups.
import { isJingleSlot, MUSIC_SLOT_LABEL, MUSIC_SLOTS, SOUND_GROUPS, SOUND_KEY_LABEL, type MusicSlot, type StandardSoundKey } from '../../model/audioLabels'
import { musicOf, setUseMusic, setUseSound, soundsOf } from '../../model/audio'
import type { ProjectDoc } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { AudioChoice } from './AudioChoice'
import { PromotionSounds } from './PromotionSounds'
import { SoundPlay, TrackPlay } from './sources'
import { T } from './text'

const shownValue = (v: unknown): string | undefined => (typeof v === 'string' && v !== '' ? v : undefined)

function MusicRow({ doc, slot }: { doc: ProjectDoc; slot: MusicSlot }) {
  const edit = useEdit()
  const value = shownValue(doc.audio?.use?.[slot])
  const track = value !== undefined ? musicOf(doc)[value] : undefined
  const jingle = isJingleSlot(slot)
  return (
    <li className="slot-row" data-testid={`slot-${slot}`}>
      <span className="slot-label">{MUSIC_SLOT_LABEL[slot]}{jingle && <span className="help slot-note"> ({T.oncePlays})</span>}</span>
      <AudioChoice kind="music" mode="nothing" value={value} aria-label={MUSIC_SLOT_LABEL[slot]} onChange={(v) => edit((d) => setUseMusic(d, slot, v))} />
      {track && value !== undefined
        ? <TrackPlay id={value} track={track} playKey={`slot:${slot}`} />
        : <span className="play-gap" aria-hidden="true" />}
    </li>
  )
}

function SoundRow({ doc, event }: { doc: ProjectDoc; event: StandardSoundKey }) {
  const edit = useEdit()
  const value = shownValue(doc.audio?.use?.sounds?.[event])
  const sound = value !== undefined ? soundsOf(doc)[value] : undefined
  return (
    <li className="slot-row" data-testid={`slot-${event}`}>
      <span className="slot-label">{SOUND_KEY_LABEL[event]}</span>
      <AudioChoice data-learn={`sound-event-${event}`} kind="sound" mode="nothing" value={value} aria-label={SOUND_KEY_LABEL[event]} onChange={(v) => edit((d) => setUseSound(d, event, v))} />
      {sound && value !== undefined
        ? <SoundPlay id={value} sound={sound} playKey={`slot:${event}`} />
        : <span className="play-gap" aria-hidden="true" />}
    </li>
  )
}

export function EventsCard() {
  const doc = useDoc()
  return (
    <Card title={T.eventsTitle} help={T.eventsHelp} id="events" className="sound-card">
      <div className="stack" data-testid="events-card">
        <section className="slot-group" aria-label={T.musicGroup}>
          <h3>{T.musicGroup}</h3>
          <ul className="slot-list">{MUSIC_SLOTS.map((s) => <MusicRow key={s} doc={doc} slot={s} />)}</ul>
        </section>
        <PromotionSounds />
        {SOUND_GROUPS.map((g) => (
          <section key={g.title} className="slot-group" aria-label={g.title}>
            <h3>{g.title}</h3>
            <ul className="slot-list">{g.keys.map((k) => <SoundRow key={k} doc={doc} event={k} />)}</ul>
          </section>
        ))}
      </div>
    </Card>
  )
}
