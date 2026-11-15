// The Music part: the tracks of the game. Each has a file, a play button (it repeats like the game does), a volume, and
// whether and from where it repeats.
import { soundPreview } from '../../audio/preview'
import { useSoundPreview } from '../../audio/useSoundPreview'
import { setTrackFile, setTrackLoopStart, setTrackRepeats, setTrackVolume, tracksRepeats } from '../../model/audio'
import type { MusicTrack } from '../../model/audioTypes'
import { useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { FilePick } from './FilePick'
import { TrackPlay } from './sources'
import { T } from './text'

function TrackItem({ id, track, onRename, onDelete }: { id: string; track: MusicTrack; onRename: () => void; onDelete: () => void }) {
  const edit = useEdit()
  const { key } = useSoundPreview()
  const playing = key === `music:${id}`
  const k = `track:${id}`
  const repeats = tracksRepeats(track)
  return (
    <li className="sound-item" data-testid={`track-${id}`}>
      <div className="sound-head">
        <TrackPlay id={id} track={track} />
        <strong className="sound-name">{id}</strong>
        <span className="item-row-buttons">
          <button type="button" className="btn ghost sm" aria-label={T.renameLabel(id)} onClick={onRename}>{T.rename}</button>
          <button type="button" className="btn ghost sm icon" aria-label={T.deleteLabel(id)} onClick={onDelete}><Icon name="trash" /></button>
        </span>
      </div>
      <FormGrid>
        <Field label={T.trackFile}>
          <FilePick value={track.file ?? ''} aria-label={`${id}: ${T.trackFile}`} onChange={(f) => edit((d) => setTrackFile(d, id, f))} />
        </Field>
        <Field label={T.trackVolume} help={T.trackVolumeHelp}>
          <NumberInput value={track.volume ?? 100} min={0} max={100} suffix="%" aria-label={`${id}: ${T.trackVolume}`}
            onChange={(v) => { if (v !== undefined) edit((d) => setTrackVolume(d, id, v), `${k}:volume`) }} />
        </Field>
      </FormGrid>
      <Check label={T.repeats} help={T.repeatsHelp} checked={repeats} onChange={(v) => edit((d) => setTrackRepeats(d, id, v))} />
      {repeats && (
        <Field label={T.repeatFrom} help={T.repeatFromHelp}>
          <div className="row">
            <NumberInput value={track.loopStart ?? 0} min={0} max={36000} integer={false} suffix="seconds" aria-label={`${id}: ${T.repeatFrom}`}
              onChange={(v) => { if (v !== undefined) edit((d) => setTrackLoopStart(d, id, v), `${k}:loopStart`) }} />
            <button type="button" className="btn sm" disabled={!playing} title={playing ? T.repeatFromNowHelp : T.repeatFromNowHelp}
              onClick={() => edit((d) => setTrackLoopStart(d, id, Math.round(soundPreview.position() * 100) / 100))}>{T.repeatFromNow}</button>
          </div>
        </Field>
      )}
    </li>
  )
}

export function MusicCard({ tracks, onAdd, onRename, onDelete }: { tracks: Array<[string, MusicTrack]>; onAdd: () => void; onRename: (id: string) => void; onDelete: (id: string) => void }) {
  return (
    <Card title={T.musicTitle} help={T.musicHelp} id="music" className="sound-card" actions={<button type="button" className="btn sm primary" onClick={onAdd}>{T.addMusic}</button>}>
      <div data-testid="music-card">
        {tracks.length === 0 && <p className="help">{T.noMusic}</p>}
        <ul className="sound-list" aria-label={T.musicTitle}>
          {tracks.map(([id, t]) => <TrackItem key={id} id={id} track={t} onRename={() => onRename(id)} onDelete={() => onDelete(id)} />)}
        </ul>
      </div>
    </Card>
  )
}
