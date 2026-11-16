// The story pieces about supports (E4 of docs/SRPG.md): the pair picker the command, condition and trigger share, and the
// "Add support points" form. A pair is written as `[a, b]` and must be one the Supports screen declares.
import { levelName, pairKey, pairLabel, pairsOf } from '../../../../../model/supports'
import type { Command } from '../../../../../model/storyTypes'
import type { ProjectDoc } from '../../../../../model/types'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

const ANY = 'any'

/** The first declared pair, the default of a new command, condition or trigger (empty ids when there is none yet). */
export const firstPair = (doc: ProjectDoc): [string, string] => {
  const p = pairsOf(doc)[0]
  return p ? [p.a, p.b] : ['', '']
}

/** "Which pair": only declared pairs are offered; a pair in the file that is not declared stays visible as "(not there any more)". */
export function PairPick({ value, onChange, label = S.supportPair }: { value: unknown; onChange: (pair: [string, string]) => void; label?: string }) {
  const { doc } = useStory()
  const pairs = pairsOf(doc)
  const cur = Array.isArray(value) && value.length === 2 && typeof value[0] === 'string' && typeof value[1] === 'string' ? (value as [string, string]) : (['', ''] as [string, string])
  const key = cur[0] === '' && cur[1] === '' ? '' : pairKey({ a: cur[0], b: cur[1] })
  // The file may name the pair the other way round: the same pair, so it is shown as chosen.
  const declared = pairs.find((p) => pairKey(p) === key || pairKey({ a: p.b, b: p.a }) === key)
  return (
    <PickSelect label={label} value={declared ? pairKey(declared) : key} help={S.supportPairHelp} empty={S.supportNoPairs}
      options={pairs.map((p) => ({ value: pairKey(p), label: pairLabel(doc, p) }))}
      onChange={(v) => { const p = pairs.find((x) => pairKey(x) === v); if (p) onChange([p.a, p.b]) }} />
  )
}

export function AddSupportForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight">
      <PairPick value={cmd.addSupport} onChange={(pair) => patch({ addSupport: pair })} />
      <Field label={S.supportBy} help={S.supportByHelp}>
        <NumberInput value={cmd.by ?? 0} min={-1_000_000} max={1_000_000} onChange={(v) => { if (v !== undefined) patch({ by: v }, 'by') }} />
      </Field>
    </div>
  )
}

/** The level a trigger waits for: any, or one of the pair's levels (named as the game names them). */
export function SupportLevelPick({ pair, value, onChange }: { pair: unknown; value: number | undefined; onChange: (level: number | undefined) => void }) {
  const { doc } = useStory()
  const p = Array.isArray(pair) ? pairsOf(doc).find((x) => (x.a === pair[0] && x.b === pair[1]) || (x.a === pair[1] && x.b === pair[0])) : undefined
  const options = [{ value: ANY, label: S.supportAnyLevel }, ...(p?.levels ?? []).map((_, i) => ({ value: String(i + 1), label: `Level ${levelName(doc.supports?.rules, i)}` }))]
  return <PickSelect label={S.supportTriggerLevel} value={value === undefined ? ANY : String(value)} options={options} help={S.supportTriggerHelp}
    onChange={(v) => onChange(v === ANY ? undefined : Number(v))} />
}
