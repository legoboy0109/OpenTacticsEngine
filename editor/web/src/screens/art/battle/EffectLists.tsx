// The lists of one effect entry (docs/BATTLE_SCENE.md "Several effects, by unit type"): the effects that also play with it, and the
// choices by the fighter's unit type.
import { effectList, idList, listable, renameByTypeKey, setByType, setEffectIds } from '../../../model/battleEffects'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { ET } from './effectText'

/** Rows of effect choices: one Select per listed effect, a remove button each, and an add button. */
function IdRows({ ids, options, label, which, remove, add, addLabel, onChange }: {
  ids: string[]; options: string[]; label: string; which: (n: number) => string; remove: (n: number) => string; add: string; addLabel?: string; onChange: (ids: string[]) => void
}) {
  return (
    <div className="stack tight">
      {ids.map((id, n) => (
        <div className="row" key={n}>
          <Select aria-label={`${label}: ${which(n + 1)}`} value={id}
            options={[...(options.includes(id) ? [] : [{ value: id, label: `${id} (missing)` }]), ...options.map((o) => ({ value: o, label: o }))]}
            onChange={(v) => onChange(ids.map((x, k) => (k === n ? v : x)))} />
          <button type="button" className="btn sm" aria-label={`${label}: ${remove(n + 1)}`} onClick={() => onChange(ids.filter((_, k) => k !== n))}><Icon name="trash" /></button>
        </div>
      ))}
      {options.length > 0 && <div><button type="button" className="btn sm" aria-label={addLabel ?? add} onClick={() => onChange([...ids, options[0]])}><Icon name="plus" />{add}</button></div>}
    </div>
  )
}

export function EffectLists({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const e = effectList(doc)[index]
  if (!e) return null
  const options = listable(doc, index).map((l) => l.id)
  const ids = idList(e.effects)
  const rows = Object.entries(e.byType ?? {})
  const units = doc.units ?? []
  const freeTypes = units.filter((u) => !(e.byType && u.id in e.byType))
  return (
    <>
      <Field label={ET.also} help={options.length === 0 && ids.length === 0 ? ET.alsoNone : ET.alsoHelp}>
        <IdRows ids={ids} options={options} label={ET.also} which={ET.alsoWhich} remove={ET.alsoRemove} add={ET.alsoAdd} onChange={(v) => edit((d) => setEffectIds(d, index, v))} />
      </Field>
      <Field label={ET.byType} help={ET.byTypeHelp}>
        <div className="stack tight" data-testid={`effect-bytype-${index}`}>
          {rows.map(([type, v]) => {
            const list = idList(v)
            return (
              <div className="stack tight battle-rule" key={type}>
                <div className="row">
                  <Select aria-label={`${ET.byType}: ${ET.byTypeWhich} ${type}`} value={type}
                    options={[{ value: type, label: units.find((u) => u.id === type)?.name ?? type }, ...freeTypes.map((u) => ({ value: u.id, label: u.name }))]}
                    onChange={(to) => edit((d) => renameByTypeKey(d, index, type, to))} />
                  <button type="button" className="btn sm" aria-label={ET.byTypeRemove(type)} onClick={() => edit((d) => setByType(d, index, type, null))}><Icon name="trash" />{ET.removeShort}</button>
                </div>
                {list.length === 0 && <p className="help">{ET.byTypeEmpty}</p>}
                <IdRows ids={list} options={options} label={type} which={(n) => ET.byTypeEffect(type, n)} remove={(n) => ET.byTypeEffectRemove(type, n)}
                  add={ET.byTypeEffectAdd(type)} onChange={(next) => edit((d) => setByType(d, index, type, next))} />
              </div>
            )
          })}
          {freeTypes.length > 0 && <div><button type="button" className="btn sm" onClick={() => edit((d) => setByType(d, index, freeTypes[0].id, []))}><Icon name="plus" />{ET.byTypeAdd}</button></div>}
        </div>
      </Field>
    </>
  )
}
