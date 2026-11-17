// The selected unit's details: type, side, name, level, leader and its own stats.
import { typeIdOf } from '../../../../model/characterUnits'
import { classWord } from '../../../../model/characters'
import { gridOfGame } from '../../../../model/gridSpec'
import { ctxOf, ctxOfParts, editMap } from '../../../../model/mapCtx'
import { changeUnitType, setUnitStat, startStats, unitAt, updateMapUnit } from '../../../../model/mapUnits'
import { idProblem, suggestUnitId, takenUnitIds } from '../../../../model/storyIds'
import { storyIdUsages } from '../../../../model/storyRefs'
import { changeUnitStoryId } from '../../../../model/storyUnits'
import { IdInput } from '../../../../ui/IdInput'
import { S } from '../story/text'
import { useRemoveUnit } from '../story/UnitRemoval'
import { useState } from 'react'
import { Link, projectPath } from '../../../../router'
import { useDoc, useEdit, useStore } from '../../../../store/context'
import { cap, T as C } from '../../../characters/text'
import { ItemListEditor } from '../../../../ui/ItemListEditor'
import { Check, Toggle } from '../../../../ui/Toggle'
import { inventorySize } from '../../../../model/itemRules'
import { entryId } from '../../../../model/itemGrants'
import { clone } from '../../../../model/doc'
import { Field, FieldGroup } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import type { Cell } from '../../../../model/mapGrid'
import { setGoesFor } from '../../../../model/mapPlaces'
import { DifficultyOnly } from './DifficultyOnly'
import { GoesFor } from './GoesFor'
import { UnitBehaviour } from './UnitBehaviour'
import { UnitRankOverride } from './UnitRankOverride'
import { UnitSkills } from './UnitSkills'
import { T } from '../text'

interface Props { mapId: string; cell: Cell; say: (m: string | null) => void }

