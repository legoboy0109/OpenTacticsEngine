// The choices for "which units" lists that take unit type ids and labels (tags), shared by a structure's Repairs and the
// extra sight of terrain and structures: every unit type, every label no unit type id shadows, and entries the game no longer
// has (kept visible so they can be un-ticked; the checks name them).
import { allTags } from './units'
import type { ProjectDoc } from './types'

export function unitKeyOptions(
  doc: ProjectDoc,
  current: string[],
  tagLabel: (tag: string) => string,
  missingLabel: (entry: string) => string,
): Array<{ value: string; label: string }> {
  const unitIds = new Set((doc.units ?? []).map((u) => u.id))
  const tags = allTags(doc).filter((t) => !unitIds.has(t))
  const known = new Set([...unitIds, ...tags])
  return [
    ...(doc.units ?? []).map((u) => ({ value: u.id, label: u.name })),
    ...tags.map((t) => ({ value: t, label: tagLabel(t) })),
    ...current.filter((e) => !known.has(e)).map((e) => ({ value: e, label: missingLabel(e) })),
  ]
}
