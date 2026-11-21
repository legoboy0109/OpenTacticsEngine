// The stats (and growths) table of a character: three columns. The class's value (greyed, read-only), the personal bonus
// (editable, may be negative) and the total the unit starts with. Typing in the middle column is the only edit.
import { setCharacterNumber, totalStat } from '../../model/characters'
import { featureEnabled } from '../../model/rules'
import type { NumMap } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { FieldGroup } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { cap, T } from './text'
import { STAT_LIMIT } from '../../model/dataLimits'

interface TableProps {
  label: string
  help: string
  word: string
  rows: Array<{ key: string; label: string }>
  base: NumMap | undefined
  bonus: NumMap | undefined
  suffix?: string
  /** Names the personal box for screen readers and tests: "Strength personal bonus" / "Strength personal growth bonus". */
  boxName: string
  onChange: (key: string, v: number | undefined) => void
  /** `data-learn` name for the tutorial's pointing ring. */
  learn?: string
}

function ThreeColumns({ label, help, word, rows, base, bonus, suffix, boxName, onChange, learn }: TableProps) {
  return (
    <FieldGroup label={label} help={help}>
      <table className="kv three-col">
        <thead>
          <tr>
            <th scope="col"><span className="sr-only">Name</span></th>
            <th scope="col">{cap(word)}</th>
            <th scope="col">{T.colBonus}</th>
            <th scope="col">{T.colTotal}</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((r, i) => (
            <tr key={r.key}>
              <th scope="row">{r.label}</th>
              <td className="muted-cell" data-testid={`base-${r.key}`}>{base?.[r.key] ?? 0}{suffix}</td>
              <td>
                <NumberInput data-learn={i === 0 ? learn : undefined} value={bonus?.[r.key]} allowEmpty min={-STAT_LIMIT} max={STAT_LIMIT} placeholder="0" suffix={suffix} aria-label={`${r.label} ${boxName}`}
                  onChange={(v) => onChange(r.key, v)} />
              </td>
              <td className="total-cell" data-testid={`total-${r.key}`}><strong>{totalStat(base?.[r.key], bonus?.[r.key])}{suffix}</strong></td>
            </tr>
          ))}
        </tbody>
      </table>
    </FieldGroup>
  )
}

export function CharacterStatsCard({ id, word }: { id: string; word: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.characters?.find((x) => x.id === id)
  if (!c) return null
  const base = doc.units?.find((u) => u.id === c.type)
  const statRows = (doc.game?.stats ?? []).map((s) => ({ key: s.id, label: s.name }))
  const key = `character:${id}`
  return (
    <Card>
      <div className="stack">
        {statRows.length === 0 && <p className="help">{T.noStats}</p>}
        {statRows.length > 0 && (
          <ThreeColumns label={T.statsTitle} help={T.statsHelp(word)} word={word} rows={statRows} base={base?.stats} bonus={c.stats} boxName="personal bonus" learn="character-stats"
            onChange={(k, v) => edit((d) => setCharacterNumber(d, id, 'stats', k, v), `${key}:stat:${k}`)} />
        )}
        {statRows.length > 0 && featureEnabled(doc, 'progression') && (
          <ThreeColumns label={T.growthsTitle} help={T.growthsHelp(word)} word={word} rows={statRows} base={base?.growths} bonus={c.growths} suffix="%" boxName="personal growth bonus"
            onChange={(k, v) => edit((d) => setCharacterNumber(d, id, 'growths', k, v), `${key}:growth:${k}`)} />
        )}
      </div>
    </Card>
  )
}
