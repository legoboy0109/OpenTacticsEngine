// "Teaches" card of a weapon or an accessory (docs/JOBS.md "Learning from equipment"): skills and abilities a unit may use while it has the item,
// each with the ability points that master it. Only weapons and accessories teach (the loader refuses the key on other kinds).
import { POINTS_MAX, TEACH_MAX, addTeach, canTeach, removeTeach, setTeachEntry, setTeachPoints, teachId, teachKind, teachesOf, type TeachKind } from '../../../model/teaches'
import { apName } from '../../../model/abilityPoints'
import { useDoc, useEdit } from '../../../store/context'
import { Card, FormGrid } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TC } from './teachesText'

export function ItemTeachesCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((i) => i.id === id)
  if (!item || !canTeach(item)) return null
  const list = teachesOf(item)
  const abilities = doc.abilities ?? []
  const skills = doc.skills ?? []
  // Nothing to teach in the game and nothing taught: the card stays out of the way.
  if (list.length === 0 && abilities.length === 0 && skills.length === 0) return null
  const points = apName(doc)
  const optionsFor = (kind: TeachKind, cur: string) => {
    const base = (kind === 'ability' ? abilities : skills).map((x) => ({ value: x.id, label: x.name }))
    return base.some((o) => o.value === cur) || cur === '' ? base : [...base, { value: cur, label: TC.unknown(cur) }]
  }
  return (
    <Card title={TC.title} help={TC.help(points)}>
      <div className="stack" data-testid="item-teaches">
        {list.length === 0 && <p className="help">{TC.none}</p>}
        {list.map((t, i) => {
          const kind = teachKind(t)
          const cur = teachId(t)
          return (
            <div className="stack tight" key={i} data-testid={`teach-${i}`}>
              <FormGrid cols={3}>
                <Field label={TC.kind(i + 1)}>
                  <Select value={kind} options={[{ value: 'ability', label: TC.ability }, { value: 'skill', label: TC.skill }]}
                    onChange={(v) => edit((d) => setTeachEntry(d, id, i, v as TeachKind, ''))} />
                </Field>
                <Field label={TC.entry(i + 1)}>
                  <Select value={cur} placeholder="—" options={optionsFor(kind, cur)} onChange={(v) => edit((d) => setTeachEntry(d, id, i, kind, v))} />
                </Field>
                <Field label={TC.points(i + 1, points)} help={TC.pointsHelp}>
                  <NumberInput value={t.points} min={1} max={POINTS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setTeachPoints(d, id, i, v), `item:${id}:teach:${i}`) }} />
                </Field>
              </FormGrid>
              <div className="add-row">
                <button type="button" className="btn ghost sm icon" aria-label={TC.remove(i + 1)} onClick={() => edit((d) => removeTeach(d, id, i))}><Icon name="trash" /></button>
              </div>
            </div>
          )
        })}
        {list.length < TEACH_MAX && (
          <div className="add-row">
            <button type="button" className="btn sm" onClick={() => edit((d) => addTeach(d, id))}><Icon name="plus" />{TC.add}</button>
          </div>
        )}
      </div>
    </Card>
  )
}
