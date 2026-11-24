// The form of one condition: name, description, map tag, how long it lasts, what ends it early, what it replaces, what it stops and forces (switches),
// its other rules (the Skills screen's rule cards), and the sentence the whole condition adds up to.
import { useMemo } from 'react'
import { conditionChecks } from '../../model/conditionChecks'
import { ruleIndexes } from '../../model/conditionBehaviour'
import { conditionSummary } from '../../model/conditionText'
import {
  CONDITION_TURNS_MAX, CONDITION_WEAROFF_MAX, TAG_MAX, addConditionBlock, blocksOfCondition, durationOf, hasScalableEffect, setScale, moveConditionBlock, removeConditionBlock, setConditionKind, setDuration, setEndsWhenStruck,
  setReplaces, setTag, setTurns, setWearOff, tagOf, updateCondition, updateConditionBlock, type Duration,
} from '../../model/conditions'
import { MAX_BLOCKS } from '../../model/skillRules'
import { STARTER_CONDITIONS } from '../../model/starterConditions'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { gameHasDowned, setAutoRevive } from '../../model/downed'
import { D } from '../downed/text'
import { ReviveAmount } from '../downed/ReviveAmount'
import { BlockCard } from '../skills/BlockCard'
import '../skills/skills.css'
import { BehaviourCard } from './BehaviourCard'
import { T } from './text'

const STARTER_IDS = new Set(STARTER_CONDITIONS.map((c) => c.id))

