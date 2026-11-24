// "Experience and conditions" card of an ability (docs/SRPG.md section 11): what the user earns per use, and a condition the target gets.
import { addInflict, canAddInflict, inflictEntries, removeInflict, setInflictChance, setInflictCondition, setInflictTurns } from '../../model/abilityInflicts'
import { abilityExtraProblems, amountText, setAbilityXp } from '../../model/fidelityAbility'
import type { Ability } from '../../model/srpgTypes'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

export function AbilityExtras({ ability, change }: { ability: Ability; change: (fn: (a: Ability) => Ability, k?: string) => void }) {
  const doc = useDoc()
  const progression = doc.game?.rules?.progression?.enabled === true
  const conditions = doc.conditions ?? []
  const entries = inflictEntries(ability)
  const noUnit = ability.target === 'cell'
  const problems = abilityExtraProblems(doc, ability)
  if (!progression && conditions.length === 0 && ability.xp === undefined && ability.inflict === undefined) return null
  return (
    <Card title={T.extrasCard} help={T.extrasHelp}>
      <div className="stack" data-testid="ability-extras">
        {(progression || ability.xp !== undefined) && (
          <Field label={T.xp} help={T.xpHelp}>
            <TextInput value={amountText(ability.xp)} mono placeholder="0" aria-label={T.xp} onChange={(v) => change((a) => setAbilityXp(a, v), 'xp')} />
          </Field>
        )}
        {(conditions.length > 0 || ability.inflict !== undefined) && !noUnit && (
          <>
            {(entries.length === 0 ? [undefined] : entries).map((e, i) => (
              <div key={i} className="stack tight" data-testid="ability-inflict-row">
                <Field label={entries.length > 1 ? `${T.inflictEntry} ${i + 1}` : T.inflict} help={i === 0 ? T.inflictHelp : undefined}>
                  <Select value={e?.condition ?? ''} placeholder={T.inflictNone}
                    options={[{ value: '', label: T.inflictNone }, ...conditions.map((c) => ({ value: c.id, label: c.name }))]}
                    onChange={(v) => change((a) => setInflictCondition(a, i, v))} />
                </Field>
                {e && (
                  <FormGrid>
                    <Field label={T.inflictTurns} help={T.inflictTurnsHelp}>
                      <NumberInput value={e.turns} allowEmpty min={1} max={99} placeholder={T.inflictOwn} aria-label={`${T.inflictTurns} ${i + 1}`}
                        onChange={(v) => change((a) => setInflictTurns(a, i, v), `inflictTurns${i}`)} />
                    </Field>
                    <Field label={T.inflictChance} help={T.inflictChanceHelp}>
                      <TextInput value={amountText(e.chance)} mono placeholder="100" aria-label={`${T.inflictChance} ${i + 1}`}
                        onChange={(v) => change((a) => setInflictChance(a, i, v), `inflictChance${i}`)} />
                    </Field>
                  </FormGrid>
                )}
                {e && entries.length > 1 && <button type="button" className="btn sm" onClick={() => change((a) => removeInflict(a, i))}>{T.inflictRemove}</button>}
              </div>
            ))}
            {entries.length > 0 && canAddInflict(doc, ability) && <button type="button" className="btn sm" data-testid="ability-inflict-add" onClick={() => change((a) => addInflict(doc, a))}>{T.inflictAdd}</button>}
          </>
        )}
        {problems.map((p) => <Banner key={p} tone="warn" title={p} />)}
      </div>
    </Card>
  )
}
