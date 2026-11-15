// Story switches and variables (game.json "switches" / "variables"): two named lists, like resources, whose delete
// asks first and lists the events that use the item (they lose the command or condition, in the same undo step).
import { useState } from 'react'
import { addStoryItem, moveStoryItem, newStoryId, removeStoryItem, renameStoryItem, storyItemUsages, storyListOf, type StoryList } from '../../model/storyLists'
import { useDoc, useEdit } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { NamedListCard } from './NamedListCard'
import { T } from './text'

function StoryListCard({ listKey }: { listKey: StoryList }) {
  const doc = useDoc()
  const edit = useEdit()
  const sw = listKey === 'switches'
  const items = storyListOf(doc, listKey)
  const [deleting, setDeleting] = useState<string | null>(null)
  const target = items.find((x) => x.id === deleting)
  return (
    <>
      <NamedListCard title={sw ? T.switches : T.variables} help={sw ? T.switchesHelp : T.variablesHelp} noun={sw ? T.switchNoun : T.variableNoun}
        items={items} emptyText={sw ? T.switchesEmpty : T.variablesEmpty}
        onAdd={(name) => { const id = newStoryId(doc, listKey, name); edit((d) => addStoryItem(d, listKey, name, id)) }}
        onRename={(id, name) => edit((d) => renameStoryItem(d, listKey, id, name), `${listKey}:${id}`)}
        onMove={(a, b) => edit((d) => moveStoryItem(d, listKey, a, b))}
        onDelete={setDeleting} />
      <DeleteConfirm open={!!target} what={target ? (sw ? T.deleteSwitchTitle(target.name) : T.deleteVariableTitle(target.name)) : ''}
        usages={target ? storyItemUsages(doc, listKey, target.id) : []}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) edit((d) => removeStoryItem(d, listKey, target.id)); setDeleting(null) }} />
    </>
  )
}

export const SwitchesCard = () => <StoryListCard listKey="switches" />
export const VariablesCard = () => <StoryListCard listKey="variables" />
