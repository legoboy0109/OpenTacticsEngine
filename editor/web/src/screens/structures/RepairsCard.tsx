// "Research can be done here" and "Repairs" on a structure kind (docs/ECONOMY.md).
import { unitKeyOptions } from '../../model/unitKeys'
import { repairOn } from '../../model/supplyRules'
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
  const options = unitKeyOptions(doc, list, T.repairsTag, T.missing)
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
