// The Characters screen (docs/SRPG.md "Editor"): list on the left, form on the right, like Units. It sits inside the
// shared frame of the strategy-RPG screens, which handles "switched off" and "file could not be read".
import { useState } from 'react'
import { addCharacter, classWord, duplicateCharacter, moveCharacter } from '../../model/characters'
import { characterUsageLines, deleteCharacter, newCharacterId } from '../../model/srpgRefs'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { SrpgScreen } from '../srpg/SrpgScreen'
import { CharacterChecks } from './CharacterChecks'
import { CharacterForm } from './CharacterForm'
import { T } from './text'

export function CharactersScreen({ itemId }: { itemId?: string }) {
  return <SrpgScreen feature="characters"><CharactersList itemId={itemId} /></SrpgScreen>
}

function CharactersList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.characters ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'characters', id))
  const target = list.find((c) => c.id === deleting)
  const word = classWord(doc)
  const typeName = (id: string) => doc.units?.find((u) => u.id === id)?.name ?? id
  const noClasses = (doc.units?.length ?? 0) === 0

  return (
    <>
      <CharacterChecks />
      {noClasses && <Banner tone="info">{T.noClasses}</Banner>}
      <MasterDetail heading="h2" title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        items={list.map((c) => ({ id: c.id, label: c.name, sub: T.subtitle(typeName(c.type), c.level) }))}
        onSelect={go} onBack={() => go()} onNew={() => { if (!noClasses) setNaming(true) }}
        onDuplicate={(id) => {
          const nid = newCharacterId(doc, `${list.find((c) => c.id === id)!.name} copy`)
          edit((d) => duplicateCharacter(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((c) => c.id === id); edit((d) => moveCharacter(d, i, i + delta)) }}>
        {(id) => <CharacterForm id={id} word={word} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newCharacterId(doc, name); edit((d) => addCharacter(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? characterUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteCharacter(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
