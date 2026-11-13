import { damageMode } from '../../model/formulas'
import { allTags } from '../../model/units'
import { changeVersusKey, removeVersus, setVersus, setVersusDefault } from '../../model/weapons'
import { useDoc, useEdit } from '../../store/context'
import { FieldGroup } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { Select, type OptionGroup } from '../../ui/Select'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

/** "Against this unit or label, use this number": rows plus an "everything else" line. */
export function Effectiveness({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const weapon = doc.weapons!.find((w) => w.id === id)!
  const percent = damageMode(doc.game!) === 'percent'
  const normal = percent ? 100 : 1
  const rows = Object.entries(weapon.versus ?? {})
  const used = new Set(rows.map(([k]) => k))

  const units = (doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))
  const unitIds = new Set(units.map((u) => u.value))
  const labels = allTags(doc).filter((t) => !unitIds.has(t)).map((t) => ({ value: t, label: t }))
  const groups = (except?: string): OptionGroup[] => [
    { group: T.groupUnits, options: units.filter((o) => !used.has(o.value) || o.value === except) },
    { group: T.groupLabels, options: labels.filter((o) => !used.has(o.value) || o.value === except) },
  ].filter((g) => g.options.length > 0)
  const key = `weapon:${id}`

  return (
    <FieldGroup label={T.effect} help={percent ? T.effectHelpPercent : T.effectHelpMultiplier}>
      {rows.length === 0 && <p className="help">{T.effectEmpty}</p>}
      {rows.length > 0 && (
        <table className="kv">
          <thead><tr><th scope="col">Against</th><th scope="col">{percent ? T.effectColPercent : T.effectColMultiplier}</th><th scope="col"><span className="sr-only">Remove</span></th></tr></thead>
          <tbody>
            {rows.map(([k, v]) => {
              const known = unitIds.has(k) || labels.some((l) => l.value === k)
              const options: Array<OptionGroup | { value: string; label: string }> = groups(k)
              if (!known) options.unshift({ value: k, label: T.effectMissing(k) })
              return (
                <tr key={k}>
                  <td><Select value={k} options={options} aria-label="Unit or label" onChange={(to) => edit((d) => changeVersusKey(d, id, k, to))} /></td>
                  <td><NumberInput value={v} integer={false} aria-label={`${k} ${percent ? T.effectColPercent : T.effectColMultiplier}`}
                    onChange={(n) => { if (n !== undefined) edit((d) => setVersus(d, id, k, n, normal), `${key}:vs:${k}`) }} /></td>
                  <td><button type="button" className="btn ghost sm icon" aria-label={T.removeRow(k)} onClick={() => edit((d) => removeVersus(d, id, k))}><Icon name="close" /></button></td>
                </tr>
              )
            })}
          </tbody>
        </table>
      )}
      {groups().length > 0 && (
        <div className="add-row">
          <Select value="" placeholder={T.effectAdd} options={groups()} aria-label={T.effectAdd}
            onChange={(k) => edit((d) => setVersus(d, id, k, normal, normal))} />
        </div>
      )}
      {rows.length > 0 && (
        <div className="stack else-row">
          <Toggle label={T.elseToggle} help={weapon.versusDefault !== undefined ? T.elseHelpOn : T.elseHelpOff} checked={weapon.versusDefault !== undefined}
            onChange={(on) => edit((d) => setVersusDefault(d, id, on ? normal : undefined))} />
          {weapon.versusDefault !== undefined && (
            <div className="field">
              <NumberInput value={weapon.versusDefault} integer={false} aria-label={percent ? T.elseValuePercent : T.elseValueMultiplier}
                onChange={(n) => { if (n !== undefined) edit((d) => setVersusDefault(d, id, n), `${key}:else`) }} />
            </div>
          )}
        </div>
      )}
    </FieldGroup>
  )
}
