// "Only on" difficulty checkboxes and the "keep the same on every difficulty" tick of a unit (docs/DIFFICULTY.md). One control for the
// map's unit panel and the story "Add a unit" command: `onChange` gets the patch to apply. Shown only in a game that has difficulties.
import { difficultyList, hasDifficulties, toggleOnlyOn, unknownOnlyOn } from '../../../../model/difficulty'
import type { MapUnit, ProjectDoc } from '../../../../model/types'
import { FieldGroup } from '../../../../ui/Field'
import { Check } from '../../../../ui/Toggle'
import { D } from '../../../rules/difficultyText'

export function DifficultyOnly({ doc, unit, ai, onChange }: { doc: ProjectDoc; unit: Pick<MapUnit, 'difficulty' | 'difficultyShift'>; ai: boolean; onChange: (patch: { difficulty?: string[] | undefined; difficultyShift?: boolean | undefined }) => void }) {
  if (!hasDifficulties(doc) && unit.difficulty === undefined) return null
  if (!ai && unit.difficulty === undefined) return null  // a unit of a human side belongs to the party: it cannot come and go with a difficulty
  const list = difficultyList(doc)
  const all = list.map((d) => d.id)
  const now = unit.difficulty ?? all
  const gone = unknownOnlyOn(doc, unit.difficulty)
  return (
    <div className="stack tight" data-testid="difficulty-only">
      <FieldGroup label={D.onlyOnTitle} help={D.onlyOnHelp}>
        <div className="check-list">
          {list.map((d) => (
            <Check key={d.id} label={d.name || d.id} checked={now.includes(d.id)}
              onChange={(v) => onChange({ difficulty: toggleOnlyOn(unit.difficulty, all, d.id, v) })} />
          ))}
        </div>
        {gone.map((id) => <p className="error-text" key={id}>{D.onlyOnGone(id)}</p>)}
      </FieldGroup>
      {ai && <Check checked={unit.difficultyShift === false} label={D.shiftTitle} help={D.shiftHelp} onChange={(v) => onChange({ difficultyShift: v ? false : undefined })} />}
    </div>
  )
}