export function ConditionForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const c = doc.conditions?.find((x) => x.id === id)
  const checks = useMemo(() => conditionChecks(doc).filter((x) => x.ids.includes(id)), [doc, id])
  if (!c) return null
  const key = `condition:${id}`
  const blocks = blocksOfCondition(c)
  const shown = ruleIndexes(c)
  const duration = durationOf(c)
  const others = (doc.conditions ?? []).filter((x) => x.id !== id).map((x) => ({ value: x.id, label: x.name }))
  for (const r of c.replaces ?? []) if (r !== id && !others.some((o) => o.value === r)) others.push({ value: r, label: `${r} (not a condition)` })
  const blocking = checks.filter((x) => x.blocks)
  const notes = checks.filter((x) => !x.blocks)

  return (
    <div className="stack" data-testid="condition-form">
      {blocking.length > 0 && (
        <Banner tone="error" title={T.notValid}><ul className="usage-list" data-testid="condition-issues">{blocking.map((x, i) => <li key={i}>{x.text}</li>)}</ul></Banner>
      )}
      <Card>
        <FormGrid>
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={c.name} onChange={(v) => edit((d) => updateCondition(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput value={c.description ?? ''} onChange={(v) => edit((d) => updateCondition(d, id, { description: v === '' ? undefined : v }), `${key}:description`)} />
          </Field>
          <Field label={T.tag} help={T.tagHelp}>
            <TextInput value={c.tag ?? ''} maxLength={TAG_MAX} placeholder={tagOf(c)} onChange={(v) => edit((d) => setTag(d, id, v), `${key}:tag`)} />
          </Field>
        </FormGrid>
      </Card>

      <Card title={T.lastTitle} help={T.lastHelp}>
        <div className="stack">
          <Field label={T.duration} help={duration === 'turns' ? undefined : T.untilHelp}>
            <Select value={duration} options={(Object.keys(T.durations) as Duration[]).map((m) => ({ value: m, label: T.durations[m] }))}
              onChange={(m) => edit((d) => setDuration(d, id, m as Duration))} />
          </Field>
          {duration === 'turns' && (
            <Field label={T.turns} help={T.turnsHelp}>
              <NumberInput value={c.turns} min={1} max={CONDITION_TURNS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setTurns(d, id, v), `${key}:turns`) }} />
            </Field>
          )}
          {duration === 'turns' && (
            <Check label={T.scale} help={c.scale === 'turnsLeft' && !hasScalableEffect(c) ? T.scaleNothing : T.scaleHelp} checked={c.scale === 'turnsLeft'} onChange={(on) => edit((d) => setScale(d, id, on))} />
          )}
          <Check label={T.wearOff} help={T.wearOffHelp} checked={c.wearOff !== undefined} onChange={(on) => edit((d) => setWearOff(d, id, on ? 25 : undefined))} />
          {c.wearOff !== undefined && (
            <Field label={T.wearOffChance}>
              <NumberInput value={c.wearOff} min={1} max={CONDITION_WEAROFF_MAX} suffix="%" onChange={(v) => { if (v !== undefined) edit((d) => setWearOff(d, id, v), `${key}:wearOff`) }} />
            </Field>
          )}
          <Check label={T.endsWhenStruck} help={T.endsWhenStruckHelp} checked={c.endsWhenStruck === true} onChange={(on) => edit((d) => setEndsWhenStruck(d, id, on))} />
          <Field label={T.kind} help={T.kindHelp}>
            <Select value={c.kind === 'helpful' ? 'helpful' : 'harmful'} options={[{ value: 'harmful', label: T.kinds.harmful }, { value: 'helpful', label: T.kinds.helpful }]}
              onChange={(v) => edit((d) => setConditionKind(d, id, v as 'harmful' | 'helpful'))} />
          </Field>
          <MultiSelect label={T.replaces} help={T.replacesHelp} options={others} value={c.replaces ?? []} emptyText={T.replacesNone} onChange={(v) => edit((d) => setReplaces(d, id, v))} />
        </div>
      </Card>

      <BehaviourCard id={id} />

      {(c.autoRevive !== undefined || gameHasDowned(doc)) && (
        <Card title={D.title}>
          <div className="stack" data-testid="condition-auto-revive">
            <Check label={D.conditionOn} help={D.conditionOnHelp} checked={c.autoRevive !== undefined} onChange={(on) => edit((d) => setAutoRevive(d, id, on ? { percent: 50 } : undefined))} />
            {c.autoRevive !== undefined && <ReviveAmount formula={false} value={c.autoRevive} onChange={(v, k) => edit((d) => setAutoRevive(d, id, v as never), k && `${key}:autoRevive:${k}`)} />}
          </div>
        </Card>
      )}

      <h2 className="skill-rules-title">{T.rulesTitle}</h2>
      <p className="help">{T.rulesHelp}</p>
      {shown.map((i, n) => (
        <BlockCard key={i} doc={doc} block={blocks[i]} index={i} count={shown.length} forCondition canDelete={blocks.length > 1 || c.autoRevive !== undefined} shownNumber={n + 1}
          learn={n === 0 && !STARTER_IDS.has(id) ? 'condition-rule' : undefined}
          edit={(fn, k) => edit((d) => updateConditionBlock(d, id, i, fn), k && `${key}:${i}:${k}`)}
          onMove={(delta) => { const to = shown[n + delta]; if (to !== undefined) edit((d) => moveConditionBlock(d, id, i, to)) }}
          onDelete={() => edit((d) => removeConditionBlock(d, id, i))} />
      ))}
      <div>
        <button type="button" className="btn" data-testid="condition-add-rule" disabled={blocks.length >= MAX_BLOCKS} onClick={() => edit((d) => addConditionBlock(d, id))}>{T.addRule}</button>
        {blocks.length >= MAX_BLOCKS && <p className="help">{T.ruleLimit}</p>}
      </div>

      <Card title={T.summaryTitle}>
        <p data-testid="condition-summary">{conditionSummary(doc, c)}</p>
      </Card>

      {notes.length > 0 && (
        <Banner tone="warn" title={T.checksTitle}><ul className="usage-list" data-testid="condition-notes">{notes.map((x, i) => <li key={i}>{x.text}</li>)}</ul></Banner>
      )}

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={c.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
