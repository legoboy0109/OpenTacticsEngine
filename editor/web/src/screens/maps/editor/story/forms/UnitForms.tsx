// Commands about units that are already there: remove, move, change side.
import type { Command } from '../../../../../model/storyTypes'
import { AreaField } from '../AreaField'
import { FilterPicker, SidePick, UnitPick } from '../pickers'
import { S } from '../text'
import { MoveExtras } from './SceneForms'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function RemoveUnitForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <FilterPicker label={S.removeWho} value={cmd.removeUnit} onChange={(f) => patch({ removeUnit: f ?? {} })} />
      <p className="help">{S.removeHelp}</p>
    </div>
  )
}

export function MoveUnitForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <UnitPick label={S.moveWho} value={cmd.moveUnit ?? ''} onChange={(v) => patch({ moveUnit: v })} />
      <AreaField label={S.moveTo} mode="square" value={{ x: cmd.x ?? 0, y: cmd.y ?? 0, w: 1, h: 1 }} help={S.moveHelp} onChange={(r) => patch({ x: r.x, y: r.y })} />
      <MoveExtras cmd={cmd} at={at} />
    </div>
  )
}

export function ChangeSideForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <FilterPicker label={S.changeWho} value={cmd.changeSide} onChange={(f) => patch({ changeSide: f ?? {} })} />
      <SidePick label={S.changeTo} help={S.changeHelp} value={cmd.side ?? ''} onChange={(v) => patch({ side: v })} />
    </div>
  )
}
