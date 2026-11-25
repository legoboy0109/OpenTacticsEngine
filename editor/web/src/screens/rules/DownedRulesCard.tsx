// "Downed units" group on the Rules screen: rules.downed in game.json (docs/SRPG.md "Downed units and revive"). Off by default; switching it off
// removes the block. Revive abilities, items, staves and conditions need it.
import { downedAfterBattle, downedRemoveAfter, gameHasDowned, REMOVE_AFTER_MAX, setDownedAfterBattle, setDownedOn, setDownedRemoveAfter } from '../../model/downed'
import { downedRuleChecks } from '../../model/downedChecks'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { D } from '../downed/text'

export function DownedRulesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const on = gameHasDowned(doc)
  const problems = downedRuleChecks(doc)
  return (
    <Card title={D.title} help={D.help}>
      <div className="stack" data-testid="downed-rule">
        <Check label={D.on} help={D.onHelp} checked={on} onChange={(v) => edit((d) => setDownedOn(d, v))} />
        {on && (
          <>
            <Field label={D.afterBattle} help={D.afterBattleHelp}>
              <Select value={downedAfterBattle(doc)} options={[{ value: 'return', label: D.afterBattleOptions.return }, { value: 'fallen', label: D.afterBattleOptions.fallen }]}
                onChange={(v) => edit((d) => setDownedAfterBattle(d, v as 'return' | 'fallen'))} />
            </Field>
            <Field label={D.removeAfter} help={D.removeAfterHelp}>
              <NumberInput value={downedRemoveAfter(doc)} min={0} max={REMOVE_AFTER_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setDownedRemoveAfter(d, v), 'rules:downed:removeAfter') }} />
            </Field>
          </>
        )}
        {problems.length > 0 && <ul className="usage-list" data-testid="downed-rule-issues">{problems.map((p, i) => <li key={i}>{p}</li>)}</ul>}
      </div>
    </Card>
  )
}
