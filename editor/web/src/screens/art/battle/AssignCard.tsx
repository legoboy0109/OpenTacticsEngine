// "Who uses which set": rules that give an animation set to a unit type, optionally for one character or one weapon group.
import { addAssign, assignList, removeAssign, setList, updateAssign, weaponGroups } from '../../../model/battleArt'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { T } from './text'

export function AssignCard() {
  const doc = useDoc()
  const edit = useEdit()
  const sets = setList(doc)
  const rules = assignList(doc)
  const units = doc.units ?? []
  const characters = doc.characters ?? []
  const groups = weaponGroups(doc)
  if (sets.length === 0) return <Card title={T.assignTitle} help={T.assignHelp} id="battle-assign"><p className="help">{T.assignNeedSet}</p></Card>
  return (
    <Card title={T.assignTitle} help={T.assignHelp} id="battle-assign">
      <div className="stack" data-testid="battle-assign">
        {rules.length === 0 && <p className="help">{T.assignNone}</p>}
        <ul className="battle-rules" aria-label={T.assignTitle}>
          {rules.map((r, i) => (
            <li key={i} className="battle-rule" data-testid={`assign-${i}`}>
              <div className="battle-fields">
                <Field label={T.assignSet}>
                  <Select aria-label={`${T.assignRule(i + 1)}: ${T.assignSet}`} value={r.set} options={sets.map((s) => ({ value: s.id, label: s.id }))} onChange={(v) => edit((d) => updateAssign(d, i, { set: v }))} />
                </Field>
                <Field label={T.assignType}>
                  <Select aria-label={`${T.assignRule(i + 1)}: ${T.assignType}`} value={r.type ?? ''} options={[{ value: '', label: T.anyType }, ...units.map((u) => ({ value: u.id, label: u.name }))]}
                    onChange={(v) => edit((d) => updateAssign(d, i, { type: v === '' ? undefined : v }))} />
                </Field>
                <Field label={T.assignCharacter}>
                  <Select aria-label={`${T.assignRule(i + 1)}: ${T.assignCharacter}`} value={r.character ?? ''} options={[{ value: '', label: T.anyCharacter }, ...characters.map((c) => ({ value: c.id, label: c.name }))]}
                    onChange={(v) => edit((d) => updateAssign(d, i, { character: v === '' ? undefined : v }))} />
                </Field>
                <Field label={T.assignGroup}>
                  <Select aria-label={`${T.assignRule(i + 1)}: ${T.assignGroup}`} value={r.group ?? ''} options={[{ value: '', label: T.anyGroup }, ...groups.map((g) => ({ value: g, label: g }))]}
                    onChange={(v) => edit((d) => updateAssign(d, i, { group: v === '' ? undefined : v }))} />
                </Field>
              </div>
              <button type="button" className="btn sm" aria-label={T.assignDelete(i + 1)} onClick={() => edit((d) => removeAssign(d, i))}><Icon name="trash" />{T.assignDelete(i + 1)}</button>
            </li>
          ))}
        </ul>
        {units.length > 0 && (
          <div><button type="button" className="btn sm" onClick={() => edit((d) => addAssign(d, sets[0].id))}><Icon name="plus" />{T.assignAdd}</button></div>
        )}
      </div>
    </Card>
  )
}
