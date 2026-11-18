// An effect's "atRange" rows: from a fight distance on, the whole effect starts later (docs/BATTLE_SCENE.md "Effect timing by distance").
import { addEffectRange, effectList, effectRanges, RANGE_MAX, setEffectRanges } from '../../../model/battleEffects'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { ET } from './effectText'

export function EffectRanges({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const rows = effectRanges(effectList(doc)[index])
  const patchRow = (n: number, p: Partial<{ from: number; delayMs: number }>, group: string) =>
    edit((d) => setEffectRanges(d, index, rows.map((r, k) => (k === n ? { ...r, ...p } : r))), group)
  return (
    <Field label={ET.ranges} help={ET.rangesHelp}>
      <div className="stack tight" data-testid={`effect-ranges-${index}`}>
        {rows.map((r, n) => (
          <div className="battle-fields" key={n}>
            <Field label={ET.rangeFrom}><NumberInput aria-label={`${ET.ranges} ${n + 1}: ${ET.rangeFrom}`} min={1} max={99} value={r.from} onChange={(v) => v !== undefined && patchRow(n, { from: v }, `fx${index}:range${n}:from`)} /></Field>
            <Field label={ET.rangeDelay}><NumberInput aria-label={`${ET.ranges} ${n + 1}: ${ET.rangeDelay}`} min={0} max={15000} value={r.delayMs} onChange={(v) => v !== undefined && patchRow(n, { delayMs: v }, `fx${index}:range${n}:ms`)} /></Field>
            <button type="button" className="btn sm" aria-label={ET.rangeRemove(n + 1)} onClick={() => edit((d) => setEffectRanges(d, index, rows.filter((_, k) => k !== n)))}>{ET.removeShort}</button>
          </div>
        ))}
        {rows.length < RANGE_MAX && <div><button type="button" className="btn sm" aria-label={`${ET.rangeAdd} (effect ${index + 1})`} onClick={() => edit((d) => addEffectRange(d, index))}><Icon name="plus" />{ET.rangeAdd}</button></div>}
      </div>
    </Field>
  )
}
