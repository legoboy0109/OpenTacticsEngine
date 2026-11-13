// Image files in the game's folder that no image entry uses. Deleting one is for good (Undo never touches files).
import { useState } from 'react'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { addSheet, newSheetId, sheetsOf } from '../../model/graphics'
import { unusedFiles } from '../../model/graphicsUsage'
import { guessDivision } from '../../model/gridGuess'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { T } from './text'
import type { ArtNotice } from './useImageImport'

export function UnusedFiles({ say }: { say: (tone: ArtNotice['tone'], text: string) => void }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const { files } = useArtState()
  const [deleting, setDeleting] = useState<string | null>(null)
  const unused = unusedFiles(doc, files)
  if (unused.length === 0) return null

  const use = (file: string) => {
    const f = files.find((x) => x.file === file)
    if (!f || f.error || !f.width) return
    const div = guessDivision(f.width, f.height, doc.game?.view?.tileSize ?? 24)
    edit((d) => addSheet(d, { file, cellWidth: div.cellWidth, cellHeight: div.cellHeight }, newSheetId(d, artBase(file))))
  }
  const remove = async () => {
    const file = deleting
    setDeleting(null)
    if (!file) return
    // The helper refuses while the SAVED game still uses the file, so make sure what is on screen is saved first.
    await store.flush()
    const res = await store.art.remove(file)
    if (res.ok) { say('success', T.unusedDeleted(file)); return }
    if (res.status === 409 && res.usedBy?.length) {
      const names = res.usedBy.map((id) => sheetsOf(doc).find((s) => s.id === id)?.name ?? id)
      say('error', T.unusedStillUsed(file, names.join(', ')))
    } else say('error', T.unusedFailed(file, res.error))
  }

  return (
    <Card title={T.unusedTitle} help={T.unusedHelp} id="unused-files">
      <ul className="unused-list">
        {unused.map((file) => {
          const f = files.find((x) => x.file === file)
          return (
            <li key={file}>
              {f && !f.error ? <img className="art-thumb" src={store.art.url(file)} alt="" /> : <span className="art-thumb missing" aria-hidden="true" />}
              <span className="unused-name">
                <strong>{file}</strong>
                <span className="help">{f?.error ? T.unreadable : T.size(f?.width ?? 0, f?.height ?? 0)}</span>
              </span>
              <span className="row">
                {f && !f.error && <button type="button" className="btn sm" aria-label={`${T.unusedUse} ${file}`} onClick={() => use(file)}>{T.unusedUse}</button>}
                <button type="button" className="btn sm danger" aria-label={`${T.unusedDelete} ${file}`} onClick={() => setDeleting(file)}>{T.unusedDelete}</button>
              </span>
            </li>
          )
        })}
      </ul>
      <ConfirmDialog open={!!deleting} title={T.unusedDeleteTitle(deleting ?? '')} confirmLabel="Delete" danger onConfirm={() => void remove()} onCancel={() => setDeleting(null)}>
        <p>{T.unusedDeleteBody}</p>
      </ConfirmDialog>
    </Card>
  )
}
