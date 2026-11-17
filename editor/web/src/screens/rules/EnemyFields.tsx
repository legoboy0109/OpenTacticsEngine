// The numbers of one difficulty (docs/DIFFICULTY.md): enemy levels, added stats, stats percent, experience and resources. Used for an
// entry of the game's list (Rules) and for a map's own numbers (Map tab), which differ only in what an empty box means.
import type { DifficultyEnemy } from '../../model/difficultyTypes'
import type { Named } from '../../model/types'
import { Field, FieldGroup } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { MAX_FLAT_STAT, MAX_LEVEL_SHIFT, MAX_PERCENT } from '../../model/difficulty'
import { D } from './difficultyText'

export interface EnemyFieldsProps {
  /** A stable name for the controls (test ids and labels). */
  name: string
  stats: Named[]
  enemy: DifficultyEnemy | undefined
  xpPercent: number | undefined
  resourcePercent: number | undefined
  onLevels: (v: number | undefined) => void
  onStat: (stat: string, v: number | undefined) => void
  onStatsPercent: (v: number | undefined) => void
  onXp: (v: number | undefined) => void
  onResource: (v: number | undefined) => void
  /** What an empty box shows (the game's number on a map; the default in the game's list). */
  hint?: { levels?: string; statsPercent?: string; xp?: string; resource?: string; stat?: (stat: string) => string }
}

export function EnemyFields(p: EnemyFieldsProps) {
  const h = p.hint ?? {}
  return (
    <div className="stack tight" data-testid={`difficulty-fields-${p.name}`}>
      <p className="help">{D.enemyHelp}</p>
      <div className="form-grid">
        <Field label={D.levels} help={D.levelsHelp}>
          <NumberInput aria-label={`${D.levels}: ${p.name}`} value={p.enemy?.levels} min={-MAX_LEVEL_SHIFT} max={MAX_LEVEL_SHIFT} allowEmpty placeholder={h.levels ?? '0'} onChange={p.onLevels} />
        </Field>
        <Field label={D.statsPercent} help={D.statsPercentHelp}>
          <NumberInput aria-label={`${D.statsPercent}: ${p.name}`} value={p.enemy?.statsPercent} min={0} max={MAX_PERCENT} allowEmpty placeholder={h.statsPercent ?? '100'} suffix="%" onChange={p.onStatsPercent} />
        </Field>
        <Field label={D.xpPercent} help={D.xpPercentHelp}>
          <NumberInput aria-label={`${D.xpPercent}: ${p.name}`} value={p.xpPercent} min={0} max={MAX_PERCENT} allowEmpty placeholder={h.xp ?? '100'} suffix="%" onChange={p.onXp} />
        </Field>
        <Field label={D.resourcePercent} help={D.resourcePercentHelp}>
          <NumberInput aria-label={`${D.resourcePercent}: ${p.name}`} value={p.resourcePercent} min={0} max={MAX_PERCENT} allowEmpty placeholder={h.resource ?? '100'} suffix="%" onChange={p.onResource} />
        </Field>
      </div>
      {p.stats.length > 0 && (
        <FieldGroup label={D.statsAdd} help={D.statsAddHelp}>
          <table className="kv">
            <tbody>
              {p.stats.map((s) => (
                <tr key={s.id}>
                  <th scope="row">{s.name}</th>
                  <td>
                    <NumberInput aria-label={`${s.name}: ${p.name}`} value={p.enemy?.stats?.[s.id]} min={-MAX_FLAT_STAT} max={MAX_FLAT_STAT} allowEmpty placeholder={h.stat?.(s.id) ?? '0'} onChange={(v) => p.onStat(s.id, v)} />
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </FieldGroup>
      )}
    </div>
  )
}
