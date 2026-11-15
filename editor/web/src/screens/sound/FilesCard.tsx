// The sound files of the game: what each is, what uses it, which nobody uses; play one, use an unused one as music or a
// sound, delete one. Deleting a file that tracks or sounds use asks first and takes them out in the same Undo step; the file
// itself is gone for good (Undo never touches files).
import { useState } from 'react'
import { dropAudioFile } from '../../model/audio'
import { audioFileUsers, audioName, lengthText, unusedAudioFiles } from '../../model/audioFiles'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { FilePlay, useAudioFileList } from './sources'
import { T } from './text'

export function FilesCard({ onUse, say }: { onUse: (kind: 'music' | 'sound', file: string) => void; say: (tone: 'info' | 'error' | 'success', text: string) => void }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const { files } = useAudioFileList()
  const [deleting, setDeleting] = useState<string | null>(null)
  if (files.length === 0) return null
  const unused = new Set(unusedAudioFiles(doc, files))
  const users = deleting ? audioFileUsers(doc, deleting) : []

  const remove = async () => {
    const file = deleting
    setDeleting(null)
    if (!file) return
    // The helper refuses while the SAVED sound settings still name the file: take them out, save, then delete.
    if (audioFileUsers(doc, file).length) edit((d) => dropAudioFile(d, file))
    await store.flush()
    const res = await store.audioFiles.remove(file)
    if (res.ok) { say('success', T.fileDeleted(file)); return }
    if (res.status === 409 && res.uses?.length) say('error', T.fileStillUsed(file, res.uses.map((u) => u.id).join(', ')))
    else say('error', T.fileDeleteFailed(file, res.error))
  }

  return (
    <Card title={T.filesTitle} help={T.filesHelp} id="files" className="sound-card">
      <ul className="unused-list" data-testid="sound-files">
        {files.map((f) => {
          const by = audioFileUsers(doc, f.file).map((u) => u.id)
          return (
            <li key={f.file} data-testid={`file-${audioName(f.file)}`}>
              <FilePlay playKey={`file:${f.file}`} what={audioName(f.file)} file={f.file} />
              <span className="unused-name">
                <strong>{f.file}</strong>
                <span className="help">
                  {f.error ? T.unreadable : [lengthText(f.seconds), f.rate ? T.fileRate(f.rate, f.channels ?? 0) : ''].filter(Boolean).join(', ')}
                  {' · '}{unused.has(f.file) ? T.fileUnused : T.fileUsedBy(by)}
                </span>
              </span>
              <span className="row">
                {unused.has(f.file) && !f.error && (
                  <>
                    <button type="button" className="btn sm" aria-label={`${T.useAsMusic}: ${f.file}`} onClick={() => onUse('music', f.file)}>{T.useAsMusic}</button>
                    <button type="button" className="btn sm" aria-label={`${T.useAsSound}: ${f.file}`} onClick={() => onUse('sound', f.file)}>{T.useAsSound}</button>
                  </>
                )}
                <button type="button" className="btn sm danger" aria-label={T.deleteFileLabel(f.file)} onClick={() => setDeleting(f.file)}>{T.deleteFile}</button>
              </span>
            </li>
          )
        })}
      </ul>
      <ConfirmDialog open={!!deleting} title={T.deleteFileTitle(deleting ?? '')} confirmLabel={T.deleteConfirm} danger onConfirm={() => void remove()} onCancel={() => setDeleting(null)}>
        {users.length === 0
          ? <p>{T.deleteFileUnused}</p>
          : (
            <>
              <p>{T.deleteFileUsed}</p>
              <ul className="usage-list">{users.map((u) => <li key={`${u.kind}:${u.id}`}>{u.kind === 'music' ? `Music “${u.id}”` : `Sound “${u.id}”`}</li>)}</ul>
              <p className="help">{T.deleteFileUnused}</p>
            </>
          )}
      </ConfirmDialog>
    </Card>
  )
}
