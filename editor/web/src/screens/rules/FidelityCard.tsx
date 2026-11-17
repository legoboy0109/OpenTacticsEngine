// "Hit roll and experience options" on the Rules screen (docs/SRPG.md section 11): rules.hitRoll, and the two extra keys of
// rules.progression, shown while experience is on.
import { computerGains, fidelityRuleProblems, hitRoll, setComputerGains, setHitRoll, setXpNoDamage, xpNoDamageText, type HitRoll } from '../../model/fidelityRules'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function FidelityCard() {
  const doc = useDoc()
  const edit = useEdit()
  const xpOn = doc.game?.rules?.progression?.enabled === true
  const problems = fidelityRuleProblems(doc)
  return (
    <Card title={T.fidelity} help={T.fidelityHelp}>
      <div className="stack" data-testid="fidelity-rules">
        <Field label={T.hitRoll} help={T.hitRollHelp}>
          <Select value={hitRoll(doc)} onChange={(v) => edit((d) => setHitRoll(d, v as HitRoll))}
            options={[{ value: 'single', label: T.hitRollSingle }, { value: 'average2', label: T.hitRollAverage }]} />
        </Field>
        {hitRoll(doc) === 'average2' && <p className="help" data-testid="hit-roll-note">{T.hitRollNote}</p>}
        {xpOn && (
          <>
            <Field label={T.xpNoDamage} help={T.xpNoDamageHelp}>
              <TextInput value={xpNoDamageText(doc)} mono placeholder="0" aria-label={T.xpNoDamage}
                onChange={(v) => edit((d) => setXpNoDamage(d, v), 'rules:xpNoDamage')} />
            </Field>
            <Check label={T.computerGains} help={T.computerGainsHelp} checked={computerGains(doc)}
              onChange={(v) => edit((d) => setComputerGains(d, v))} />
          </>
        )}
        {problems.map((p) => <Banner key={p} tone="warn" title={p} />)}
      </div>
    </Card>
  )
}
