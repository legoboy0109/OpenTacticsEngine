// "Research can be done here" (only while the game's rule says research needs a structure) and "Repairs" on a
// structure kind (docs/ECONOMY.md).
import { allTags } from '../../model/units'
import { repairOn } from '../../model/supplyRules'
import { researchNeedsStructure } from '../../model/optionRules'
import { repairsAll, setRepairs, setResearchHere } from '../../model/structuresEconomy'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { Field } from '../../ui/Field'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function ResearchHereCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  // Without the rule nothing needs this flag: one line says so and links to the rule. An existing `research: true`
  // stays in the file untouched (it matters again the moment the rule is switched on).
  if (!researchNeedsStructure(doc))
    return (
      <Card>
        <p className="help" data-testid="research-rule-off">{T.researchRuleOff} <Link to={projectPath(store.id, 'rules')}>{T.openRules}</Link></p>
      </Card>
    )
  return (
    <Card>
      <div className="stack">
        <Check label={T.researchHere} help={T.researchHereHelp} checked={s.research === true} onChange={(on) => edit((d) => setResearchHere(d, id, on))} />
        {s.research === true && (doc.research?.length ?? 0) === 0 && (
          <p className="help" data-testid="research-here-none">{T.researchHereNone} <Link to={projectPath(store.id, 'research')}>{T.openResearch}</Link></p>
        )}
      </div>
    </Card>
  )
}

/** Which units the structure repairs. Shown only while the game has a repair rule (or the structure already has a list). */
export function RepairsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  const on = repairOn(doc)
  const all = repairsAll(s)
  const list = s.repairs ?? []
  const unitIds = new Set((doc.units ?? []).map((u) => u.id))
  const tags = allTags(doc).filter((t) => !unitIds.has(t))
  const known = new Set([...unitIds, ...tags])
  const options = [
    ...(doc.units ?? []).map((u) => ({ value: u.id, label: u.name })),
    ...tags.map((t) => ({ value: t, label: T.repairsTag(t) })),
    // Entries the game no longer has stay visible so they can be un-ticked (the checks name them).
    ...list.filter((e) => !known.has(e)).map((e) => ({ value: e, label: T.missing(e) })),
  ]
  return (
    <Card>
      <div className="stack">
        <Field label={T.repairs} help={T.repairsHelp}>
          <Select value={all ? 'all' : 'only'} options={[{ value: 'all', label: T.repairsAll }, { value: 'only', label: T.repairsOnly }]}
            onChange={(v) => edit((d) => setRepairs(d, id, v === 'all' ? undefined : []))} />
        </Field>
        {!all && (
          <>
            <MultiSelect label={T.repairsPick} value={list} options={options} onChange={(v) => edit((d) => setRepairs(d, id, v))} />
            {list.length === 0 && <p className="help" data-testid="repairs-nothing">{T.repairsNothing}</p>}
          </>
        )}
        {!on && <p className="help">{T.repairsOff}</p>}
      </div>
    </Card>
  )
}
