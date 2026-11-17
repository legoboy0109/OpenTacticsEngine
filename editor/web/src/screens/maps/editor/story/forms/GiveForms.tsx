// Commands that give or take something: an item to / from units, resources to a side.
import { asFilter, type Command } from '../../../../../model/storyTypes'
import { setNum } from '../../../../../model/util'
import { FieldGroup } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { Check } from '../../../../../ui/Toggle'
import { FilterPicker, ItemPick, SidePick, TechPick, UnitPick } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function GiveItemForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <ItemPick label={S.giveItem} value={cmd.giveItem ?? ''} onChange={(v) => patch({ giveItem: v })} />
      <FilterPicker label={S.giveItemTo} value={typeof cmd.to === 'object' ? cmd.to : {}} onChange={(f) => patch({ to: f ?? {} })} />
      <p className="help">{S.giveItemHelp}</p>
    </div>
  )
}

/** An item taken away from units (map events) or from one character of the party (campaign scenes). */
export function TakeItemForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { scene } = useStory()
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <ItemPick label={S.takeItem} value={cmd.takeItem ?? ''} onChange={(v) => patch({ takeItem: v })} />
      {scene
        ? <UnitPick label={S.takeItemFromScene} value={typeof cmd.from === 'string' ? cmd.from : ''} onChange={(v) => patch({ from: v })} />
        : <FilterPicker label={S.takeItemFrom} value={asFilter(cmd.from) ?? {}} onChange={(f) => patch({ from: f ?? {} })} />}
      <Check label={S.takeItemAll} help={S.takeItemAllHelp} checked={cmd.all === true} onChange={(v) => patch({ all: v ? true : undefined })} />
      <p className="help">{scene ? S.takeItemHelpScene : S.takeItemHelp}</p>
    </div>
  )
}

/** A technology a side has at once, for free (map events only: a campaign scene has no sides). */
export function GrantResearchForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <TechPick label={S.grantTech} value={cmd.grantResearch ?? ''} onChange={(v) => patch({ grantResearch: v })} />
      <SidePick label={S.grantSide} value={cmd.side ?? ''} onChange={(v) => patch({ side: v })} />
      <p className="help">{S.grantHelp}</p>
    </div>
  )
}

/** One number per resource of the game; an empty box leaves that resource out of the gift. */
export function GiveResourceForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const amounts = cmd.giveResource ?? {}
  const resources = doc.game?.resources ?? []
  const known = new Set(resources.map((r) => r.id))
  // Amounts of a resource that is gone stay visible so they can be cleared.
  const extra = Object.keys(amounts).filter((id) => !known.has(id) && id !== '').map((id) => ({ id, name: S.missing(id) }))
  const setAmount = (id: string, v: number | undefined) => patch({ giveResource: setNum(amounts, id, v) ?? {} }, `amount:${id}`)
  return (
    <div className="stack tight">
      <SidePick label={S.giveSide} value={cmd.side ?? ''} onChange={(v) => patch({ side: v })} />
      {resources.length === 0 && <p className="help">{S.noResources}</p>}
      <FieldGroup label={S.giveAmounts} help={S.giveAmountsHelp}>
        <table className="kv"><tbody>
          {[...resources, ...extra].map((r) => (
            <tr key={r.id}><th scope="row">{r.name}</th><td>
              <NumberInput aria-label={`Amount of ${r.name}`} value={amounts[r.id]} allowEmpty placeholder="0" onChange={(v) => setAmount(r.id, v)} />
            </td></tr>
          ))}
        </tbody></table>
      </FieldGroup>
    </div>
  )
}
