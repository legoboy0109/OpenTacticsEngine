// Story commands: switches, a team winning or losing, ending a talking unit's turn, moving the view.
import { rectOf, type Command } from '../../../../../model/storyTypes'
import { AreaField } from '../AreaField'
import { PickSelect, SwitchPick, TeamPick, UnitPick } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function SetSwitchForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <SwitchPick label={S.setSwitch} value={cmd.setSwitch ?? ''} onChange={(v) => patch({ setSwitch: v })} />
      <PickSelect label={S.setSwitchTo} value={cmd.to === false ? 'off' : 'on'} options={[{ value: 'on', label: S.on }, { value: 'off', label: S.off }]}
        onChange={(v) => patch({ to: v === 'on' })} />
    </div>
  )
}

export function WinLoseForm({ cmd, at, win }: { cmd: Command; at: CommandRef; win: boolean }) {
  const { patch } = useCommandEdit(at)
  const key = win ? 'win' : 'lose'
  return <TeamPick label={win ? S.winTeam : S.loseTeam} help={win ? S.winHelp : S.loseHelp} value={cmd[key] ?? ''} onChange={(v) => patch({ [key]: v })} />
}

export function EndTurnForm() {
  return <p className="help">{S.endTurnHelp}</p>
}

/** The view goes to a unit or to a square. */
export function FocusForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const unit = typeof cmd.focus === 'string'
  return (
    <div className="stack tight">
      <PickSelect label={S.focusWhat} value={unit ? 'unit' : 'square'} options={[{ value: 'square', label: S.focusSquare }, { value: 'unit', label: S.focusUnit }]}
        onChange={(v) => patch({ focus: v === 'unit' ? '' : { x: 0, y: 0 } })} />
      {unit
        ? <UnitPick label={S.focusUnit} value={cmd.focus as string} onChange={(v) => patch({ focus: v })} />
        : <AreaField label={S.focusSquare} mode="square" value={{ ...rectOf(cmd.focus), w: 1, h: 1 }} onChange={(r) => patch({ focus: { x: r.x, y: r.y } })} />}
    </div>
  )
}
