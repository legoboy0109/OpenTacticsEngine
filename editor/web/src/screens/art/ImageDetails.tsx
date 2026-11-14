// The right-hand side of the Art screen: one image, how it is divided, who uses it, replace and remove.
import { useRef, useState } from 'react'
import { prepareImage, type Prepared } from '../../art/importImage'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { findSheet, setSheetName, setSheetSmooth } from '../../model/graphics'
import { outsideUsers, sheetUsers } from '../../model/graphicsUsage'
import { gridInfo } from '../../model/gridGuess'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Advanced, Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { Check } from '../../ui/Toggle'
import { PieceGrid } from '../../ui/picture/PieceGrid'
import { TextInput } from '../../ui/TextInput'
import { DivisionCard } from './DivisionCard'
import { T } from './text'
import { uploadFailure, type ArtNotice } from './useImageImport'

interface Props { id: string; say: (tone: ArtNotice['tone'], text: string) => void }

export function ImageDetails({ id, say }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const art = useArtState()
  const sheet = findSheet(doc, id)
  const picker = useRef<HTMLInputElement>(null)
  const [pending, setPending] = useState<(Extract<Prepared, { ok: true }> & { name: string }) | null>(null)
  if (!sheet) return null
  const info = art.files.find((f) => f.file === sheet.file)
  const img = store.art.image(sheet.file)
  const readable = !!info && !info.error && info.width > 0
  const width = readable ? info!.width : img.width, height = readable ? info!.height : img.height
  const label = sheet.name || artBase(sheet.file)
  const users = sheetUsers(doc, id)
  const grid = width ? gridInfo(width, height, sheet.cellWidth, sheet.cellHeight) : null
  const outside = grid ? outsideUsers(doc, id, grid.cols, grid.rows) : []

  const choose = async (f: File | undefined) => {
    if (!f) return
    const prep = await prepareImage(f)
    if (!prep.ok) { say('error', prep.message); return }
    setPending({ ...prep, name: f.name })
  }
  const replace = async () => {
    const p = pending
    setPending(null)
    if (!p) return
    const res = await store.art.upload(artBase(sheet.file), p.blob, true)
    if (!res.ok) say('error', uploadFailure(p.name, res)); else say('success', T.replaced)
  }
  const after = pending ? gridInfo(pending.width, pending.height, sheet.cellWidth, sheet.cellHeight) : null
  const pendingOutside = after ? outsideUsers(doc, id, after.cols, after.rows) : []

  return (
    <div className="stack">
      {art.loaded && !readable && (
        <Banner tone="error" title={T.missingTitle}>{T.missingBody(sheet.file)}</Banner>
      )}
      {outside.length > 0 && <Banner tone="warn" title={T.outsideTitle}>{T.outsideBody(outside.map((u) => u.name).join(', '))}</Banner>}

      <Card>
        <Field label={T.name} help={T.nameHelp}>
          <TextInput value={sheet.name ?? ''} placeholder={artBase(sheet.file)} onChange={(v) => edit((d) => setSheetName(d, id, v), `art:${id}:name`)} />
        </Field>
      </Card>

      {width > 0 && <DivisionCard sheet={sheet} width={width} height={height} />}

      <Card>
        <Check label={T.smooth} help={T.smoothHelp} checked={sheet.smooth === true} onChange={(v) => edit((d) => setSheetSmooth(d, id, v), `art:${id}:smooth`)} />
      </Card>

      {width > 0 && grid && (
        <Card title="Preview" help={T.size(width, height)}>
          {img.status === 'ready'
            ? <PieceGrid key={sheet.file} image={img.img} width={width} height={height} cellWidth={sheet.cellWidth} cellHeight={sheet.cellHeight} label={T.previewLabel(label)} />
            : <p className="help" role="status">{img.status === 'loading' ? T.loading : T.unreadable}</p>}
          <p className="help" data-testid="grid-summary">{T.across(grid.cols, grid.rows)}</p>
          {grid.cols === 0 || grid.rows === 0 ? <p className="error-text" role="alert">{T.tooSmall}</p>
            : (grid.extraRight > 0 || grid.extraBottom > 0) && <p className="help" data-testid="leftover">{T.leftover(grid.extraRight, grid.extraBottom)}</p>}
        </Card>
      )}

      <Card title={T.usedBy}>
        {users.length === 0
          ? <p className="help">{T.usedByNone}</p>
          : (
            <ul className="usage-list" aria-label={T.usedBy}>
              {users.map((u) => (
                <li key={`${u.kind}:${u.id}`}>
                  <Link to={projectPath(store.id, u.kind, u.id)}>{T.usedByItem(u.kind, u.name)}</Link>
                </li>
              ))}
            </ul>
          )}
      </Card>

      <Card>
        <div className="row">
          <button type="button" className="btn" onClick={() => picker.current?.click()}>{T.replace}</button>
          <input ref={picker} type="file" accept="image/*" hidden aria-label={T.replace} data-testid="replace-input"
            onChange={(e) => { void choose(e.target.files?.[0]); e.target.value = '' }} />
        </div>
        <p className="help">{T.replaceHelp}</p>
      </Card>

      <Advanced label={T.advanced}>
        <div className="stack">
          <Field label={T.internalName} help={T.internalNameHelp}><TextInput value={sheet.id} readOnly mono /></Field>
          <Field label={T.fileName} help={T.fileNameHelp}><TextInput value={sheet.file} readOnly mono /></Field>
        </div>
      </Advanced>

      <ConfirmDialog open={!!pending} title={T.replaceTitle} confirmLabel={T.replaceConfirm} danger onConfirm={() => void replace()} onCancel={() => setPending(null)}>
        {pending && <p>{T.replaceBody(label, pending.width, pending.height)}</p>}
        {pendingOutside.length > 0 && <p className="attention" role="alert">{T.replaceOutside(pendingOutside.map((u) => u.name).join(', '))}</p>}
      </ConfirmDialog>
    </div>
  )
}
