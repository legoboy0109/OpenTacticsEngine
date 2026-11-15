// The Sounds part: the sound effects of the game. Each is one file or several variants (one is picked each time).
import { MAX_VARIANTS, filesOfSound, setSoundFiles, setSoundVolume } from '../../model/audio'
import type { SoundEffect } from '../../model/audioTypes'
import { useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { FilePick } from './FilePick'
import { FilePlay, SoundPlay, useAudioFileList } from './sources'
import { T } from './text'

function SoundItem({ id, sound, onRename, onDelete }: { id: string; sound: SoundEffect; onRename: () => void; onDelete: () => void }) {
  const edit = useEdit()
  const { files: folder } = useAudioFileList()
  const list = filesOfSound(sound)
  const shown = list.length ? list : ['']
  const setList = (next: string[]) => edit((d) => setSoundFiles(d, id, next.filter((f) => f !== '')))
  const addVariant = () => {
    const fresh = folder.find((f) => !list.includes(f.file))?.file ?? list[list.length - 1]
    if (fresh) setList([...list, fresh])
  }
  return (
    <li className="sound-item" data-testid={`sound-${id}`}>
      <div className="sound-head">
        <SoundPlay id={id} sound={sound} />
        <strong className="sound-name">{id}</strong>
        <span className="item-row-buttons">
          <button type="button" className="btn ghost sm" aria-label={T.renameLabel(id)} onClick={onRename}>{T.rename}</button>
          <button type="button" className="btn ghost sm icon" aria-label={T.deleteLabel(id)} onClick={onDelete}><Icon name="trash" /></button>
        </span>
      </div>
      <FormGrid>
        <Field label={T.soundVolume} help={T.soundVolumeHelp}>
          <NumberInput value={sound.volume ?? 100} min={0} max={100} suffix="%" aria-label={`${id}: ${T.soundVolume}`}
            onChange={(v) => { if (v !== undefined) edit((d) => setSoundVolume(d, id, v), `sound:${id}:volume`) }} />
        </Field>
      </FormGrid>
      <div className="stack tight" role="group" aria-label={`${id}: ${T.variants}`}>
        <div className="label-sm">{T.variants}</div>
        <p className="help">{T.variantsHelp}</p>
        <ul className="variant-list">
          {shown.map((f, i) => (
            <li key={i} className="variant-row" data-testid={`variant-${id}-${i + 1}`}>
              <FilePlay playKey={`sound:${id}:${i}`} what={`${id} ${T.variantN(i + 1)}`} file={f} volume={sound.volume ?? 100} />
              <div className="variant-pick">
                <FilePick value={f} aria-label={`${id}: ${T.variantN(i + 1)}`} onChange={(file) => setList(shown.map((x, j) => (j === i ? file : x)))} />
              </div>
              {shown.length > 1 && (
                <button type="button" className="btn ghost sm icon" aria-label={`${id}: ${T.removeVariant(i + 1)}`} onClick={() => setList(shown.filter((_, j) => j !== i))}><Icon name="close" /></button>
              )}
            </li>
          ))}
        </ul>
        <div><button type="button" className="btn sm" disabled={list.length >= MAX_VARIANTS || list.length === 0 || (folder.length === 0)} title={list.length >= MAX_VARIANTS ? T.variantsFull : undefined}
          aria-label={`${id}: ${T.addVariant}`} onClick={addVariant}><Icon name="plus" />{T.addVariant}</button></div>
      </div>
    </li>
  )
}

export function SoundsCard({ sounds, onAdd, onRename, onDelete }: { sounds: Array<[string, SoundEffect]>; onAdd: () => void; onRename: (id: string) => void; onDelete: (id: string) => void }) {
  return (
    <Card title={T.soundsTitle} help={T.soundsHelp} id="sounds" className="sound-card" actions={<button type="button" className="btn sm primary" onClick={onAdd}>{T.addSound}</button>}>
      <div data-testid="sounds-card">
        {sounds.length === 0 && <p className="help">{T.noSounds}</p>}
        <ul className="sound-list" aria-label={T.soundsTitle}>
          {sounds.map(([id, s]) => <SoundItem key={id} id={id} sound={s} onRename={() => onRename(id)} onDelete={() => onDelete(id)} />)}
        </ul>
      </div>
    </Card>
  )
}
