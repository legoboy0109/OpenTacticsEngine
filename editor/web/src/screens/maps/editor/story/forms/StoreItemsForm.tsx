// "Send a unit's items to storage" (docs/EVENTS.md "Sending a unit's items to storage"): whose items, and which items stay with them.
// In a map event the units are a filter; in a campaign scene one character of the party.
import { storeItemsProblems } from '../../../../../model/storyExtras'
import { asFilter, type Command } from '../../../../../model/storyTypes'
import { MultiSelect } from '../../../../../ui/MultiSelect'
import { FilterPicker, UnitPick } from '../pickers'
import { useStory } from '../StoryContext'
import { useCommandEdit, type CommandRef } from './useCommandEdit'
import { EX } from './extrasText'

export function StoreItemsForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { scene, doc } = useStory()
  const { patch } = useCommandEdit(at)
  const keep = Array.isArray(cmd.keep) ? cmd.keep : []
  const items = (doc.items ?? []).map((i) => ({ value: i.id, label: i.name }))
  const problems = storeItemsProblems(doc, cmd)
  return (
    <div className="stack tight">
      {scene
        ? <UnitPick label={EX.storeWhoScene} value={typeof cmd.storeItems === 'string' ? cmd.storeItems : ''} onChange={(v) => patch({ storeItems: v })} />
        : <FilterPicker label={EX.storeWho} value={asFilter(cmd.storeItems) ?? {}} onChange={(f) => patch({ storeItems: f ?? {} })} />}
      <MultiSelect label={EX.storeKeep} options={items} value={keep} emptyText={EX.storeKeepNone}
        onChange={(l) => patch({ keep: l.length ? l : undefined }, 'keep')} />
      {problems.length > 0 && <p className="help" role="alert" data-testid="store-items-problem">{EX.storeNoStorage}</p>}
      <p className="help">{scene ? EX.storeHelpScene : EX.storeHelp}</p>
    </div>
  )
}
