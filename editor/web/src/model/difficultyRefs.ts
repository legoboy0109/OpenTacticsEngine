// What deleting a difficulty takes with it (docs/DIFFICULTY.md): the lines of the delete question. The clean-up itself is
// `removeDifficulty` in difficulty.ts; the events' and scenes' part is the story walk (storyWalk.ts, kind 'difficulty').
import { difficultyById, difficultyName } from './difficulty'
import type { Usage } from './references'
import { storyUsages } from './storyRefs'
import type { ProjectDoc } from './types'

const mapName = (doc: ProjectDoc, id: string): string => doc.game?.maps.find((m) => m.id === id)?.name ?? id

export function difficultyUsages(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  const name = difficultyName(doc, id)
  if (doc.game?.difficulties?.default === id && doc.game.difficulties.list.length > 1)
    out.push({ section: 'rules', text: 'It is the game’s default: the first of the others becomes the default' })
  for (const [mid, m] of Object.entries(doc.maps)) {
    if (!m) continue
    const prefix = `Map “${mapName(doc, mid)}”`
    if (m.difficulty?.[id]) out.push({ section: 'maps', text: `${prefix}: its own numbers for “${name}” are removed` })
    for (const u of m.units ?? []) {
      if (!u.difficulty?.includes(id)) continue
      const who = `${u.name || u.id || u.type || 'A unit'} (${u.x + 1}, ${u.y + 1})`
      out.push({ section: 'maps', text: u.difficulty.length === 1
        ? `${prefix}: ${who} was only on “${name}” and now appears on every difficulty`
        : `${prefix}: ${who} is no longer limited to “${name}”` })
    }
  }
  out.push(...storyUsages(doc, [{ kind: 'difficulty', id }]))
  if (!difficultyById(doc, id)) return []
  return out
}
