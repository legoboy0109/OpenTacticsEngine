// The door, chest and place details of the selected structure (docs/SRPG.md section 10): story name, lock, a chest's contents, who may visit.
// Shown under the kind and owner in the Structures tool's panel, only for a kind that is one of them.
import { changePlaceId, placeIdProblem, placeIsNamedByEvents, setContents, setPlacedLocked, setVisitSides, setVisitUnits, suggestPlaceId } from '../../../../model/mapPlaces'
import { editMap } from '../../../../model/mapCtx'
import { lockedByDefault, placeKindOf } from '../../../../model/places'
import type { MapFile, MapStructure, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { IdInput } from '../../../../ui/IdInput'
import { MultiSelect } from '../../../../ui/MultiSelect'
import { Select } from '../../../../ui/Select'
import { ChestContents } from './ChestContents'
import { Pl } from './placeText'

interface Props { doc: ProjectDoc; map: MapFile; mapId: string; s: MapStructure }

export function PlaceProps({ doc, map, mapId, s }: Props) {
  const edit = useEdit()
  const def = doc.structures?.find((k) => k.id === s.type)
  const kind = placeKindOf(def)
  if (!def || !kind) return null
  const at = { x: s.x, y: s.y }
  const key = (f: string) => `place:${mapId}:${s.x},${s.y}:${f}`
  const onMap = (fn: (m: MapFile) => MapFile, k?: string) => edit((d) => editMap(d, mapId, (m) => fn(m)), k)
  const setId = (id: string) => edit((d) => changePlaceId(d, mapId, at, id).doc, key('id'))
  // Clearing a name that events still use would leave them pointing at nothing, so it is refused (as for a unit's story name).
  const inUse = s.id ? placeIsNamedByEvents(map, s.id) : false
  const validate = (text: string) => (text === '' ? (inUse ? Pl.nameInUse : null) : placeIdProblem(map, s, text))
  const lockedValue = s.locked === undefined ? '' : s.locked ? 'yes' : 'no'
  const unitOptions = (map.units ?? []).filter((u) => u.id).map((u) => ({ value: u.id!, label: u.id! }))
  for (const u of s.visitUnits ?? []) if (!unitOptions.some((o) => o.value === u)) unitOptions.push({ value: u, label: `${u} (not a unit of this map)` })
  return (
    <div className="stack tight" data-testid="place-props">
      <h4>{Pl.title}</h4>
      <Field label={Pl.storyName} help={Pl.storyNameHelp}>
        <IdInput value={s.id ?? ''} validate={validate} onChange={setId} placeholder={Pl.storyNamePlaceholder} />
      </Field>
      <button type="button" className="btn sm" onClick={() => setId(suggestPlaceId(map, s.type))}>{Pl.suggest}</button>
      {(kind === 'door' || kind === 'chest') && (
        <Field label={Pl.locked} help={Pl.lockedHelp}>
          <Select value={lockedValue} options={[{ value: '', label: Pl.lockedKind(lockedByDefault(def)) }, { value: 'yes', label: Pl.lockedYes }, { value: 'no', label: Pl.lockedNo }]}
            onChange={(v) => onMap((m) => setPlacedLocked(m, at, v === '' ? undefined : v === 'yes'))} />
        </Field>
      )}
      {kind === 'chest' && <ChestContents doc={doc} value={s.contents ?? []} onChange={(list, k) => onMap((m) => setContents(m, at, list), k && key(k))} />}
      {kind === 'visit' && (
        <>
          <MultiSelect label={Pl.whoSides} help={Pl.whoHelp} options={(map.sides ?? []).map((x) => ({ value: x.id, label: x.name || x.id }))} value={s.visitSides ?? []}
            onChange={(v) => onMap((m) => setVisitSides(m, at, v))} />
          <MultiSelect label={Pl.whoUnits} emptyText={Pl.whoNoUnits} options={unitOptions} value={s.visitUnits ?? []} onChange={(v) => onMap((m) => setVisitUnits(m, at, v))} />
        </>
      )}
    </div>
  )
}
