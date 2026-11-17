// The Items section: potions, carried weapons, accessories (list + form, like Weapons).
import { useState } from 'react'
import { addItem, deleteItem, duplicateItem, moveItemInList, newItemId } from '../../model/items'
import { itemUsageLines } from '../../model/itemRefs'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { AudioChecks } from '../sound/AudioChecks'
import { PlaceChecks } from '../structures/PlaceChecks'
import { ItemChecks } from './ItemChecks'
import { ItemForm } from './ItemForm'
import { T } from './text'

export function ItemsScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game']}><ItemsGate itemId={itemId} /></Needs>
    </div>
  )
}

/** A damaged data/items.json loads as "none"; editing would overwrite it, so refuse until it is fixed. */
function ItemsGate({ itemId }: { itemId?: string }) {
  const { doc, problems } = useProject()
  const bad = problems.filter((p) => p.file === 'data/items.json')
  if (doc.items === null && bad.length > 0) {
    return (
      <Banner tone="error" title="Items can't be opened: data/items.json could not be read.">
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
        <p>Nothing here will be changed or saved. Fix or restore that file (it is in the game folder), then reload this page.</p>
      </Banner>
    )
  }
  return <ItemList itemId={itemId} />
}

function ItemList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.items ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'items', id))
  const target = list.find((i) => i.id === deleting)

  return (
    <>
      <ItemChecks section="items" />
      <PlaceChecks section="items" />
      <AudioChecks section="items" />
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} note={T.intro} selectedId={itemId}
        items={list.map((i) => ({ id: i.id, label: i.name, sub: T.subtitle(i.kind, i.uses) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = list.find((i) => i.id === id)!
          const nid = newItemId(doc, `${src.name} copy`)
          edit((d) => duplicateItem(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((x) => x.id === id); edit((d) => moveItemInList(d, i, i + delta)) }}>
        {(id) => <ItemForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newItemId(doc, name); edit((d) => addItem(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? itemUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteItem(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
