import {
  TURNS_MAX, needsFirstBlock, setTechCost, setTechDescription, setTechRequires, setTechTurns, setTechUnlocks, turnsOf, updateTechnology,
} from '../../model/research'
import { technologyUses } from '../../model/researchRefs'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field, FieldGroup } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { ResearchBonus } from './ResearchBonus'
import { T } from './text'

export function ResearchForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const tech = doc.research?.find((t) => t.id === id)
  if (!tech) return null
  const key = `tech:${id}`
  const resources = doc.game?.resources ?? []
  const turns = turnsOf(tech)
  const others = (doc.research ?? []).filter((t) => t.id !== id)
  // A prerequisite that would make a loop cannot be ticked; it says why in its name.
  const needs = others.map((t) => {
    const why = needsFirstBlock(doc, id, t.id)
    return { value: t.id, label: why ? `${t.name} ${T.needsLoop(why)}` : t.name, disabled: !!why }
  })
  const uses = technologyUses(doc, id)

  return (
    <div className="stack">
      <Card>
        <div className="stack">
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={tech.name} onChange={(v) => edit((d) => updateTechnology(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput value={tech.description ?? ''} onChange={(v) => edit((d) => setTechDescription(d, id, v), `${key}:description`)} />
          </Field>
          <p className="help" data-testid="tech-uses">{uses.length ? T.usedByList(uses) : T.usedByNone}</p>
        </div>
      </Card>

      <Card>
        <div className="stack">
          {resources.length === 0
            ? <FieldGroup label={T.cost}><p className="help">{T.costNone}</p></FieldGroup>
            : (
              <KeyNumberTable label={T.cost} help={T.costHelp} rows={resources.map((r) => ({ key: r.id, label: r.name }))}
                columns={[{ title: T.costCol, values: tech.cost, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setTechCost(d, id, k, v), `${key}:cost:${k}`) }]} />
            )}
          <FormGrid>
            <Field label={T.turns} help={T.turnsHelp}>
              <NumberInput value={turns} min={0} max={TURNS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setTechTurns(d, id, v), `${key}:turns`) }} />
            </Field>
          </FormGrid>
          <p className="help" data-testid="tech-turns-text">{turns === 0 ? T.turnsNow : T.turnsN(turns)}</p>
        </div>
      </Card>

      <Card>
        <MultiSelect label={T.needs} help={T.needsHelp} emptyText={T.needsNone} value={tech.requires ?? []} options={needs}
          onChange={(v) => edit((d) => setTechRequires(d, id, v))} />
      </Card>

      <Card>
        <MultiSelect learn="tech-unlocks" label={T.unlocks} help={T.unlocksHelp} emptyText={T.unlocksNone} value={tech.unlocks?.units ?? []}
          options={(doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))}
          onChange={(v) => edit((d) => setTechUnlocks(d, id, v))} />
        <p className="help">{T.unlocksSentence}</p>
      </Card>

      <Card>
        <ResearchBonus id={id} />
      </Card>

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={tech.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
