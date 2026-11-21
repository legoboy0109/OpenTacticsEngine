import { setMoveCost } from '../../model/terrain'
import { useDoc, useEdit } from '../../store/context'
import { FieldGroup } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'
import { MOVE_COST_MAX } from '../../model/dataLimits'

/** One row per movement type: a cost, or "cannot enter" (stored as 0). */
export function MoveCosts({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const terrain = doc.terrain!.find((t) => t.id === id)!
  return (
    <FieldGroup label={T.moveCosts} help={T.moveCostsHelp}>
      <table className="kv">
        <thead><tr><th scope="col"><span className="sr-only">Movement type</span></th><th scope="col">{T.moveCostCol}</th><th scope="col">{T.blockedCol}</th></tr></thead>
        <tbody>
          {doc.game!.moveTypes.map((m) => {
            const cost = terrain.moveCost?.[m.id]
            const blocked = cost !== undefined && cost <= 0
            return (
              <tr key={m.id}>
                <th scope="row">{m.name}</th>
                <td>
                  <NumberInput value={blocked ? undefined : cost} min={1} max={MOVE_COST_MAX} allowEmpty placeholder="1" disabled={blocked} aria-label={`${m.name} cost`}
                    onChange={(v) => edit((d) => setMoveCost(d, id, m.id, v), `terrain:${id}:cost:${m.id}`)} />
                </td>
                <td>
                  <Check label={`${m.name} cannot enter`} checked={blocked} onChange={(on) => edit((d) => setMoveCost(d, id, m.id, on ? 0 : undefined))} />
                </td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </FieldGroup>
  )
}
