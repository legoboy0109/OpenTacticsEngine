// Resources and movement types: two simple named lists with delete confirmation.
import { useState } from 'react'
import { findUsages } from '../../model/references'
import {
  addMoveType, addResource, fallbackMoveType, moveMoveType, moveResource, moveTypeRemovalBlock,
  newMoveTypeId, newResourceId, removeMoveType, removeResource, renameMoveType, renameResource,
} from '../../model/rulesLists'
import { useDoc, useEdit } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { NamedListCard } from './NamedListCard'
import { T } from './text'

export function ResourcesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const items = doc.game!.resources ?? []
  const [deleting, setDeleting] = useState<string | null>(null)
  const target = items.find((r) => r.id === deleting)
  return (
    <>
      <NamedListCard title={T.resources} help={T.resourcesHelp} noun={T.resourceNoun} items={items} emptyText={T.resourcesEmpty}
        onAdd={(name) => { const id = newResourceId(doc, name); edit((d) => addResource(d, name, id)) }}
        onRename={(id, name) => edit((d) => renameResource(d, id, name), `resource:${id}`)}
        onMove={(a, b) => edit((d) => moveResource(d, a, b))}
        onDelete={setDeleting} />
      <DeleteConfirm open={!!target} what={target ? T.deleteResourceTitle(target.name) : ''} usages={target ? findUsages(doc, 'resource', target.id) : []}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) edit((d) => removeResource(d, target.id)); setDeleting(null) }} />
    </>
  )
}

export function MoveTypesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const items = doc.game!.moveTypes
  const [deleting, setDeleting] = useState<string | null>(null)
  const target = items.find((m) => m.id === deleting)
  const fallback = target ? fallbackMoveType(doc, target.id) : undefined
  const usages = target ? findUsages(doc, 'moveType', target.id) : []
  return (
    <>
      <NamedListCard title={T.moveTypes} help={T.moveTypesHelp} noun={T.moveTypeNoun} items={items}
        onAdd={(name) => { const id = newMoveTypeId(doc, name); edit((d) => addMoveType(d, name, id)) }}
        onRename={(id, name) => edit((d) => renameMoveType(d, id, name), `movetype:${id}`)}
        onMove={(a, b) => edit((d) => moveMoveType(d, a, b))}
        onDelete={setDeleting} />
      <DeleteConfirm open={!!target} what={target ? T.deleteMoveTypeTitle(target.name) : ''} usages={usages}
        notes={fallback && usages.some((u) => u.section === 'units') ? [T.moveTypeFallback(fallback)] : []}
        blockedReason={target ? moveTypeRemovalBlock(doc) : null}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) edit((d) => removeMoveType(d, target.id)); setDeleting(null) }} />
    </>
  )
}
