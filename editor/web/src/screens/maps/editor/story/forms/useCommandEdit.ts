// How a command form changes its command: keys of the command (undefined removes one). One undo step per edit, and
// one per burst of typing when a `key` is given.
import { patchCommand, replaceCommand, type ListPath } from '../../../../../model/storyEdit'
import type { Command } from '../../../../../model/storyTypes'
import { useStory } from '../StoryContext'

export interface CommandRef { eventId: string; path: ListPath; index: number }

export function useCommandEdit(r: CommandRef) {
  const { apply } = useStory()
  const key = (k?: string) => (k ? `cmd:${r.eventId}:${JSON.stringify(r.path)}:${r.index}:${k}` : undefined)
  return {
    patch: (p: Record<string, unknown>, k?: string) => apply((m) => patchCommand(m, r.eventId, r.path, r.index, p), key(k)),
    replace: (c: Command, k?: string) => apply((m) => replaceCommand(m, r.eventId, r.path, r.index, c), key(k)),
  }
}
