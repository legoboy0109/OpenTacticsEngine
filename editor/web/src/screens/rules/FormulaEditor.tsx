import { useEffect, useState } from 'react'
import { formulaView, setCritMultiplier, setFormulaExpression, setFormulaParam, setFormulaPreset } from '../../model/formulas'
import { FORMULA_DEFS, type FormulaKey, type ParamDef } from '../../model/presetDefs'
import { CUSTOM, presetOf } from '../../model/presets'
import { useDoc, useEdit } from '../../store/context'
import { Advanced } from '../../ui/Card'
import { Check } from '../../ui/Toggle'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'
import { CRIT_MULTIPLIER_MAX } from '../../model/dataLimits'

/** One formula as a preset picker + the preset's few blanks + a plain-language summary. */
export function FormulaEditor({ fkey, label, help }: { fkey: FormulaKey; label?: string; help?: string }) {
  const game = useDoc().game!
  const edit = useEdit()
  const def = FORMULA_DEFS[fkey]
  const view = formulaView(game, fkey)
  const preset = presetOf(fkey, view.preset)
  const custom = view.preset === CUSTOM
  const options = [...def.presets.map((p) => ({ value: p.id, label: p.name })), { value: CUSTOM, label: T.custom }]

  return (
    <div className="formula" data-formula={fkey}>
      <Field label={`${label ?? def.label}: ${T.preset.toLowerCase()}`} help={help ?? def.help}>
        <Select value={view.preset} options={options} onChange={(v) => edit((d) => setFormulaPreset(d, fkey, v))} />
      </Field>
      <p className="help preset-desc">{preset?.description ?? T.customDescription}</p>

      {preset && preset.params.length > 0 && (
        <div className="form-grid cols-2">
          {preset.params.map((p) => (
            <ParamControl key={p.id} fkey={fkey} param={p} value={view.params[p.id]} />
          ))}
        </div>
      )}
      {custom && <p className="help">{T.customNote}</p>}
      {view.missing.length > 0 && (
        <p className="attention" role="alert">{view.missing.map((m) => T.missingStat(m)).join(' ')}</p>
      )}
      <p className="summary"><span className="summary-label">{T.summary}</span>{view.summary}</p>

      {fkey === 'crit' && view.preset !== 'none' && (
        <Field label={T.critMultiplier} help={T.critMultiplierHelp}>
          <NumberInput value={game.formulas?.critMultiplier} integer={false} min={1} max={CRIT_MULTIPLIER_MAX} allowEmpty placeholder="3"
            onChange={(v) => edit((d) => setCritMultiplier(d, v), 'game:crit-mult')} />
        </Field>
      )}
      <Advanced label={T.advanced} defaultOpen={custom}>
        <RawFormula fkey={fkey} text={view.expr} custom={custom} />
      </Advanced>
    </div>
  )
}

function ParamControl({ fkey, param, value }: { fkey: FormulaKey; param: ParamDef; value: string | number | boolean | undefined }) {
  const game = useDoc().game!
  const edit = useEdit()
  const set = (v: string | number | boolean) => edit((d) => setFormulaParam(d, fkey, param.id, v), `formula:${fkey}:${param.id}`)
  if (param.kind === 'bool') {
    return <Check label={param.label} checked={value === true} onChange={set} />
  }
  if (param.kind === 'number') {
    return (
      <Field label={param.label}>
        <NumberInput value={typeof value === 'number' ? value : param.default} min={param.min} max={param.max} suffix={param.suffix}
          onChange={(v) => { if (v !== undefined) set(v) }} />
      </Field>
    )
  }
  const cur = typeof value === 'string' ? value : ''
  const missing = !game.stats.some((s) => s.id === cur)
  return (
    <Field label={param.label} error={missing ? T.pickStat : null}>
      <Select value={missing ? '' : cur} placeholder={`${T.pickStat}…`} options={game.stats.map((s) => ({ value: s.id, label: s.name }))} onChange={set} />
    </Field>
  )
}

/** The exact text. Editable only when the formula is already custom (or becomes so by typing). */
function RawFormula({ fkey, text, custom }: { fkey: FormulaKey; text: string | undefined; custom: boolean }) {
  const edit = useEdit()
  const [local, setLocal] = useState(text ?? '')
  useEffect(() => setLocal(text ?? ''), [text])
  return (
    <Field label={T.formulaText} help={custom ? T.formulaTextHelp : text === undefined ? T.formulaTextEmpty : T.formulaTextReadonly}>
      <TextInput multiline mono rows={2} value={local} readOnly={!custom}
        onChange={(v) => { setLocal(v); edit((d) => setFormulaExpression(d, fkey, v), `formula:${fkey}:raw`) }} />
    </Field>
  )
}
