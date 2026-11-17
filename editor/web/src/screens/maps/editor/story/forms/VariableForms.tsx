// "Set a variable" and "Add to a variable": the number is typed, or read from a unit (its level, hit points, most hit
// points or one of its stats), so sums and bars can be built with the ordinary "a variable is …" condition.
import { numberSource, type Command, type NumberSource } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { PickSelect, UnitPick, VariablePick } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const ALL_READINGS: Record<string, undefined> = { level: undefined, hp: undefined, maxHp: undefined, stat: undefined, of: undefined }

/** The "number" half of both forms. `key` is the written number's key: `to` (set) or `by` (add). */
function NumberFields({ cmd, at, numberKey: key, label, help, fallback }: { cmd: Command; at: CommandRef; numberKey: 'to' | 'by'; label: string; help?: string; fallback: number }) {
  const { doc, units, scene } = useStory()
  const { patch } = useCommandEdit(at)
  const source = numberSource(cmd)
  const stats = doc.game?.stats ?? []
  // A scene's units have no hit points between battles, so "now" is not offered there (the loader refuses it).
  const sources = (['number', 'level', 'hp', 'maxHp', 'stat'] as NumberSource[]).filter((s) => !(scene && s === 'hp'))
  const choose = (v: string) => {
    if (v === 'number') return patch({ ...ALL_READINGS, [key]: fallback })
    const first = units[0]?.id ?? ''
    if (v === 'stat') return patch({ ...ALL_READINGS, [key]: undefined, stat: stats[0]?.id ?? '', of: first })
    return patch({ ...ALL_READINGS, [key]: undefined, [v]: first })
  }
  const unitKey = source === 'stat' ? 'of' : source
  const unitValue = source === 'number' ? '' : String(cmd[unitKey as 'of' | 'level' | 'hp' | 'maxHp'] ?? '')
  return (
    <>
      <PickSelect label={S.numberFrom} value={source} onChange={choose}
        options={sources.map((s) => ({ value: s, label: S.numberSources[s] }))} />
      {source === 'number' && (
        <Field label={label} help={help}>
          <NumberInput value={typeof cmd[key] === 'number' ? (cmd[key] as number) : fallback} onChange={(v) => { if (v !== undefined) patch({ [key]: v }, key) }} />
        </Field>
      )}
      {source === 'stat' && (stats.length === 0
        ? <p className="help">{S.readNoStats}</p>
        : <PickSelect label={S.readStat} value={cmd.stat ?? ''} onChange={(v) => patch({ stat: v })} options={stats.map((s) => ({ value: s.id, label: s.name || s.id }))} />)}
      {source !== 'number' && (
        <>
          <UnitPick label={S.readUnit} value={unitValue} onChange={(v) => patch({ [unitKey]: v })} />
          <p className="help">{scene ? S.readHelpScene : S.readHelp}</p>
        </>
      )}
    </>
  )
}

export function SetVariableForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <VariablePick label={S.setVariable} value={cmd.setVariable ?? ''} onChange={(v) => patch({ setVariable: v })} />
      <NumberFields cmd={cmd} at={at} numberKey="to" label={S.setVariableTo} fallback={0} />
    </div>
  )
}

export function AddVariableForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <VariablePick label={S.addVariable} value={cmd.addVariable ?? ''} onChange={(v) => patch({ addVariable: v })} />
      <NumberFields cmd={cmd} at={at} numberKey="by" label={S.addVariableBy} help={S.addVariableHelp} fallback={1} />
    </div>
  )
}
