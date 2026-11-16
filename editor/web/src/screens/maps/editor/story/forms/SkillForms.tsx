// "Teach a skill" and "Make units forget a skill" (E2 of docs/SRPG.md). Both name a skill and which units; the loader needs
// the unit filter ({} = every unit), so "anyone" is written as {} and never left out.
import type { Command } from '../../../../../model/storyTypes'
import { FilterPicker, SkillPick } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function TeachSkillForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <SkillPick label={S.teachSkillPick} value={cmd.teachSkill ?? ''} onChange={(v) => patch({ teachSkill: v })} />
      <FilterPicker label={S.teachWho} value={typeof cmd.to === 'object' ? cmd.to : {}} onChange={(f) => patch({ to: f ?? {} })} />
      <p className="help">{S.teachHelp}</p>
    </div>
  )
}

export function ForgetSkillForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <SkillPick label={S.forgetSkillPick} value={cmd.forgetSkill ?? ''} onChange={(v) => patch({ forgetSkill: v })} />
      <FilterPicker label={S.forgetWho} value={cmd.from ?? {}} onChange={(f) => patch({ from: f ?? {} })} />
      <p className="help">{S.forgetHelp}</p>
    </div>
  )
}
