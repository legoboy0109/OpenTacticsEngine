// One row per weapon group with a rank picker: the shared body of the "Starting ranks" cards of classes and characters and of a
// placed unit's override. A value is a rank id; a number of weapon experience (only a hand-edited file has one) is kept and shown.
import { groupsOf, ladderOf } from '../../model/ranks'
import type { RankMap, RankValue } from '../../model/srpgTypes'
import { useDoc } from '../../store/context'
import { Select } from '../../ui/Select'
import { R } from './text'

const XP = '__xp'
const UNKNOWN = '__unknown'

interface Props {
  /** The groups to offer (their order is kept). Groups the map already has but no weapon has are added as warnings. */
  groups: string[]
  value: RankMap | undefined
  /** The choice that means "no entry here" ("None", "Class's rank", "Its own"). */
  noneLabel: string
  onSet: (group: string, v: RankValue | undefined) => void
  testId: string
}

export function RankMapTable({ groups, value, noneLabel, onSet, testId }: Props) {
  const doc = useDoc()
  const ladder = ladderOf(doc)
  const existing = groupsOf(doc)
  const rows = [...new Set([...groups.filter((g) => existing.includes(g)), ...Object.keys(value ?? {})])]
  return (
    <table className="kv rank-map" data-testid={testId}>
      <tbody>
        {rows.map((g) => {
          const v = value?.[g]
          const gone = !existing.includes(g)
          const known = typeof v === 'string' && ladder.some((r) => r.id === v)
          const options = [
            { value: '', label: noneLabel },
            ...ladder.map((r) => ({ value: r.id, label: r.name })),
            ...(typeof v === 'number' ? [{ value: XP, label: R.expKept(v) }] : []),
            ...(typeof v === 'string' && !known ? [{ value: UNKNOWN, label: `${v} (not in the ladder)` }] : []),
          ]
          const current = v === undefined ? '' : typeof v === 'number' ? XP : known ? v : UNKNOWN
          return (
            <tr key={g}>
              <th scope="row">{g}{gone && <span className="error-text" role="alert">{R.groupGone(g)}</span>}</th>
              <td>
                <Select aria-label={`Starting rank in ${g}`} value={current} options={options}
                  onChange={(next) => { if (next === XP || next === UNKNOWN) return; onSet(g, next === '' ? undefined : next) }} />
              </td>
              {gone && <td><button type="button" className="btn ghost sm" onClick={() => onSet(g, undefined)}>{R.forgetRank}</button></td>}
            </tr>
          )
        })}
      </tbody>
    </table>
  )
}
