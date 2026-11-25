// "A guest joins the party": the matching guests become the player's ordinary units (model/guests.ts).
import type { Command } from '../../../../../model/storyTypes'
import { FilterPicker } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function GuestJoinsForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <FilterPicker actor label={S.guestJoinsWho} value={cmd.guestJoins} onChange={(f) => patch({ guestJoins: f ?? {} })} />
      <p className="help">{S.guestJoinsHelp}</p>
    </div>
  )
}
