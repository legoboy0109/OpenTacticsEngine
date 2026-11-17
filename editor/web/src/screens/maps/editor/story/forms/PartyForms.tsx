// "Switch to another party" and "Merge parties" (campaign scenes only; docs/EVENTS.md "Parties").
import { partiesOf, partyName } from '../../../../../model/parties'
import type { Command } from '../../../../../model/storyTypes'
import { MultiSelect } from '../../../../../ui/MultiSelect'
import { PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { useCommandEdit, type CommandRef } from './useCommandEdit'
import { PT } from './partyText'

export function SwitchPartyForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const options = partiesOf(doc.campaign).map((p) => ({ value: p.id, label: partyName(doc.campaign, p.id) }))
  return (
    <div className="stack tight">
      <PickSelect label={PT.switchWhich} value={cmd.switchParty ?? ''} empty={PT.switchNone} options={options} onChange={(v) => patch({ switchParty: v })} />
      <p className="help">{PT.switchHelp}</p>
    </div>
  )
}

export function MergePartiesForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const all = partiesOf(doc.campaign).map((p) => ({ value: p.id, label: partyName(doc.campaign, p.id) }))
  const into = cmd.into ?? ''
  const merged = Array.isArray(cmd.mergeParties) ? cmd.mergeParties : []
  return (
    <div className="stack tight">
      <MultiSelect label={PT.mergeFrom} options={all.map((o) => ({ ...o, disabled: o.value === into }))} value={merged} emptyText={PT.mergeFromNone}
        onChange={(l) => patch({ mergeParties: l }, 'from')} />
      <PickSelect label={PT.mergeInto} value={into} empty={PT.switchNone} options={all}
        onChange={(v) => patch({ into: v, mergeParties: merged.filter((id) => id !== v) })} />
      {into !== '' && merged.includes(into) && <p className="help" role="alert">{PT.mergeSelf}</p>}
      <p className="help">{PT.mergeHelp}</p>
    </div>
  )
}
