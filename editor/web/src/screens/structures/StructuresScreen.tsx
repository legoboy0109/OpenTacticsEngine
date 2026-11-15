// The Structures section: cities, forts, factories... (list + form, like Terrain).
import { useState } from 'react'
import { addStructure, deleteStructure, duplicateStructure, moveStructureKind, newStructureId, sizeOf } from '../../model/structures'
import { structureUsageLines } from '../../model/structuresRefs'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { ItemChecks } from '../items/ItemChecks'
import { EconomyChecks } from '../research/EconomyChecks'
import { StructureForm } from './StructureForm'
import { T } from './text'

export function StructuresScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game']}><StructuresGate itemId={itemId} /></Needs>
    </div>
  )
}

/** A damaged data/structures.json loads as "none"; editing would overwrite it, so refuse until it is fixed. */
function StructuresGate({ itemId }: { itemId?: string }) {
  const { doc, problems } = useProject()
  const bad = problems.filter((p) => p.file === 'data/structures.json')
  if (doc.structures === null && bad.length > 0) {
    return (
      <Banner tone="error" title={T.blockedTitle}>
        <p>{T.blockedBody}</p>
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
      </Banner>
    )
  }
  return <StructureList itemId={itemId} />
}

function StructureList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.structures ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'structures', id))
  const target = list.find((s) => s.id === deleting)

  return (
    <>
      <ItemChecks section="structures" />
      <EconomyChecks section="structures" />
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} note={T.intro} selectedId={itemId}
        items={list.map((s) => ({ id: s.id, label: s.name, sub: T.subtitle(sizeOf(s).w, sizeOf(s).h, s.glyph), swatch: s.color }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = list.find((s) => s.id === id)!
          const nid = newStructureId(doc, `${src.name} copy`)
          edit((d) => duplicateStructure(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((s) => s.id === id); edit((d) => moveStructureKind(d, i, i + delta)) }}>
        {(id) => <StructureForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newStructureId(doc, name); edit((d) => addStructure(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''}
        usages={target ? structureUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteStructure(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
