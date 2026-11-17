// The "Then" part of a rule: a row per effect (a kind picker and that kind's blanks) and a picker to add one. Only the
// effects the rule's "when" allows are offered (model/skillRules.ts). Taking the last effect away is allowed: the rule then
// says it does nothing yet (a check, and the loader's own message) until an effect is added.
import { addEffect, newEffect, offeredEffects, setEffect, setEffects } from '../../model/skillBlocks'
import { FOE_EFFECTS, MAX_EFFECTS, effectKindOf, type EffectKind, type When } from '../../model/skillRules'
import type { Extra, ProjectDoc } from '../../model/types'
import { moveItem } from '../../model/util'
import { Field } from '../../ui/Field'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { EffectFields } from './EffectFields'
import { T } from './text'

interface Props { doc: ProjectDoc; block: Extra; setBlock: (fn: (b: Extra) => Extra, key?: string) => void; forCondition?: boolean }

export function EffectRows({ doc, block, setBlock, forCondition }: Props) {
  const effects = (Array.isArray(block.do) ? block.do : []) as Extra[]
  const when = block.when as When
  const aura = !!block.aura
  const offered = offeredEffects(doc, block)
  const patch = (i: number, e: Extra, key?: string) => setBlock((b) => setEffect(b, i, e), key && `fx:${i}:${key}`)

  return (
    <div className="story-list effects" data-testid="skill-effects">
      {effects.map((e, i) => {
        const kind = effectKindOf(e)
        const n = T.effectN(i + 1)
        const kinds = kind && !offered.includes(kind) ? [...offered, kind] : offered
        return (
          <div className="story-row effect" role="group" aria-label={n} key={i}>
            <div className="story-row-head">
              <Field label={n} help={kind ? T.effectHelps[kind] : undefined}>
                {kind
                  ? <Select value={kind} options={kinds.map((k) => ({ value: k, label: T.effectKinds[k] }))}
                      onChange={(k) => { const fresh = newEffect(k as EffectKind, doc, when); if (fresh) patch(i, e.to === 'foe' && FOE_EFFECTS.includes(k as EffectKind) && !aura ? { ...fresh, to: 'foe' } : fresh) }} />
                  : <p className="help">{T.effectKept}</p>}
              </Field>
              <RowButtons index={i} count={effects.length} up={`Move ${n} up`} down={`Move ${n} down`} del={`Delete ${n}`}
                onUp={() => setBlock((b) => setEffects(b, moveItem(effects, i, i - 1)))} onDown={() => setBlock((b) => setEffects(b, moveItem(effects, i, i + 1)))}
                onDelete={() => setBlock((b) => setEffects(b, effects.filter((_, j) => j !== i)))} />
            </div>
            <EffectFields doc={doc} effect={e} when={when} aura={aura} forCondition={forCondition} change={(next, key) => patch(i, next, key)} />
          </div>
        )
      })}
      {effects.length >= MAX_EFFECTS
        ? <p className="help">{T.effectLimit}</p>
        : (
          <Select aria-label={T.addEffect} value="" placeholder={T.addEffect} options={offered.map((k) => ({ value: k, label: T.effectKinds[k] }))}
            onChange={(k) => { const fresh = newEffect(k as EffectKind, doc, when); if (fresh) setBlock((b) => addEffect(b, fresh)) }} />
        )}
    </div>
  )
}
