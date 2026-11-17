// The fields of a "difficulty" condition (docs/DIFFICULTY.md): is / is at least / is at most, and which difficulty. Shared by map
// events and campaign scenes (both use ConditionList).
import { difficultyList } from '../../../../model/difficulty'
import type { Condition } from '../../../../model/storyTypes'
import { PickSelect } from './pickers'
import { useStory } from './StoryContext'
import { S } from './text'

type Op = 'is' | 'atLeast' | 'atMost'

const opOf = (c: Condition): Op => (typeof c.difficulty === 'object' && c.difficulty !== null ? (c.difficulty.atMost !== undefined && c.difficulty.atLeast === undefined ? 'atMost' : 'atLeast') : 'is')
const idOf = (c: Condition): string => (typeof c.difficulty === 'string' ? c.difficulty : c.difficulty?.atLeast ?? c.difficulty?.atMost ?? '')

const make = (op: Op, id: string): Condition['difficulty'] => (op === 'is' ? id : op === 'atLeast' ? { atLeast: id } : { atMost: id })

export function DifficultyConditionFields({ c, onChange }: { c: Condition; onChange: (v: Condition['difficulty']) => void }) {
  const { doc } = useStory()
  const op = opOf(c)
  const id = idOf(c)
  return (
    <>
      <PickSelect label={S.conditionDifficultyIs} value={op} onChange={(v) => onChange(make(v as Op, id))}
        options={(['is', 'atLeast', 'atMost'] as Op[]).map((o) => ({ value: o, label: S.conditionDifficultyOps[o] }))} />
      <PickSelect label={S.conditionDifficulty} value={id} onChange={(v) => onChange(make(op, v))}
        options={difficultyList(doc).map((d) => ({ value: d.id, label: d.name || d.id }))} />
      <p className="help">{S.conditionDifficultyHelp}</p>
    </>
  )
}
