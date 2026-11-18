// "Computer player" card on the Rules screen: rules.ai in game.json (docs/AI.md "Scripted-style computer players"): the order the computer's units act in and
// the named sets of attack weights. Always shown; nothing is written for the defaults.
import { useState } from 'react'
import { addWeightSet, aiRuleProblems, aiRules, isSetName, removeWeightSet, setTurnOrder, weightSetUsers, type TurnOrder } from '../../model/aiRules'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { Field } from '../../ui/Field'
import { NameDialog } from '../../ui/NameDialog'
import { Select } from '../../ui/Select'
import { AI } from './aiRuleText'
import { WeightSetEditor } from './WeightSetEditor'

export function AiRuleCard() {
  const doc = useDoc()
  const edit = useEdit()
  const { turnOrder, weights } = aiRules(doc)
  const [adding, setAdding] = useState(false)
  const [removing, setRemoving] = useState<string | null>(null)
  const [badName, setBadName] = useState(false)
  const problems = aiRuleProblems(doc)
  const names = Object.keys(weights)
  const add = (raw: string) => {
    if (!isSetName(raw) || names.includes(raw)) { setBadName(true); return }
    setBadName(false)
    setAdding(false)
    edit((d) => addWeightSet(d, raw))
  }
  return (
    <Card title={AI.title} help={AI.help} id="computer-player">
      <div className="stack" data-testid="ai-rules">
        {problems.length > 0 && <Banner tone="warn" title="Check the computer player"><ul data-testid="ai-rule-problems">{problems.map((p, i) => <li key={i}>{p}</li>)}</ul></Banner>}
        <Field label={AI.turnOrder} help={AI.turnOrderHelp}>
          <Select aria-label={AI.turnOrder} value={turnOrder} onChange={(v) => edit((d) => setTurnOrder(d, v as TurnOrder))}
            options={[{ value: 'best', label: AI.best }, { value: 'order', label: AI.order }]} />
        </Field>
        <h3>{AI.weightsTitle}</h3>
        <p className="help">{AI.weightsHelp}</p>
        {names.length === 0 && <p className="help">{AI.weightsNone}</p>}
        {names.map((n) => (
          <details key={n} className="fieldset" data-testid={`weight-card-${n}`}>
            <summary>{AI.setTitle(n)}</summary>
            <WeightSetEditor name={n} set={weights[n]} />
            <button type="button" className="btn sm danger" onClick={() => setRemoving(n)}>{AI.removeSet}</button>
          </details>
        ))}
        <div><button type="button" className="btn sm" onClick={() => { setBadName(false); setAdding(true) }}>{AI.addSet}</button></div>
      </div>
      <NameDialog open={adding} title={AI.addSetTitle} label={AI.addSetLabel} help={badName ? AI.addSetBad : AI.addSetHelp} onSubmit={add} onCancel={() => setAdding(false)} />
      <DeleteConfirm open={removing !== null} what={removing ? AI.removeTitle(removing) : ''} usages={[]}
        notes={removing && weightSetUsers(doc, removing) > 0 ? [AI.usedBy(weightSetUsers(doc, removing))] : []}
        onConfirm={() => { if (removing) edit((d) => removeWeightSet(d, removing)); setRemoving(null) }} onCancel={() => setRemoving(null)} />
    </Card>
  )
}
