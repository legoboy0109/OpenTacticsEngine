// "Costs" card of an ability (docs/SRPG.md "Unit resources"): what using it takes, per resource (a pool each unit has, or a resource of the side).
import { SPEND_MAX, setSpend, spendOf } from '../../model/abilities'
import { spendChecks } from '../../model/abilityStrikeChecks'
import { unitResourceList, unitResourceName } from '../../model/unitResources'
import type { Ability } from '../../model/srpgTypes'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { T } from './text'

export function AbilitySpend({ ability, change }: { ability: Ability; change: (fn: (a: Ability) => Ability, k?: string) => void }) {
  const doc = useDoc()
  const pools = unitResourceList(doc).map((r) => ({ id: r.id, name: unitResourceName(r) }))
  const sides = (doc.game?.resources ?? []).map((r) => ({ id: r.id, name: r.name }))
  const all = [...pools, ...sides]
  const spend = spendOf(ability)
  const rows = Object.entries(spend)
  const free = all.filter((r) => !(r.id in spend))
  const nameOf = (id: string) => all.find((r) => r.id === id)?.name ?? id
  // Nothing to pay in and nothing paid: the card would be an empty promise, so it stays out of the way.
  if (all.length === 0 && rows.length === 0) return null
  const problems = spendChecks(doc, ability)
  return (
    <Card title={T.spendCard} help={T.spendHelp}>
      <div className="stack" data-testid="ability-spend">
        {rows.length === 0 && <p className="help">{T.spendNone}</p>}
        {rows.map(([id, n]) => (
          <div className="add-row" key={id}>
            <Field label={T.spendAmount(nameOf(id))}>
              <NumberInput value={n} min={1} max={SPEND_MAX} onChange={(v) => { if (v !== undefined) change((a) => setSpend(a, id, v), `spend:${id}`) }} />
            </Field>
            <button type="button" className="btn ghost sm icon" aria-label={T.spendRemove(nameOf(id))} onClick={() => change((a) => setSpend(a, id, undefined))}><Icon name="trash" /></button>
          </div>
        ))}
        {free.length > 0 && (
          <Select value="" placeholder={T.spendAdd} options={free.map((r) => ({ value: r.id, label: r.name }))} onChange={(v) => { if (v) change((a) => setSpend(a, v, 1)) }} />
        )}
        {problems.map((p) => <Banner key={p} tone="warn" title={p} />)}
      </div>
    </Card>
  )
}
