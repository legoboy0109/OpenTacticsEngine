// The small form a command opens when it is selected in the list: one form per kind of command.
import { commandKind, type Command } from '../../../../model/storyTypes'
import { ObjectiveEditor } from './ObjectiveEditor'
import { tidyObjectives } from '../../../../model/storyEdit'
import { AddUnitForm } from './forms/AddUnitForm'
import { GiveItemForm, GiveResourceForm } from './forms/GiveForms'
import { SayForm } from './forms/SayForm'
import { AddVariableForm, EndTurnForm, FocusForm, SetSwitchForm, SetVariableForm, WinLoseForm } from './forms/StoryForms'
import { ChangeSideForm, MoveUnitForm, RemoveUnitForm } from './forms/UnitForms'
import { useCommandEdit, type CommandRef } from './forms/useCommandEdit'
import { ConditionList } from './ConditionList'
import { S } from './text'

export function CommandForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  switch (commandKind(cmd)) {
    case 'say': return <SayForm cmd={cmd} at={at} />
    case 'addUnit': return <AddUnitForm cmd={cmd} at={at} />
    case 'removeUnit': return <RemoveUnitForm cmd={cmd} at={at} />
    case 'moveUnit': return <MoveUnitForm cmd={cmd} at={at} />
    case 'changeSide': return <ChangeSideForm cmd={cmd} at={at} />
    case 'giveItem': return <GiveItemForm cmd={cmd} at={at} />
    case 'giveResource': return <GiveResourceForm cmd={cmd} at={at} />
    case 'setSwitch': return <SetSwitchForm cmd={cmd} at={at} />
    case 'setVariable': return <SetVariableForm cmd={cmd} at={at} />
    case 'addVariable': return <AddVariableForm cmd={cmd} at={at} />
    case 'objective':
      return (
        <div className="stack tight">
          <p className="help">{S.objectiveCommandHelp}</p>
          <ObjectiveEditor name="command" value={cmd.objective ?? {}} onChange={(o, k) => patch({ objective: tidyObjectives(o) }, k && `objective:${k}`)} />
        </div>
      )
    case 'win': return <WinLoseForm cmd={cmd} at={at} win />
    case 'lose': return <WinLoseForm cmd={cmd} at={at} win={false} />
    case 'if':
      return (
        <div className="stack tight">
          <p className="help">{S.ifConditions}</p>
          <ConditionList name={`If ${at.index + 1}`} conditions={cmd.if ?? []} onChange={(list, k) => patch({ if: list }, k && `if:${k}`)} />
        </div>
      )
    case 'endTurn': return <EndTurnForm />
    case 'focus': return <FocusForm cmd={cmd} at={at} />
    default: return <p className="help">This command is not one the editor knows. It is kept as it is.</p>
  }
}
