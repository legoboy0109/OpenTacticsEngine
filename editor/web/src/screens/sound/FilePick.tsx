// "Which sound file": a drop-down of the files in the game's audio folder, plus "Add another file…" which opens the file
// chooser, uploads, and picks the new file. A file that is not in the folder stays visible as "(not there any more)".
import { useRef } from 'react'
import { audioName, lengthText } from '../../model/audioFiles'
import { Select, type Option } from '../../ui/Select'
import { useAudioFileList } from './sources'
import { T } from './text'
import { useUpload } from './useAudioUpload'

const UPLOAD = '__upload__'

export function FilePick({ value, onChange, 'aria-label': label }: { value: string; onChange: (file: string) => void; 'aria-label'?: string }) {
  const { files, loaded } = useAudioFileList()
  const upload = useUpload()
  const input = useRef<HTMLInputElement>(null)
  const options: Option[] = [
    ...(value && loaded && !files.some((f) => f.file === value) ? [{ value, label: T.missing(audioName(value)) }] : []),
    ...files.map((f) => ({ value: f.file, label: `${audioName(f.file)}${f.seconds !== undefined ? ` (${lengthText(f.seconds)})` : ''}` })),
    { value: UPLOAD, label: T.uploadAnother },
  ]
  return (
    <>
      <Select value={value} options={options} placeholder={T.chooseFile} aria-label={label}
        onChange={(v) => { if (v === UPLOAD) input.current?.click(); else onChange(v) }} />
      <input ref={input} type="file" hidden accept=".wav,.ogg,audio/wav,audio/x-wav,audio/ogg" aria-label={T.addInputLabel} data-testid="pick-file-input"
        onChange={(e) => {
          const f = e.target.files?.[0]
          e.target.value = ''
          if (f) void upload(f).then((file) => { if (file) onChange(file) })
        }} />
    </>
  )
}
