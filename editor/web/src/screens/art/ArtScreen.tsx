// The Art section: the images of the game (list + details), the unit-base setting and unused files.
import { useCallback, useRef, useState, type DragEvent } from 'react'
import { artBase } from '../../model/artFiles'
import { findSheet, removeSheet, setUnitMarker, sheetsOf, unitMarkerOn } from '../../model/graphics'
import { sheetUsers } from '../../model/graphicsUsage'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { MasterDetail } from '../../ui/MasterDetail'
import { Toggle } from '../../ui/Toggle'
import { ImageDetails } from './ImageDetails'
import { T } from './text'
import { UnusedFiles } from './UnusedFiles'
import { useImageImport } from './useImageImport'

export function ArtScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game']}><ArtGate itemId={itemId} /></Needs>
    </div>
  )
}

/** A damaged data/graphics.json loads as "no art"; editing would overwrite it, so refuse until it is fixed. */
function ArtGate({ itemId }: { itemId?: string }) {
  const { doc, problems } = useProject()
  if (doc.graphics === null && problems.some((p) => p.file === 'data/graphics.json')) {
    return (
      <Banner tone="error" title={T.blockedTitle}>
        <p>{T.blockedBody}</p>
        {problems.filter((p) => p.file === 'data/graphics.json').map((p, i) => <p key={i}>{p.message}</p>)}
      </Banner>
    )
  }
  return <ArtBody itemId={itemId} />
}

const hasFiles = (e: DragEvent) => Array.from(e.dataTransfer?.types ?? []).includes('Files')

function ArtBody({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const sheets = sheetsOf(doc)
  const input = useRef<HTMLInputElement>(null)
  const [dragging, setDragging] = useState(false)
  const [removing, setRemoving] = useState<string | null>(null)
  const go = useCallback((id?: string) => navigate(projectPath(store.id, 'art', id)), [store])
  const { addFiles, busy, notices, say, dismiss } = useImageImport(go)
  const target = removing ? findSheet(doc, removing) : undefined
  const users = target ? sheetUsers(doc, target.id) : []

  const drop = (e: DragEvent) => {
    if (!hasFiles(e)) return
    e.preventDefault()
    setDragging(false)
    void addFiles(Array.from(e.dataTransfer.files))
  }
  const doRemove = (id: string) => { edit((d) => removeSheet(d, id)); if (itemId === id) go(); setRemoving(null) }

  return (
    <div className="art-screen" data-dragging={dragging || undefined}
      onDragOver={(e) => { if (hasFiles(e)) { e.preventDefault(); setDragging(true) } }}
      onDragLeave={(e) => { if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setDragging(false) }}
      onDrop={drop}>
      {dragging && <div className="drop-veil" aria-hidden="true">{T.dropHere}</div>}
      <input ref={input} type="file" accept="image/*" multiple hidden aria-label={T.add} data-testid="add-image-input"
        onChange={(e) => { void addFiles(Array.from(e.target.files ?? [])); e.target.value = '' }} />
      <div className="stack">
        {notices.length > 0 && (
          <div className="stack tight" aria-live="polite">
            {notices.map((n) => <Banner key={n.id} tone={n.tone} onDismiss={() => dismiss(n.id)}><p>{n.text}</p></Banner>)}
          </div>
        )}
        {sheets.length === 0 && <p className="help">{T.emptyHelp}</p>}
        {busy > 0 && <p className="help" role="status">{T.adding(busy)}</p>}
        <MasterDetail title={T.title} noun={T.noun} newLabel={T.add} deleteLabel={T.remove} emptyText={T.empty} selectedId={itemId}
          items={sheets.map((s) => ({
            id: s.id, label: s.name || artBase(s.file),
            sub: store.art.fileInfo(s.file)?.width ? `${store.art.fileInfo(s.file)!.width} × ${store.art.fileInfo(s.file)!.height}` : undefined,
            thumb: <img className="art-thumb" src={store.art.url(s.file)} alt="" />,
          }))}
          onSelect={go} onBack={() => go()} onNew={() => input.current?.click()}
          onDelete={(id) => { if (sheetUsers(doc, id).length) setRemoving(id); else doRemove(id) }}>
          {(id) => <ImageDetails id={id} say={say} />}
        </MasterDetail>
        <Card title={T.settingsTitle}>
          <Toggle label={T.marker} help={T.markerHelp} checked={unitMarkerOn(doc)} onChange={(v) => edit((d) => setUnitMarker(d, v))} />
        </Card>
        <UnusedFiles say={say} />
      </div>
      <ConfirmDialog open={!!target} title={T.removeTitle(target?.name || (target ? artBase(target.file) : ''))} confirmLabel={T.removeConfirm} danger
        onConfirm={() => target && doRemove(target.id)} onCancel={() => setRemoving(null)}>
        <p>{T.removeUsedIntro}</p>
        <ul className="usage-list">{users.map((u) => <li key={`${u.kind}:${u.id}`}>{T.usedByItem(u.kind, u.name)}</li>)}</ul>
        <p className="help">{T.removeUnusedNote}</p>
        <p className="help">{T.removeUndo}</p>
      </ConfirmDialog>
    </div>
  )
}
