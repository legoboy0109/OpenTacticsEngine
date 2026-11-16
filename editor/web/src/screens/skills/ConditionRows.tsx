// The "Only if" part of a rule: a row per condition (a kind picker and that kind's blanks), and a picker to add one.
// Only conditions the rule's "when" allows are offered (model/skillRules.ts), and a row that already has a refused one
// stays so the maker can see and remove it.
import { addCondition, newCondition, noConditionsWhy, offeredConditions, removeCondition, setCondition, setConditions } from '../../model/skillBlocks'
import { FOEABLE, canJudgeFoe, condKindOf, condsFor, type CondKind, type When } from '../../model/skillRules'

import type { Extra, ProjectDoc } from '../../model/types'
import { moveItem } from '../../model/util'
import { Field } from '../../ui/Field'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { ConditionFields } from './ConditionFields'
import { T } from './text'

interface Props { doc: ProjectDoc; block: Extra; setBlock: (fn: (b: Extra) => Extra, key?: string) => void }

export function ConditionRows({ doc, block, setBlock }: Props) {
  const conds = (Array.isArray(block.if) ? block.if : []) as Extra[]
  const when = block.when as When
  const aura = !!block.aura
  const offered = offeredConditions(doc, block)
  const why = noConditionsWhy(doc, block)
  // The kinds listed in the add box; a kind the game cannot make yet is shown but disabled, with the reason.
  const addOptions = condsFor(when, aura)
    .map((k) => ({ value: k, label: offered.includes(k) ? T.condKinds[k] : `${T.condKinds[k]} (${T.condKindsNeed[k]})`, disabled: !offered.includes(k) }))
  const patch = (i: number, c: Extra, key?: string) => setBlock((b) => setCondition(b, i, c), key && `cond:${i}:${key}`)

  return (
    <div className="story-list conditions" data-testid="skill-conditions">
      {conds.length === 0 && <p className="help">{T.onlyIfNone}</p>}
      {conds.map((c, i) => {
        const kind = condKindOf(c)
        const n = T.conditionN(i + 1)
        const kinds = kind && !offered.includes(kind) ? [...offered, kind] : offered
        return (
          <div className="story-row condition" role="group" aria-label={n} key={i}>
            <div className="story-row-head">
              <Field label={n}>
                {kind
                  ? <Select value={kind} options={kinds.map((k) => ({ value: k, label: T.condKinds[k] }))}
                      onChange={(k) => { const fresh = newCondition(k as CondKind, doc); if (fresh) patch(i, c.who === 'foe' && canJudgeFoe(k as CondKind, when, aura) ? { ...fresh, who: 'foe' } : fresh) }} />
                  : <p className="help">{T.condKept}</p>}
              </Field>
              <RowButtons index={i} count={conds.length} up={`Move ${n} up`} down={`Move ${n} down`} del={`Delete ${n}`}
                onUp={() => setBlock((b) => setConditions(b, moveItem(conds, i, i - 1)))} onDown={() => setBlock((b) => setConditions(b, moveItem(conds, i, i + 1)))}
                onDelete={() => setBlock((b) => removeCondition(b, i))} />
            </div>
            {kind && FOEABLE.includes(kind) && canJudgeFoe(kind, when, aura) && (
              <Field label={T.whoLabel}>
                <Select value={c.who === 'foe' ? 'foe' : 'self'} options={[{ value: 'self', label: T.whoSelf }, { value: 'foe', label: T.whoFoe }]}
                  onChange={(v) => { const { who: _w, ...rest } = c; patch(i, v === 'foe' ? { ...rest, who: 'foe' } : rest) }} />
              </Field>
            )}
            <ConditionFields doc={doc} cond={c} change={(next, key) => patch(i, next, key)} />
          </div>
        )
      })}
      {why ? <p className="help" data-testid="skill-no-conditions">{T.noConditionsWhy[why]}</p> : (
        <Select aria-label={T.addCondition} value="" placeholder={T.addCondition} options={addOptions}
          onChange={(k) => { const fresh = newCondition(k as CondKind, doc); if (fresh) setBlock((b) => addCondition(b, fresh)) }} />
      )}
    </div>
  )
}

