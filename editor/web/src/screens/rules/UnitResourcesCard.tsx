// "Unit resources" card on the Rules screen: game.json "unitResources" (docs/SRPG.md "Unit resources (MP and the like)"): pools each unit has
// for itself, whose maximum is a stat, that abilities spend. Each pool is a row of small fields; the checks of model/unitResources.ts show below.
import { useState } from 'react'
import { poolUsers, UNIT_RESOURCE_MAX, NAME_MAX, addUnitResource, newUnitResourceId, moveUnitResource, removeUnitResource, unitResourceChecks, unitResourceList, unitResourceName, updateUnitResource } from '../../model/unitResources'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, FormGrid } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { U } from './unitResourcesText'

export function UnitResourcesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const [text, setText] = useState('')
  const [deleting, setDeleting] = useState<string | null>(null)
  const list = unitResourceList(doc)
  const stats = (doc.game?.stats ?? []).map((s) => ({ value: s.id, label: s.name }))
  const target = list.find((r) => r.id === deleting)
  const problems = unitResourceChecks(doc)
  const add = () => {
    const name = text.trim().slice(0, NAME_MAX)
    if (!name) return
    edit((d) => addUnitResource(d, name, newUnitResourceId(d, name)))
    setText('')
  }
  return (
    <Card title={U.title} help={U.help}>
      <div className="stack" data-testid="unit-resources">
        {list.length === 0 && <p className="help">{U.empty}</p>}
        {list.map((r, i) => {
          const key = `unitResource:${r.id}`
          const options = stats.some((s) => s.value === r.max) ? stats : [...stats, { value: r.max, label: U.unknownStat(r.max) }]
          return (
            <div className="stack tight" key={r.id} data-testid={`unit-resource-${r.id}`}>
              <FormGrid>
                <Field label={U.name}><TextInput value={r.name ?? ''} placeholder={r.id} maxLength={NAME_MAX} onChange={(v) => edit((d) => updateUnitResource(d, r.id, { name: v }), `${key}:name`)} /></Field>
                <Field label={U.max} help={U.maxHelp}><Select value={r.max} options={options} onChange={(v) => edit((d) => updateUnitResource(d, r.id, { max: v }))} /></Field>
                <Field label={U.start} help={U.startHelp}><NumberInput value={r.start ?? 100} min={0} max={100} suffix="%" onChange={(v) => { if (v !== undefined) edit((d) => updateUnitResource(d, r.id, { start: v }), `${key}:start`) }} /></Field>
                <Field label={U.perTurn} help={U.regenHelp}><NumberInput value={r.regen?.perTurn ?? 0} min={0} max={999} onChange={(v) => { if (v !== undefined) edit((d) => updateUnitResource(d, r.id, { perTurn: v }), `${key}:perTurn`) }} /></Field>
                <Field label={U.percent}><NumberInput value={r.regen?.percent ?? 0} min={0} max={100} suffix="%" onChange={(v) => { if (v !== undefined) edit((d) => updateUnitResource(d, r.id, { percent: v }), `${key}:percent`) }} /></Field>
              </FormGrid>
              <div className="add-row">
                <button type="button" className="btn ghost sm icon" aria-label={U.up(unitResourceName(r))} disabled={i === 0} onClick={() => edit((d) => moveUnitResource(d, i, i - 1))}><Icon name="up" /></button>
                <button type="button" className="btn ghost sm icon" aria-label={U.down(unitResourceName(r))} disabled={i === list.length - 1} onClick={() => edit((d) => moveUnitResource(d, i, i + 1))}><Icon name="down" /></button>
                <button type="button" className="btn ghost sm icon" aria-label={U.remove(unitResourceName(r))} onClick={() => setDeleting(r.id)}><Icon name="trash" /></button>
              </div>
            </div>
          )
        })}
        {list.length < UNIT_RESOURCE_MAX
          ? (
            <div className="add-row">
              <input className="input" aria-label={U.addLabel} placeholder={U.addLabel} value={text} maxLength={NAME_MAX} onChange={(e) => setText(e.target.value)}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); add() } }} autoComplete="off" />
              <button type="button" className="btn sm" disabled={!text.trim()} onClick={add}><Icon name="plus" />{U.add}</button>
            </div>
          )
          : <p className="help">{U.full(UNIT_RESOURCE_MAX)}</p>}
        {problems.length > 0 && <Banner tone="warn" title={U.checksTitle}><ul className="usage-list" data-testid="unit-resource-checks">{problems.map((p, i) => <li key={i}>{p}</li>)}</ul></Banner>}
      </div>
      <DeleteConfirm open={!!target} what={target ? U.deleteWhat(unitResourceName(target)) : ''}
        usages={target ? poolUsers(doc, target.id).map((n) => ({ section: 'abilities' as const, text: U.usedBy(n) })) : []}
        onCancel={() => setDeleting(null)} onConfirm={() => { if (target) edit((d) => removeUnitResource(d, target.id)); setDeleting(null) }} />
    </Card>
  )
}
