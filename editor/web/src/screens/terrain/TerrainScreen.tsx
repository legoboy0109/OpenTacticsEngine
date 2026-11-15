import { useState } from 'react'
import { findUsages } from '../../model/references'
import { addTerrain, deleteTerrain, duplicateTerrain, moveTerrain, newTerrainId, terrainRemovalBlock } from '../../model/terrain'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useStore } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { EconomyChecks } from '../research/EconomyChecks'
import { TerrainForm } from './TerrainForm'
import { T } from './text'

export function TerrainScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game', 'terrain']}><TerrainList itemId={itemId} /></Needs>
    </div>
  )
}

function TerrainList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const terrain = doc.terrain!
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'terrain', id))
  const target = terrain.find((t) => t.id === deleting)

  return (
    <>
      <EconomyChecks section="terrain" />
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={terrain.map((t) => ({ id: t.id, label: t.name, sub: T.subtitle(t.glyph), swatch: t.color }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = terrain.find((t) => t.id === id)!
          const nid = newTerrainId(doc, `${src.name} copy`)
          edit((d) => duplicateTerrain(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = terrain.findIndex((t) => t.id === id); edit((d) => moveTerrain(d, i, i + delta)) }}>
        {(id) => <TerrainForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newTerrainId(doc, name); edit((d) => addTerrain(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? findUsages(doc, 'terrain', target.id) : []}
        blockedReason={target ? terrainRemovalBlock(doc) : null} onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteTerrain(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
