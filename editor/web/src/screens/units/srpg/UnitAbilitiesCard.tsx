// The Abilities card of a unit type: which menu abilities it has, in menu order (E3 of docs/SRPG.md). Edits `abilities` of the
// unit type; shown once the game has abilities (or the unit type already names some, so they can be removed).
import { abilityName } from '../../../model/abilities'
import { updateUnit } from '../../../model/units'
import { moveItem } from '../../../model/util'
import { Link, projectPath } from '../../../router'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { RowButtons } from '../../../ui/RowButtons'
import { Select } from '../../../ui/Select'
import '../../srpg/e3.css'
import { T } from '../../abilities/text'

export function UnitAbilitiesCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit || (doc.abilities === null && !unit.abilities?.length)) return null
  const have = unit.abilities ?? []
  const defined = doc.abilities ?? []
  const set = (next: string[]) => edit((d) => updateUnit(d, id, { abilities: next.length ? next : undefined }))
  const addable = defined.filter((a) => !have.includes(a.id))

  return (
    <Card title={T.cardTitle} help={T.cardHelp} id="unit-abilities">
      <div className="item-rows" data-testid="unit-abilities">
        {have.length === 0 && <p className="help">{defined.length === 0 ? T.cardNoneDefined : T.cardNone}</p>}
        {have.map((aid, i) => {
          const name = abilityName(doc, aid)
          const known = defined.some((a) => a.id === aid)
          return (
            <div className="item-row" key={aid}>
              <span className="grow">{known ? <Link to={projectPath(store.id, 'abilities', aid)}>{name}</Link> : `${aid} (not an ability any more)`}</span>
              <RowButtons index={i} count={have.length} up={`Move ${name} up`} down={`Move ${name} down`} del={T.cardRemove(name)}
                onUp={() => set(moveItem(have, i, i - 1))} onDown={() => set(moveItem(have, i, i + 1))} onDelete={() => set(have.filter((x) => x !== aid))} />
            </div>
          )
        })}
        {addable.length > 0 && (
          <Select aria-label={`${T.cardTitle}: ${T.cardAdd}`} value="" placeholder={T.cardAdd} options={addable.map((a) => ({ value: a.id, label: a.name }))}
            onChange={(v) => set([...have, v])} />
        )}
      </div>
    </Card>
  )
}
