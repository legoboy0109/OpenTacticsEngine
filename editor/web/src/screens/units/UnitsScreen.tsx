import { useState } from 'react'
import { findUsages } from '../../model/references'
import { addUnit, deleteUnit, duplicateUnit, moveUnit, newUnitId, unitRemovalBlock } from '../../model/units'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useStore } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { UnitForm } from './UnitForm'
import { T } from './text'

export function UnitsScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game', 'units']}><UnitsList itemId={itemId} /></Needs>
    </div>
  )
}

function UnitsList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const units = doc.units!
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'units', id))
  const target = units.find((u) => u.id === deleting)
  const moveName = (id?: string) => doc.game!.moveTypes.find((m) => m.id === id)?.name ?? id ?? ''

  return (
    <>
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={units.map((u) => ({ id: u.id, label: u.name, sub: T.subtitle(u.glyph, moveName(u.moveType)) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = units.find((u) => u.id === id)!
          const nid = newUnitId(doc, `${src.name} copy`)
          edit((d) => duplicateUnit(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = units.findIndex((u) => u.id === id); edit((d) => moveUnit(d, i, i + delta)) }}>
        {(id) => <UnitForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newUnitId(doc, name); edit((d) => addUnit(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? findUsages(doc, 'unit', target.id) : []}
        blockedReason={target ? unitRemovalBlock(doc) : null} onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteUnit(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
