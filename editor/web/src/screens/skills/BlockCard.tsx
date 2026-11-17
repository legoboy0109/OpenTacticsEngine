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
  /** `data-learn` name for the tutorial's pointing ring. */
  learn?: string
  /** A rule of a status condition (the Conditions screen): its issues and stat choices follow the condition rules. */
  forCondition?: boolean
  /** The card's number as the maker sees it (the Conditions form hides the blocks it shows as switches). */
  shownNumber?: number
  /** Show the delete button even when this is the only card shown (the Conditions form keeps other blocks out of sight). */
  canDelete?: boolean
}

export function BlockCard({ doc, block, index, count, edit, onMove, onDelete, learn, forCondition, shownNumber, canDelete }: Props) {
  const when = whenOf(block)
  const issues = ruleIssues(doc, block, forCondition)
  const n = shownNumber ?? index + 1
  return (
    <section className="card skill-rule" role="group" aria-label={T.rule(n)} data-testid="skill-rule">
      <header className="card-head">
        <h3>{T.rule(n)}</h3>
        {(count > 1 || canDelete) && (
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
          <Select data-learn={learn} value={when ?? 'always'} options={WHENS.map((w) => ({ value: w, label: T.whens[w] }))} onChange={(w) => edit((b) => setWhen(b, w as When, doc))} />
        </Field>
        <h4 className="skill-part">{T.onlyIf}</h4>
        <ConditionRows doc={doc} block={block} setBlock={edit} />
        <ChanceRow doc={doc} block={block} setBlock={edit} />
        <ReachesRow doc={doc} block={block} setBlock={edit} />
        <h4 className="skill-part">{T.then}</h4>
        <EffectRows doc={doc} block={block} setBlock={edit} forCondition={forCondition} />
        <p className="skill-sentence" data-testid="skill-rule-summary">{blockSummary(doc, block)}</p>
      </div>
    </section>
  )
}