export function UnitProps({ mapId, cell, say }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const [removeUnit, removeDialog] = useRemoveUnit(mapId)
  const map = doc.maps[mapId]
  const unit = map ? unitAt(map, cell.x, cell.y) : undefined
  if (!map || !unit) return <p className="help">{T.unitNone}</p>
  const typeId = typeIdOf(doc.characters, unit)
  const base = doc.units?.find((u) => u.id === typeId)
  const person = unit.character === undefined ? undefined : doc.characters?.find((c) => c.id === unit.character)
  // What the unit starts with when the map says nothing: for a character, its own items before the class's.
  const startItems = person?.items ?? base?.items ?? []
  const stats = doc.game?.stats ?? []
  const ctx = ctxOf(doc)
  const startOf = ctx ? startStats(ctx, unit, typeId) : {}
  const typeItems = startItems.map((e) => doc.items?.find((i) => i.id === entryId(e))?.name ?? entryId(e))
  const key = (f: string) => `unit:${mapId}:${cell.x},${cell.y}:${f}`
  const patch = (p: Parameters<typeof updateMapUnit>[3], k?: string) => edit((d) => editMap(d, mapId, (m) => updateMapUnit(m, cell.x, cell.y, p)), k)
  // The story name: events and objectives point at the unit with it; renaming rewrites them (one undo step).
  const setStoryId = (id: string) => edit((d) => changeUnitStoryId(d, mapId, cell.x, cell.y, id).doc, key('id'))
  const validateStoryId = (text: string) => (text === ''
    ? (unit.id && storyIdUsages(doc, mapId, unit.id).length ? S.nameInUse : null)
    : idProblem(text, takenUnitIds(map, unit)))
  const typeError = (id: string) => {
    const r = changeUnitType(map, ctxOfParts(doc.terrain ?? [], doc.structures ?? [], doc.units ?? [], gridOfGame(doc.game), doc.characters ?? []), cell.x, cell.y, id)
    if (r.error) { say(r.error); return }
    edit((d) => editMap(d, mapId, (m, c) => changeUnitType(m, c, cell.x, cell.y, id).map))
  }
  return (
    <div className="stack tight" data-testid="unit-props">
      <h3>{T.unitTitle} <span className="help inline">{T.unitAt(cell.x, cell.y)}</span></h3>
      {unit.character !== undefined && (
        <div className="stack tight" data-testid="unit-character">
          <p><strong>{person?.name ?? unit.character}</strong>{' '}
            {person ? <Link to={projectPath(store.id, 'characters', person.id)}>{C.openCharacter}</Link> : <span className="error-text">{C.characterGone(unit.character)}</span>}
          </p>
          <p className="help">{C.characterUnitHelp}</p>
        </div>
      )}
      <div className="form-grid panel-form">
        <Field label={cap(classWord(doc))} help={person ? C.classOnMapHelp : undefined}>
          <Select value={typeId} onChange={typeError} options={(doc.units ?? []).map((u) => ({ value: u.id, label: person && u.id === person.type ? `${u.name} (${C.classOnMap.toLowerCase()})` : u.name }))} />
        </Field>
        <Field label={T.sideLabel}>
          <Select value={unit.side} onChange={(v) => patch({ side: v })} options={(map.sides ?? []).map((s) => ({ value: s.id, label: s.name || s.id }))} />
        </Field>
        <Field label={T.unitName} help={T.unitNameHelp}>
          <TextInput value={unit.name ?? ''} placeholder={person?.name} onChange={(v) => patch({ name: v }, key('name'))} />
        </Field>
        {/* A character is known to events by its own id (docs/SRPG.md section 1): the loader refuses a second one. */}
        {unit.character === undefined && (
          <>
            <Field label={S.unitStoryName} help={S.unitStoryNameHelp}>
              <IdInput value={unit.id ?? ''} validate={validateStoryId} onChange={setStoryId} placeholder={S.unitStoryPlaceholder} />
            </Field>
            <button type="button" className="btn sm" onClick={() => setStoryId(suggestUnitId(doc, map, unit))}>{S.fillFromUnit}</button>
          </>
        )}
        {doc.game?.rules?.progression?.enabled && (
          <Field label={T.unitLevel} help={T.unitLevelHelp}>
            <NumberInput value={unit.level} min={1} allowEmpty placeholder={String(person?.level ?? 1)} onChange={(v) => patch({ level: v }, key('level'))} />
          </Field>
        )}
      </div>
      <Check checked={!!unit.leader} label={T.unitLeader} help={T.unitLeaderHelp} onChange={(v) => patch({ leader: v })} />
      <GoesFor map={map} side={unit.side} value={unit.goesFor} onChange={(goals) => edit((d) => editMap(d, mapId, (m) => setGoesFor(m, unit, goals)))} />
      <UnitBehaviour mapId={mapId} map={map} unit={unit} />
      <DifficultyOnly doc={doc} unit={unit} ai={map.sides?.find((s) => s.id === unit.side)?.controller === 'ai'} onChange={(p) => patch(p)} />
      {((doc.items?.length ?? 0) > 0 || unit.items !== undefined) && (
        <div className="stack tight" data-testid="unit-own-items">
          <Toggle label={T.ownItems} help={unit.items === undefined ? T.ownItemsOff(typeItems) : T.ownItemsOn}
            checked={unit.items !== undefined} onChange={(v) => patch({ items: v ? clone(startItems) : undefined })} />
          {unit.items !== undefined && (
            <ItemListEditor label={T.unitItems} entries={unit.items} items={doc.items ?? []} allowDrop max={inventorySize(doc)}
              onChange={(next, k) => patch({ items: next }, k && key(k))} />
          )}
        </div>
      )}
      <UnitRankOverride mapId={mapId} cell={cell} classId={typeId} />
      <UnitSkills doc={doc} value={unit.skills} onChange={(v) => patch({ skills: v })} />
      <FieldGroup label={T.statsTitle} help={T.statsHelp}>
        <table className="kv">
          <tbody>
            {stats.map((s) => (
              <tr key={s.id}>
                <th scope="row">{s.name}</th>
                <td>
                  <StatCell label={`${s.name} for this unit`} override={unit.stats?.[s.id]} base={startOf[s.id] ?? 0}
                    onChange={(v) => edit((d) => editMap(d, mapId, (m, c) => setUnitStat(m, c, cell.x, cell.y, s.id, v)), key(`stat:${s.id}`))} />
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </FieldGroup>
      <button type="button" className="btn danger sm" onClick={() => removeUnit(cell)}>{T.unitRemove}</button>
      {removeDialog}
    </div>
  )
}

/** A value equal to the unit type's own is not stored; the box keeps showing what was typed so typing "25" over a base of 2 is not interrupted. */
function StatCell({ label, override, base, onChange }: { label: string; override: number | undefined; base: number; onChange: (v: number | undefined) => void }) {
  const [typed, setTyped] = useState<number | undefined>()
  const shown = override ?? (typed === base ? typed : undefined)
  return <NumberInput aria-label={label} value={shown} allowEmpty placeholder={String(base)} onChange={(v) => { setTyped(v); onChange(v) }} />
}
