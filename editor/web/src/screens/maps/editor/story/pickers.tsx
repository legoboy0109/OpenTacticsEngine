// Pickers for "which thing": a labelled drop-down of what exists (so the form only offers what is valid), and the unit
// filter picker. A value that no longer exists stays visible as "(not there any more)" so it can be seen and changed.
import { newFilter } from '../../../../model/storyDefaults'
import { filterKind, type Filter, type FilterKind } from '../../../../model/storyTypes'
import { Field } from '../../../../ui/Field'
import { Select, type Option } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { useStory } from './StoryContext'
import { S } from './text'

interface PickProps {
  label: string
  value: string
  options: Option[]
  onChange: (v: string) => void
  help?: string
  /** Shown instead of the drop-down when there is nothing to choose from. */
  empty?: string
}

/** A labelled drop-down. Nothing chosen yet shows "Choose…". */
export function PickSelect({ label, value, options, onChange, help, empty }: PickProps) {
  if (options.length === 0 && value === '') return <Field label={label}><p className="help">{empty ?? S.chooseOne}</p></Field>
  const known = options.some((o) => o.value === value)
  const all = value !== '' && !known ? [{ value, label: S.missing(value) }, ...options] : options
  return (
    <Field label={label} help={help}>
      <Select value={value} options={all} placeholder={S.chooseOne} onChange={onChange} />
    </Field>
  )
}

const byName = <T extends { id: string; name?: string }>(list: T[] | null | undefined): Option[] => (list ?? []).map((x) => ({ value: x.id, label: x.name || x.id }))

export function UnitPick({ label, value, onChange, help }: { label: string; value: string; onChange: (v: string) => void; help?: string }) {
  const { units } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} help={help} empty={S.noNamedUnits}
    options={units.map((u) => ({ value: u.id, label: u.label }))} />
}

export function SidePick({ label, value, onChange, help }: { label: string; value: string; onChange: (v: string) => void; help?: string }) {
  const { map } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} help={help} options={byName(map.sides)} />
}

export function TeamPick({ label, value, onChange, help }: { label: string; value: string; onChange: (v: string) => void; help?: string }) {
  const { teams, map } = useStory()
  // A win / lose may also name a side id (a shorthand for that side's team): such a value is shown as the side.
  const side = !teams.some((t) => t.id === value) ? map.sides?.find((s) => s.id === value) : undefined
  const options = [...teams.map((t) => ({ value: t.id, label: t.label })), ...(side ? [{ value: side.id, label: `${side.name || side.id} (side)` }] : [])]
  return <PickSelect label={label} value={value} onChange={onChange} help={help} options={options} />
}

export function TypePick({ label, value, onChange }: { label: string; value: string; onChange: (v: string) => void }) {
  const { doc } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} options={byName(doc.units)} />
}

export function ItemPick({ label, value, onChange, help }: { label: string; value: string; onChange: (v: string) => void; help?: string }) {
  const { doc } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} help={help} empty={S.noItems} options={byName(doc.items)} />
}

export function SwitchPick({ label, value, onChange }: { label: string; value: string; onChange: (v: string) => void }) {
  const { doc } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} empty={S.noSwitches} options={byName(doc.game?.switches)} />
}

export function VariablePick({ label, value, onChange }: { label: string; value: string; onChange: (v: string) => void }) {
  const { doc } = useStory()
  return <PickSelect label={label} value={value} onChange={onChange} empty={S.noVariables} options={byName(doc.game?.variables)} />
}

const KINDS: FilterKind[] = ['any', 'unit', 'side', 'team', 'type', 'tag']

interface FilterProps {
  label: string
  value: Filter | undefined
  onChange: (v: Filter | undefined) => void
  /** required: "any unit" is written as {}. optional: "any unit" is the same as leaving it out. defaultLabel: a "Default" choice leaves it out and "any unit" is written as {}. */
  mode?: 'required' | 'optional' | 'default'
  defaultLabel?: string
}

/** "Which unit(s)": a specific named unit, a side, a team, a unit type, a tag or anyone. */
export function FilterPicker({ label, value, onChange, mode = 'required', defaultLabel }: FilterProps) {
  const story = useStory()
  const kind: FilterKind | 'default' = value === undefined ? (mode === 'default' ? 'default' : 'any') : filterKind(value)
  const options: Option[] = [
    ...(mode === 'default' ? [{ value: 'default', label: S.filterDefault(defaultLabel ?? '') }] : []),
    ...KINDS.map((k) => ({ value: k, label: S.filterKinds[k] })),
  ]
  const pick = (k: string) => {
    if (k === 'default') return onChange(undefined)
    if (k === 'any') return onChange(mode === 'optional' ? undefined : {})
    onChange(filterKind(value) === k && value ? value : newFilter(story, k as FilterKind))
  }
  const set = (key: 'unit' | 'side' | 'team' | 'type' | 'tag') => (v: string) => onChange({ [key]: v })
  return (
    <div className="filter-picker" role="group" aria-label={label}>
      <Field label={label}><Select value={kind} options={options} onChange={pick} /></Field>
      {value && kind === 'unit' && <UnitPick label={`${label}: ${S.filterUnit}`} value={value.unit ?? ''} onChange={set('unit')} />}
      {value && kind === 'side' && <SidePick label={`${label}: ${S.filterSide}`} value={value.side ?? ''} onChange={set('side')} />}
      {value && kind === 'team' && <TeamPick label={`${label}: ${S.filterTeam}`} value={value.team ?? ''} onChange={set('team')} />}
      {value && kind === 'type' && <TypePick label={`${label}: ${S.filterType}`} value={value.type ?? ''} onChange={set('type')} />}
      {value && kind === 'tag' && (
        <Field label={`${label}: ${S.filterTag}`}>
          <TextInput value={value.tag ?? ''} placeholder={S.tagPlaceholder} onChange={set('tag')} />
          {story.tags.length > 0 && <p className="help">{story.tags.join(', ')}</p>}
        </Field>
      )}
    </div>
  )
}
