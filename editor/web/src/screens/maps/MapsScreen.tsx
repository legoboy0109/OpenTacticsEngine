// Stub: lists the maps and handles list-level actions. The map editor (paint
// tiles, place units, sides, resize) will replace MapDetail below; everything
// it needs about a map is doc.maps[id] and the pure edits in model/maps.ts.
import { useState } from 'react'
import { addMap, deleteMap, duplicateMap, mapRemovalBlock, mapSize, moveMap, newMapId, renameMap } from '../../model/maps'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

export function MapsScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game']}><MapsList itemId={itemId} /></Needs>
    </div>
  )
}

function MapsList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const maps = doc.game!.maps
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'maps', id))
  const target = maps.find((m) => m.id === deleting)
  const block = target ? mapRemovalBlock(doc) : null
  const sizeText = (id: string) => { const s = mapSize(doc, id); return s ? T.size(s.width, s.height) : T.unreadable }

  return (
    <>
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={maps.map((m) => ({ id: m.id, label: m.name, sub: sizeText(m.id) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = maps.find((m) => m.id === id)!
          const nid = newMapId(doc, `${src.name} copy`)
          edit((d) => duplicateMap(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = maps.findIndex((m) => m.id === id); edit((d) => moveMap(d, i, i + delta)) }}>
        {(id) => <MapDetail id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newMapId(doc, name); edit((d) => addMap(d, name, id)); setNaming(false); go(id) }} />
      <ConfirmDialog open={!!target} title={block ? T.cannotDelete : T.deleteTitle(target?.name ?? '')} confirmLabel="Delete" danger blocked={!!block}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) { edit((d) => deleteMap(d, target.id)); go() } setDeleting(null) }}>
        <p>{block ?? T.deleteBody}</p>
        {!block && <p className="help">{T.deleteUndo}</p>}
      </ConfirmDialog>
    </>
  )
}

/** Replace this with the map editor. */
function MapDetail({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const ref = doc.game!.maps.find((m) => m.id === id)
  const map = doc.maps[id]
  if (!ref) return null
  const sides = map?.sides?.length ?? 0
  return (
    <div className="stack">
      <Card>
        <Field label={T.name} help={T.nameHelp}>
          <TextInput value={ref.name} onChange={(v) => edit((d) => renameMap(d, id, v), `map:${id}:name`)} />
        </Field>
        <dl className="facts">
          <div><dt>{T.sizeLabel}</dt><dd>{map ? T.size(map.grid.width, map.grid.height) : T.unreadable}</dd></div>
          <div><dt>{T.sidesLabel}</dt><dd>{map ? sides : T.unreadable}</dd></div>
          <div><dt>{T.unitsLabel}</dt><dd>{map ? map.units?.length ?? 0 : T.unreadable}</dd></div>
        </dl>
      </Card>
      {map === null && <Banner tone="error" title={T.unreadableTitle}>{T.unreadableBody}</Banner>}
      <Banner tone="info" title={T.comingTitle}>{T.comingBody}</Banner>
    </div>
  )
}
