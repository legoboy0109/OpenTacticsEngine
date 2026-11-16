// The id-or-tag pickers of the rule builder (weapon, terrain, structure, unit type): the game's own things first, then the
// tags / groups they carry. A value the file has that is not in the game stays selectable, marked, so it is never hidden.
import type { Option } from '../../ui/Select'
import type { ProjectDoc } from '../../model/types'
import { T } from './text'

const uniq = (xs: Array<string | undefined>): string[] => [...new Set(xs.filter((x): x is string => !!x))]

function withCurrent(options: Option[], current: string): Option[] {
  return current === '' || options.some((o) => o.value === current) ? options : [...options, { value: current, label: `${current} (not in this game)` }]
}

export const weaponOptions = (doc: ProjectDoc, current: string): Option[] =>
  withCurrent([
    ...(doc.weapons ?? []).map((w) => ({ value: w.id, label: w.name })),
    ...uniq((doc.weapons ?? []).map((w) => w.group)).filter((g) => !(doc.weapons ?? []).some((w) => w.id === g)).map((g) => ({ value: g, label: T.weaponGroup(g) })),
  ], current)

export const terrainOptions = (doc: ProjectDoc, current: string): Option[] =>
  withCurrent([
    ...(doc.terrain ?? []).map((t) => ({ value: t.id, label: t.name })),
    ...uniq((doc.terrain ?? []).flatMap((t) => t.tags ?? [])).filter((g) => !(doc.terrain ?? []).some((t) => t.id === g)).map((g) => ({ value: g, label: T.terrainTag(g) })),
  ], current)

export const structureOptions = (doc: ProjectDoc, current: string): Option[] =>
  withCurrent([
    { value: 'any', label: T.structureAny }, { value: 'none', label: T.structureNone },
    ...(doc.structures ?? []).map((s) => ({ value: s.id, label: s.name })),
    ...uniq((doc.structures ?? []).flatMap((s) => s.tags ?? [])).filter((g) => !(doc.structures ?? []).some((s) => s.id === g)).map((g) => ({ value: g, label: T.structureTag(g) })),
  ], current)

export const unitOptions = (doc: ProjectDoc, current: string): Option[] =>
  withCurrent([
    ...(doc.units ?? []).map((u) => ({ value: u.id, label: u.name })),
    ...uniq((doc.units ?? []).flatMap((u) => u.tags ?? [])).filter((g) => !(doc.units ?? []).some((u) => u.id === g)).map((g) => ({ value: g, label: T.unitTag(g) })),
  ], current)
