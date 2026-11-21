// A table with one box per stat: the caps of a class and the cap adjustments of a character share it. In `adjust` mode the table has a
// greyed column with the cap before the adjustment (the class's, else the game's) and a "Result" column.
import { MAX_CAP, resultingCap } from '../../model/ranks'
import { useDoc } from '../../store/context'
import { NumberInput } from '../../ui/NumberInput'
import { R } from './text'
import { STAT_LIMIT } from '../../model/dataLimits'

interface Props {
  values: Record<string, number> | undefined
  onChange: (stat: string, v: number | undefined) => void
  /** Class caps: the box shows the game's cap as its placeholder. Adjustments: a three-column table with the result. */
  mode: 'cap' | 'adjust'
  /** `adjust` only: the cap each stat has before the adjustment (undefined = none). */
  base?: (stat: string) => number | undefined
  /** `cap` only: the game's cap per stat, shown as the placeholder. */
  fallback?: (stat: string) => number | undefined
  testId: string
}

export function CapRows({ values, onChange, mode, base, fallback, testId }: Props) {
  const doc = useDoc()
  const stats = doc.game?.stats ?? []
  const word = (n: number | undefined) => (n === undefined ? R.capNone : String(n))
  return (
    <table className="kv three-col cap-rows" data-testid={testId}>
      <thead>
        <tr>
          <th scope="col"><span className="sr-only">Stat</span></th>
          {mode === 'adjust' && <th scope="col">{R.colCap}</th>}
          <th scope="col">{mode === 'adjust' ? R.colAdjust : R.capCol}</th>
          {mode === 'adjust' && <th scope="col">{R.colResult}</th>}
        </tr>
      </thead>
      <tbody>
        {stats.map((s) => (
          <tr key={s.id}>
            <th scope="row">{s.name}</th>
            {mode === 'adjust' && <td className="muted-cell" data-testid={`cap-base-${s.id}`}>{word(base?.(s.id))}</td>}
            <td>
              {mode === 'adjust'
                ? <NumberInput value={values?.[s.id]} allowEmpty min={-STAT_LIMIT} max={STAT_LIMIT} placeholder="0" aria-label={`${s.name} cap adjustment`} onChange={(v) => onChange(s.id, v)} />
                : <NumberInput value={values?.[s.id]} allowEmpty min={1} max={MAX_CAP} placeholder={word(fallback?.(s.id))} aria-label={R.capLabel(s.name)} onChange={(v) => onChange(s.id, v)} />}
            </td>
            {mode === 'adjust' && <td className="total-cell" data-testid={`cap-result-${s.id}`}><strong>{word(resultingCap(base?.(s.id), values?.[s.id]))}</strong></td>}
          </tr>
        ))}
      </tbody>
    </table>
  )
}
