// "Add a unit": the same fields as a placed unit (type, side, square, name, level, leader, story name, items).
import { followUnitRename } from '../../../../../model/campaignRefs'
import { clone } from '../../../../../model/doc'
import { editMap } from '../../../../../model/mapCtx'
import { inventorySize } from '../../../../../model/itemRules'
import { idProblem, suggestUnitId, takenUnitIds } from '../../../../../model/storyIds'
import { setAddedUnitId } from '../../../../../model/storyEdit'
import { storyIdUsages } from '../../../../../model/storyRefs'
import type { Command } from '../../../../../model/storyTypes'
import type { MapUnit } from '../../../../../model/types'
import { patchObj } from '../../../../../model/util'
import { Field } from '../../../../../ui/Field'
import { ItemListEditor } from '../../../../../ui/ItemListEditor'
import { NumberInput } from '../../../../../ui/NumberInput'
import { TextInput } from '../../../../../ui/TextInput'
import { Check, Toggle } from '../../../../../ui/Toggle'
import { UnitSkills } from '../../panels/UnitSkills'
import { AreaField } from '../AreaField'
import { IdInput } from '../../../../../ui/IdInput'
import { SidePick, TypePick } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function AddUnitForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc, map, mapId, applyDoc } = useStory()
  const { patch } = useCommandEdit(at)
  const u: MapUnit = cmd.addUnit ?? { type: '', side: '', x: 0, y: 0 }
  const set = (p: Record<string, unknown>, k?: string) => patch({ addUnit: patchObj(u, p) }, k)
  const base = doc.units?.find((t) => t.id === u.type)
  // A document edit (not just the map) so the campaign's scenes that named the old story name follow the rename.
  const setId = (id: string) => applyDoc((d) => {
    const next = editMap(d, mapId, (m) => setAddedUnitId(m, at.eventId, at.path, at.index, id))
    return u.id ? followUnitRename(d, next, u.id, id) : next
  }, `cmd:${at.eventId}:${JSON.stringify(at.path)}:${at.index}:id`)
  const validate = (text: string) => {
    if (text === '') return u.id && storyIdUsages(doc, mapId, u.id).length ? S.nameInUse : null
    return idProblem(text, takenUnitIds(map, u))
  }
  return (
    <div className="stack tight" data-testid="add-unit-form">
      <TypePick label={S.unitType} value={u.type ?? ''} onChange={(v) => set({ type: v })} />
      <SidePick label={S.unitSide} value={u.side} onChange={(v) => set({ side: v })} />
      <AreaField label={S.unitSquare} mode="square" value={{ x: u.x, y: u.y, w: 1, h: 1 }} help={S.unitSquareHelp} onChange={(r) => set({ x: r.x, y: r.y })} />
      <Field label={S.unitName} help={S.unitNameHelp}>
        <TextInput value={u.name ?? ''} onChange={(v) => set({ name: v.trim() ? v : undefined }, 'name')} />
      </Field>
      {doc.game?.rules?.progression?.enabled && (
        <Field label={S.unitLevel}>
          <NumberInput value={u.level} min={1} allowEmpty placeholder="1" onChange={(v) => set({ level: v }, 'level')} />
        </Field>
      )}
      <UnitSkills doc={doc} value={u.skills} onChange={(v) => set({ skills: v }, 'skills')} />
      <Check checked={!!u.leader} label={S.unitLeader} onChange={(v) => set({ leader: v ? true : undefined })} />
      <Field label={S.storyName} help={S.storyNameHelp}>
        <IdInput value={u.id ?? ''} validate={validate} onChange={setId} placeholder={S.unitStoryPlaceholder} />
      </Field>
      <button type="button" className="btn sm" onClick={() => setId(suggestUnitId(doc, map, u))}>{S.fillFromName}</button>
      {((doc.items?.length ?? 0) > 0 || u.items !== undefined) && (
        <div className="stack tight">
          <Toggle label={S.ownItems} help={S.ownItemsHelp} checked={u.items !== undefined}
            onChange={(v) => set({ items: v ? clone(base?.items ?? []) : undefined })} />
          {u.items !== undefined && (
            <ItemListEditor label={S.unitItems} entries={u.items} items={doc.items ?? []} allowDrop max={inventorySize(doc)}
              onChange={(next, k) => set({ items: next }, k && `items${k}`)} />
          )}
        </div>
      )}
    </div>
  )
}
