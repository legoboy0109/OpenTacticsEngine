// The Maps section: the list (new, rename, duplicate, reorder, delete) and the map editor for the picked map.
import { useState } from 'react'
import { addMap, deleteMap, duplicateMap, mapRemovalBlock, mapSize, moveMap, newMapId } from '../../model/maps'
import { mapDeleteUsages } from '../../model/campaignRefs'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { ConfirmDialog } from '../../ui/Dialog'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { useStored } from '../../ui/hooks'
import { Icon } from '../../ui/Icon'
import { MapEditor } from './editor/MapEditor'
import { T } from './text'

export function MapsScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen wide">
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
  // The editor needs width: on a laptop the list starts hidden (a button brings it back).
  const [hidden, setHidden] = useStored('ote.mapListHidden', typeof window !== 'undefined' && window.innerWidth < 1500)
  const go = (id?: string) => navigate(projectPath(store.id, 'maps', id))
  const target = maps.find((m) => m.id === deleting)
  const block = target ? mapRemovalBlock(doc) : null
  // What the campaign (if the game has one) would lose with this map: its chapters' map is cleared, scenes are kept.
  const usages = target && !block ? mapDeleteUsages(doc, target.id) : []
  const sizeText = (id: string) => { const s = mapSize(doc, id); return s ? T.size(s.width, s.height) : T.unreadable }

  return (
    <>
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={maps.map((m) => ({ id: m.id, label: m.name, sub: sizeText(m.id) }))}
        listHidden={hidden}
        extraActions={(
          <button type="button" className="btn ghost sm" aria-pressed={!hidden} onClick={() => setHidden(!hidden)}>
            <Icon name="list" />{hidden ? T.showList : T.hideList}
          </button>
        )}
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
        {!block && usages.length > 0 && (
          <>
            <p>{T.deleteCampaign}</p>
            <ul className="usage-list" data-testid="map-delete-usages">{usages.map((u, i) => <li key={i}>{u.text}</li>)}</ul>
          </>
        )}
        {!block && <p className="help">{T.deleteUndo}</p>}
      </ConfirmDialog>
    </>
  )
}

function MapDetail({ id }: { id: string }) {
  const doc = useDoc()
  const ref = doc.game!.maps.find((m) => m.id === id)
  if (!ref) return null
  if (doc.maps[id] === null) {
    return <Banner tone="error" title={T.unreadableTitle}>{T.unreadableBody}</Banner>
  }
  return <Needs parts={['terrain', 'units']}><MapEditor id={id} /></Needs>
}
