import { useState } from 'react'
import { findUsages } from '../../model/references'
import { addWeapon, deleteWeapon, duplicateWeapon, moveWeapon, newWeaponId } from '../../model/weapons'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useStore } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { EconomyChecks } from '../research/EconomyChecks'
import { WeaponForm } from './WeaponForm'
import { T } from './text'

export function WeaponsScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game', 'weapons']}><WeaponsList itemId={itemId} /></Needs>
    </div>
  )
}

function WeaponsList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const weapons = doc.weapons!
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'weapons', id))
  const target = weapons.find((w) => w.id === deleting)

  return (
    <>
      <EconomyChecks section="weapons" />
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={weapons.map((w) => ({ id: w.id, label: w.name, sub: T.rangeText(w.range) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = weapons.find((w) => w.id === id)!
          const nid = newWeaponId(doc, `${src.name} copy`)
          edit((d) => duplicateWeapon(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = weapons.findIndex((w) => w.id === id); edit((d) => moveWeapon(d, i, i + delta)) }}>
        {(id) => <WeaponForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newWeaponId(doc, name); edit((d) => addWeapon(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? findUsages(doc, 'weapon', target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteWeapon(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
