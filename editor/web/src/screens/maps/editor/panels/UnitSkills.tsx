// "Skills of its own" of a map unit (and of an addUnit command): skills learned in addition to those of its type and character
// (docs/SRPG.md "More skill blocks"). Shown when the game has skills, or the unit already names some.
import type { ProjectDoc } from '../../../../model/types'
import { MultiSelect } from '../../../../ui/MultiSelect'

interface Props { doc: ProjectDoc; value: string[] | undefined; onChange: (v: string[] | undefined) => void }

export function UnitSkills({ doc, value, onChange }: Props) {
  const skills = doc.skills ?? []
  if (skills.length === 0 && !value?.length) return null
  const options = [...skills.map((s) => ({ value: s.id, label: s.name })), ...(value ?? []).filter((id) => !skills.some((s) => s.id === id)).map((id) => ({ value: id, label: `${id} (not a skill)` }))]
  return (
    <div data-testid="unit-skills">
      <MultiSelect label="Skills of its own" help="Learned for good when the unit is made, in addition to those of its type and character." options={options} value={value ?? []}
        onChange={(v) => onChange(v.length ? v : undefined)} />
    </div>
  )
}
