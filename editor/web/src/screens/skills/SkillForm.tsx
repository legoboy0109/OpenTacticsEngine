// The form of one skill: name, description, its rules as cards, and the sentence the whole skill adds up to.
import { addBlock, blocksOf, moveBlock, removeBlock, updateBlock, updateSkill } from '../../model/skills'
import { MAX_BLOCKS } from '../../model/skillRules'
import { skillSummary } from '../../model/skillText'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { TextInput } from '../../ui/TextInput'
import { BlockCard } from './BlockCard'
import { T } from './text'
import './skills.css'

export function SkillForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const skill = doc.skills?.find((s) => s.id === id)
  if (!skill) return null
  const key = `skill:${id}`
  const blocks = blocksOf(skill)

  return (
    <div className="stack" data-testid="skill-form">
      <Card>
        <FormGrid>
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={skill.name} onChange={(v) => edit((d) => updateSkill(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.description} help={T.descriptionHelp}>
            <TextInput value={skill.description ?? ''} onChange={(v) => edit((d) => updateSkill(d, id, { description: v === '' ? undefined : v }), `${key}:description`)} />
          </Field>
        </FormGrid>
      </Card>

      <h2 className="skill-rules-title">{T.rulesCard}</h2>
      <p className="help">{T.rulesHelp}</p>
      {blocks.map((b, i) => (
        <BlockCard key={i} doc={doc} block={b} index={i} count={blocks.length}
          edit={(fn, k) => edit((d) => updateBlock(d, id, i, fn), k && `${key}:${i}:${k}`)}
          onMove={(delta) => edit((d) => moveBlock(d, id, i, i + delta))}
          onDelete={() => edit((d) => removeBlock(d, id, i))} />
      ))}
      <div>
        <button type="button" className="btn" disabled={blocks.length >= MAX_BLOCKS} onClick={() => edit((d) => addBlock(d, id))}>{T.addRule}</button>
        {blocks.length >= MAX_BLOCKS && <p className="help">{T.ruleLimit}</p>}
      </div>

      <Card title={T.summaryTitle}>
        <p data-testid="skill-summary">{skillSummary(doc, skill)}</p>
      </Card>

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={skill.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
