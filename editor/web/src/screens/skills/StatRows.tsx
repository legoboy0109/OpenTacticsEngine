// Stat bonus rows for a "Stat bonus" / "Growth bonus" effect: one row per stat (stat picker, amount, remove) and a picker to
// add another. The effect keeps at least one stat (the loader refuses an empty one), so the last row has no remove button.
import type { ProjectDoc } from '../../model/types'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Num } from './Num'
import { T } from './text'

interface Props {
  doc: ProjectDoc
  value: Record<string, number | string>
  /** A combat bonus (not growths, not an always bonus) may give a stat as a formula (docs/SRPG.md "Formula-sized bonuses"). */
  formulas?: boolean
  /** The stats that may be picked. */
  choices: string[]
  label: string
  onChange: (next: Record<string, number | string>, key?: string) => void
}

export function StatRows({ doc, value, formulas, choices, label, onChange }: Props) {
  const name = (id: string) => doc.game?.stats?.find((s) => s.id === id)?.name ?? id
  const rows = Object.entries(value)
  const free = choices.filter((s) => !(s in value))
  // Renaming a row's stat keeps its place and its amount.
  const swap = (from: string, to: string) => onChange(Object.fromEntries(rows.map(([k, n]) => (k === from ? [to, n] : [k, n]))))
  return (
    <div className="stat-rows" role="group" aria-label={label}>
      {rows.map(([k, n], i) => (
        <div className="carrier-row" key={k}>
          <Field label={`${label} ${i + 1}`}>
            <Select value={k} options={[...new Set([k, ...free])].map((s) => ({ value: s, label: name(s) }))} onChange={(to) => swap(k, to)} />
          </Field>
          {formulas && (
            <Field label={`${T.formulaStatKind} ${i + 1}`}>
              <Select value={typeof n === 'string' ? 'formula' : 'number'} options={Object.entries(T.amountModes).map(([v, l]) => ({ value: v, label: l }))}
                onChange={(m) => onChange({ ...value, [k]: m === 'formula' ? 'turn' : 0 })} />
            </Field>
          )}
          {typeof n === 'string'
            ? <Field label={`${T.amountFormula} ${i + 1}`} help={T.amountFormulaHelp}><TextInput mono value={n} onChange={(x) => onChange({ ...value, [k]: x }, `stat:${k}`)} /></Field>
            : <Field label={`${T.amount} ${i + 1}`}><Num value={n} min={-1000} max={1000} onChange={(x) => onChange({ ...value, [k]: x }, `stat:${k}`)} /></Field>}
          {rows.length > 1 && (
            <button type="button" className="btn ghost sm icon" aria-label={T.removeStat(name(k))}
              onClick={() => onChange(Object.fromEntries(rows.filter(([x]) => x !== k)))}><Icon name="trash" /></button>
          )}
        </div>
      ))}
      {free.length > 0 && (
        <Select aria-label={`${label}: ${T.addStat}`} value="" placeholder={T.addStat} options={free.map((s) => ({ value: s, label: name(s) }))}
          onChange={(s) => onChange({ ...value, [s]: 1 })} />
      )}
    </div>
  )
}
