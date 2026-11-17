// What the timeline remembers while the editor is open (not saved to disk): where the picture was, which chapter was selected,
// which levels were expanded, and which box the person left from. Coming back from a box's editor restores it all.
// Per project, so two games open in turn do not mix.
export interface TimelineMemory {
  x: number
  y: number
  k: number
  /** False until the first fit: a new timeline starts fitted, a remembered one keeps its view. */
  placed: boolean
  selected: string | null
  expanded: Set<string>
  /** The box that was open when the person left; it gets the keyboard focus back. */
  returnTo: string | null
  /** Which scene card the chapter page should scroll to ("before" / "after"). */
  scrollTo: 'before' | 'after' | null
}

const all = new Map<string, TimelineMemory>()

export function memoryOf(projectId: string): TimelineMemory {
  let m = all.get(projectId)
  if (!m) {
    m = { x: 0, y: 0, k: 1, placed: false, selected: null, expanded: new Set(), returnTo: null, scrollTo: null }
    all.set(projectId, m)
  }
  return m
}
