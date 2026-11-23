// "Experience and conditions" card of an ability (docs/SRPG.md section 11): what the user earns per use, and a condition the target gets.
import { abilityExtraProblems, abilityInflict, amountText, setAbilityInflict, setAbilityInflictChance, setAbilityXp } from '../../model/fidelityAbility'
import type { Ability } from '../../model/srpgTypes'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

export function AbilityExtras({ ability, change }: { ability: Ability; change: (fn: (a: Ability) => Ability, k?: string) => void }) {
  const doc = useDoc()
  const progression = doc.game?.rules?.progression?.enabled === true
  const conditions = doc.conditions ?? []
  const inflict = abilityInflict(ability)
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
            <Field label={T.inflict} help={T.inflictHelp}>
              <Select value={inflict?.condition ?? ''} placeholder={T.inflictNone}
                options={[{ value: '', label: T.inflictNone }, ...conditions.map((c) => ({ value: c.id, label: c.name }))]}
                onChange={(v) => change((a) => setAbilityInflict(a, v, abilityInflict(a)?.turns))} />
            </Field>
            {inflict && (
              <Field label={T.inflictTurns} help={T.inflictTurnsHelp}>
                <NumberInput value={inflict.turns} allowEmpty min={1} max={99} placeholder={T.inflictOwn}
                  onChange={(v) => change((a) => setAbilityInflict(a, abilityInflict(a)?.condition ?? '', v), 'inflictTurns')} />
              </Field>
            )}
            {inflict && (
              <Field label={T.inflictChance} help={T.inflictChanceHelp}>
                <TextInput value={amountText(inflict.chance)} mono placeholder="100" aria-label={T.inflictChance}
                  onChange={(v) => change((a) => setAbilityInflictChance(a, v), 'inflictChance')} />
              </Field>
            )}
          </>
        )}
        {problems.map((p) => <Banner key={p} tone="warn" title={p} />)}
      </div>
    </Card>
  )
}
