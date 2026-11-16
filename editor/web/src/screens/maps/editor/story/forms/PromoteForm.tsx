// The "Promote a unit" command (E3 of docs/SRPG.md): a named unit becomes another unit type. `to` is any unit type; left out, the
// unit's own type must have exactly one promotion (the loader says so otherwise).
import type { Command } from '../../../../../model/storyTypes'
import { useStory } from '../StoryContext'
import { PickSelect, UnitPick } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const OWN = 'own'

export function PromoteForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc } = useStory()
  const to = typeof cmd.to === 'string' && cmd.to !== '' ? cmd.to : OWN
  return (
    <div className="stack tight">
      <UnitPick label={S.promoteWho} value={cmd.promote ?? ''} onChange={(v) => patch({ promote: v })} />
      <PickSelect label={S.promoteTo} value={to} help={S.promoteHelp}
        options={[{ value: OWN, label: S.promoteOwn }, ...(doc.units ?? []).map((u) => ({ value: u.id, label: u.name || u.id }))]}
        onChange={(v) => patch({ to: v === OWN ? undefined : v })} />
    </div>
  )
}
