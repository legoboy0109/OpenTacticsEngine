// One rule of a skill as a card: When, Only if, Chance, Reaches, Then, and the sentence it adds up to.
import { blockSummary } from '../../model/skillText'
import { ruleIssues } from '../../model/skillChecks'
import { whenOf, WHENS, type When } from '../../model/skillRules'
import { setWhen } from '../../model/skillBlocks'
import type { Extra, ProjectDoc } from '../../model/types'
import { Banner } from '../../ui/Banner'
import { Field } from '../../ui/Field'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { ChanceRow, ReachesRow } from './BlockOptions'
import { ConditionRows } from './ConditionRows'
import { EffectRows } from './EffectRows'
import { T } from './text'

interface Props {
  doc: ProjectDoc
  block: Extra
  index: number
  count: number
  /** One edit of this rule; `key` joins quick edits (typing a number) into one undo step. */
  edit: (fn: (b: Extra) => Extra, key?: string) => void
  onMove: (delta: number) => void
  onDelete: () => void
}

export function BlockCard({ doc, block, index, count, edit, onMove, onDelete }: Props) {
  const when = whenOf(block)
  const issues = ruleIssues(doc, block)
  const n = index + 1
  return (
    <section className="card skill-rule" role="group" aria-label={T.rule(n)} data-testid="skill-rule">
      <header className="card-head">
        <h3>{T.rule(n)}</h3>
        {count > 1 && (
          <RowButtons index={index} count={count} up={T.moveRuleUp(n)} down={T.moveRuleDown(n)} del={T.deleteRule(n)}
            onUp={() => onMove(-1)} onDown={() => onMove(1)} onDelete={onDelete} />
        )}
      </header>
      <div className="card-body stack">
        {issues.length > 0 && (
          <Banner tone="warn" title="Not valid yet">
            <ul className="usage-list" data-testid="skill-rule-issues">{issues.map((t, i) => <li key={i}>{t}</li>)}</ul>
          </Banner>
        )}
        <Field label={T.when} help={when ? T.whenHelps[when] : T.whenHelp}>
          <Select value={when ?? 'always'} options={WHENS.map((w) => ({ value: w, label: T.whens[w] }))} onChange={(w) => edit((b) => setWhen(b, w as When, doc))} />
        </Field>
        <h4 className="skill-part">{T.onlyIf}</h4>
        <ConditionRows doc={doc} block={block} setBlock={edit} />
        <ChanceRow doc={doc} block={block} setBlock={edit} />
        <ReachesRow doc={doc} block={block} setBlock={edit} />
        <h4 className="skill-part">{T.then}</h4>
        <EffectRows doc={doc} block={block} setBlock={edit} />
        <p className="skill-sentence" data-testid="skill-rule-summary">{blockSummary(doc, block)}</p>
      </div>
    </section>
  )
}
