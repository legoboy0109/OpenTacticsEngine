import { moveModeOf, setMoveRule, type MoveMode } from '../../model/structures'
import { useDoc, useEdit } from '../../store/context'
import { FieldGroup } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { T } from './text'

/** One row per movement type: the ground's cost, a cost of the structure's own, or cannot enter (stored as 0). */
export function MovingThrough({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures!.find((x) => x.id === id)!
  const moveTypes = doc.game!.moveTypes
  return (
    <FieldGroup label={T.moving} help={T.movingHelp}>
      {moveTypes.length === 0 && <p className="help">{T.moveTypesNone}</p>}
      <table className="kv">
        <thead><tr><th scope="col"><span className="sr-only">Movement type</span></th><th scope="col">{T.movingCol}</th><th scope="col">{T.costCol}</th></tr></thead>
        <tbody>
          {moveTypes.map((m) => {
            const mode = moveModeOf(s, m.id)
            return (
              <tr key={m.id}>
                <th scope="row">{m.name}</th>
                <td>
                  <Select value={mode} aria-label={`${m.name}: ${T.movingCol.toLowerCase()}`}
                    options={[{ value: 'ground', label: T.sameAsGround }, { value: 'cost', label: T.ownCost }, { value: 'blocked', label: T.cannotEnter }]}
                    onChange={(v) => edit((d) => setMoveRule(d, id, m.id, v as MoveMode))} />
                </td>
                <td>
                  {mode === 'cost' && (
                    <NumberInput value={s.moveCost?.[m.id]} min={1} aria-label={`${m.name} cost`}
                      onChange={(v) => { if (v !== undefined) edit((d) => setMoveRule(d, id, m.id, 'cost', v), `structure:${id}:cost:${m.id}`) }} />
                  )}
                </td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </FieldGroup>
  )
}
