// Rules card "Carrying" (E3 of docs/SRPG.md "Editor"): Off / Carry / Pair up, who can carry whom, what the carrier loses while carrying,
// and (pair up) what the partner behind gives. Edits `rules.rescue` of game.json; the model is model/rescue.ts.
import { useState } from 'react'
import { rescueChecks } from '../../../model/rescueChecks'
import { allTags } from '../../../model/units'
import { addCarrierRow, carrierRows, changeableStats, isFormulaWord, readCanCarry, rescueMode, rescueModeLoss, removeCarrierRow, setCanCarry, setCarrierRow, setRescueMode, setRulesPairBonus, writeCanCarry, rescueOf, type CanCarry, type ChangeMode, type RescueMode } from '../../../model/rescue'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Advanced, Card } from '../../../ui/Card'
import { ConfirmDialog } from '../../../ui/Dialog'
import { Field } from '../../../ui/Field'
import { KeyNumberTable } from '../../../ui/KeyNumberTable'
import { NumberInput } from '../../../ui/NumberInput'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import '../../srpg/e3.css'
import { C } from './carryText'
import { STAT_LIMIT } from '../../../model/dataLimits'

const MODES: RescueMode[] = ['off', 'carry', 'pairUp']

export function CarryingCard() {
  const doc = useDoc()
  const edit = useEdit()
  const mode = rescueMode(doc)
  const [asking, setAsking] = useState<RescueMode | null>(null)
  const stats = changeableStats(doc)
  const problems = rescueChecks(doc).filter((c) => c.section === 'rules')

  const pick = (m: RescueMode) => (rescueModeLoss(doc, m) > 0 ? setAsking(m) : edit((d) => setRescueMode(d, m)))
  return (
    <Card title={C.title} help={C.help} id="carrying">
      <div className="stack" data-testid="carrying-card">
        {problems.length > 0 && <Banner tone="warn" title="Worth a look"><ul className="usage-list" data-testid="carrying-checks">{problems.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
        <Field label={C.mode} help={C.modeHelp[mode]}>
          <Select value={mode} options={MODES.map((m) => ({ value: m, label: C.modes[m] }))} onChange={(v) => pick(v as RescueMode)} />
        </Field>
        {mode !== 'off' && (
          <>
            <WhoCanCarry />
            <WhileCarrying stats={stats} />
            {mode === 'pairUp' && (
              <KeyNumberTable label={C.behind} help={C.behindHelp} rows={stats.map((s) => ({ key: s.id, label: s.name }))}
                columns={[{ title: C.behindCol, values: rescueOf(doc)?.pairBonus, min: -STAT_LIMIT, max: STAT_LIMIT, placeholder: '0', onChange: (k, v) => edit((d) => setRulesPairBonus(d, k, v), `rescue:pairBonus:${k}`) }]} />
            )}
          </>
        )}
      </div>
      <ConfirmDialog open={asking !== null} title={C.loseTitle(asking ? rescueModeLoss(doc, asking) : 0)} confirmLabel={C.loseConfirm} danger
        onCancel={() => setAsking(null)} onConfirm={() => { if (asking) edit((d) => setRescueMode(d, asking)); setAsking(null) }}>
        <p>{C.loseBody(asking ? rescueModeLoss(doc, asking) : 0)}</p>
      </ConfirmDialog>
    </Card>
  )
}

/** "Who can carry whom": presets, with the raw formula under Advanced. */
function WhoCanCarry() {
  const doc = useDoc()
  const edit = useEdit()
  const all = doc.game?.stats ?? []
  const tags = allTags(doc).filter(isFormulaWord)
  const formula = rescueOf(doc)?.canCarry
  const cur = readCanCarry(formula)
  const write = (c: CanCarry, key?: string) => edit((d) => setCanCarry(d, writeCanCarry(c)), key)
  const firstStat = all[0]?.id ?? ''
  const preset = (v: string) => {
    if (v === 'anyone') write({ kind: 'anyone' })
    else if (v === 'stat') write({ kind: 'stat', mine: firstStat, theirs: firstStat })
    else if (v === 'tag') write({ kind: 'tag', tag: tags[0] ?? '' })
  }
  const options = (Object.keys(C.whoPresets) as Array<keyof typeof C.whoPresets>).map((k) => ({
    value: k, label: C.whoPresets[k],
    disabled: (k === 'custom' && cur.kind !== 'custom') || (k === 'stat' && all.length === 0) || (k === 'tag' && tags.length === 0 && cur.kind !== 'tag'),
  }))
  const statOptions = all.map((s) => ({ value: s.id, label: s.name }))

  return (
    <div className="carry-preset" data-testid="who-can-carry">
      <Field label={C.who} help={cur.kind === 'tag' && tags.length === 0 ? C.whoNoTags : C.whoHelp}>
        <Select value={cur.kind} options={options} onChange={preset} />
      </Field>
      {cur.kind === 'stat' && (
        <div className="form-grid cols-2">
          <Field label={C.whoMine}><Select value={cur.mine} options={statOptions} onChange={(v) => write({ ...cur, mine: v })} /></Field>
          <Field label={C.whoTheirs}><Select value={cur.theirs} options={statOptions} onChange={(v) => write({ ...cur, theirs: v })} /></Field>
        </div>
      )}
      {cur.kind === 'tag' && (
        <Field label={C.whoTag}>
          <Select value={cur.tag} options={tags.includes(cur.tag) || cur.tag === '' ? tags.map((t) => ({ value: t, label: t })) : [{ value: cur.tag, label: cur.tag }, ...tags.map((t) => ({ value: t, label: t }))]}
            placeholder="—" onChange={(v) => write({ kind: 'tag', tag: v })} />
        </Field>
      )}
      <Advanced label={C.advanced} defaultOpen={cur.kind === 'custom'}>
        <Field label={C.formula} help={C.formulaHelp}>
          <TextInput mono value={formula ?? ''} onChange={(v) => edit((d) => setCanCarry(d, v.trim() === '' ? undefined : v), 'rescue:canCarry')} />
        </Field>
      </Advanced>
    </div>
  )
}

/** "While carrying": rows of stat + halved / minus N. */
function WhileCarrying({ stats }: { stats: Array<{ id: string; name: string }> }) {
  const doc = useDoc()
  const edit = useEdit()
  const rows = carrierRows(doc)
  const used = new Set(rows.map((r) => r.stat))
  const canAdd = stats.some((s) => !used.has(s.id))
  return (
    <div className="stack tight" data-testid="while-carrying">
      <strong>{C.carrying}</strong>
      <p className="help">{C.carryingHelp}</p>
      {rows.length === 0 && <p className="help">{C.carryingNone}</p>}
      {rows.map((r, i) => {
        const n = i + 1
        const name = stats.find((s) => s.id === r.stat)?.name ?? r.stat
        const options = stats.filter((s) => s.id === r.stat || !used.has(s.id)).map((s) => ({ value: s.id, label: s.name }))
        return (
          <div className="carrier-row" role="group" aria-label={`Change ${n}`} key={r.stat}>
            <Field label={C.carryingStat}><Select value={r.stat} options={options.length ? options : [{ value: r.stat, label: name }]} onChange={(v) => edit((d) => setCarrierRow(d, i, v, r.change))} /></Field>
            <Field label={C.carryingHow}>
              <Select value={r.change.mode} options={(Object.keys(C.changes) as ChangeMode[]).map((m) => ({ value: m, label: C.changes[m] }))}
                onChange={(v) => edit((d) => setCarrierRow(d, i, r.stat, { mode: v as ChangeMode, n: v === 'halved' ? 50 : v === 'percent' ? 75 : 1 }))} />
            </Field>
            {r.change.mode !== 'halved' && (
              <Field label={C.carryingN}>
                <NumberInput value={r.change.n} min={0} max={r.change.mode === 'percent' ? 1000 : 999} suffix={r.change.mode === 'percent' ? '%' : undefined}
                  onChange={(v) => { if (v !== undefined) edit((d) => setCarrierRow(d, i, r.stat, { ...r.change, n: v }), `rescue:carrier:${i}`) }} />
              </Field>
            )}
            <span className="item-row-buttons">
              <button type="button" className="btn ghost sm icon" aria-label={C.carryingRemove(n)} onClick={() => edit((d) => removeCarrierRow(d, i))}><Icon name="trash" /></button>
            </span>
          </div>
        )
      })}
      <div><button type="button" className="btn" disabled={!canAdd} onClick={() => edit((d) => addCarrierRow(d))}>{C.carryingAdd}</button>{!canAdd && stats.length > 0 && <span className="help"> {C.carryingNoMore}</span>}</div>
    </div>
  )
}
