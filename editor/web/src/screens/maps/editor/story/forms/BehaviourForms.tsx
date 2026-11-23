// "Change units' behaviour" and "Wake a group" (docs/AI.md, docs/EVENTS.md "Computer behaviour"), map events only. The behaviour fields are the ones of a placed unit
// (panels/BehaviourEditor.tsx); the unit filter needs the loader's `{}` for "every unit", so it is never left out.
import { groupNames } from '../../../../../model/behaviour'
import { asFilter, type Command } from '../../../../../model/storyTypes'
import { BehaviourEditor } from '../../panels/BehaviourEditor'
import { B } from '../../panels/behaviourText'
import { FilterPicker, PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function SetBehaviourForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc, map } = useStory()
  return (
    <div className="stack tight" data-testid="set-behaviour-form">
      <FilterPicker actor label={B.setBehaviourWho} value={asFilter(cmd.units) ?? {}} onChange={(f) => patch({ units: f ?? {} })} />
      {/* A block that is the default is written as "advance": the command must hold something. */}
      <BehaviourEditor value={cmd.setBehaviour} doc={doc} map={map} id="set-behaviour"
        onChange={(next, k) => patch({ setBehaviour: next ?? 'advance' }, k && `behaviour:${k}`)} />
      <p className="help">{B.setBehaviourHelp}</p>
    </div>
  )
}

export function WakeGroupForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { map } = useStory()
  const groups = groupNames(map)
  const current = cmd.wakeGroup ?? ''
  const options = (current && !groups.includes(current) ? [current, ...groups] : groups).map((g) => ({ value: g, label: g }))
  return (
    <div className="stack tight" data-testid="wake-group-form">
      <PickSelect label={B.wakeGroup} help={B.wakeGroupHelp} value={current} options={options} empty={B.noGroups} onChange={(v) => patch({ wakeGroup: v })} />
    </div>
  )
}
