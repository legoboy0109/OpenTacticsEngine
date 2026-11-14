// The game's story switches (off / on) and variables (whole numbers): game.json "switches" and "variables".
// The id is made from the name once and never changes when the name does (events point at the id).
import { withGame } from './doc'
import { makeId } from './ids'
import { scrubDoc, storyUsages } from './storyRefs'
import type { Usage } from './references'
import { moveItem, updateById } from './util'
import type { Named, ProjectDoc } from './types'

export type StoryList = 'switches' | 'variables'

export const storyListOf = (doc: ProjectDoc, key: StoryList): Named[] => doc.game?.[key] ?? []

export const newStoryId = (doc: ProjectDoc, key: StoryList, name: string): string =>
  makeId(name, storyListOf(doc, key).map((x) => x.id), { fallback: key === 'switches' ? 'switch' : 'variable' })

export function addStoryItem(doc: ProjectDoc, key: StoryList, name: string, id: string = newStoryId(doc, key, name)): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, [key]: [...(g[key] ?? []), { id, name }] }))
}

export const renameStoryItem = (doc: ProjectDoc, key: StoryList, id: string, name: string): ProjectDoc =>
  withGame(doc, (g) => ({ ...g, [key]: updateById(g[key] ?? [], id, (x) => ({ ...x, name })) }))

export const moveStoryItem = (doc: ProjectDoc, key: StoryList, from: number, to: number): ProjectDoc =>
  withGame(doc, (g) => ({ ...g, [key]: moveItem(g[key] ?? [], from, to) }))

const kindOf = (key: StoryList) => (key === 'switches' ? 'switch' : 'variable') as 'switch' | 'variable'

export const storyItemUsages = (doc: ProjectDoc, key: StoryList, id: string): Usage[] =>
  storyUsages(doc, [{ kind: kindOf(key), id }])

/** Removes it and every event, command or condition that used it, in one edit. An emptied list leaves its key out. */
export function removeStoryItem(doc: ProjectDoc, key: StoryList, id: string): ProjectDoc {
  const base = scrubDoc(doc, [{ kind: kindOf(key), id }])
  return withGame(base, (g) => {
    const rest = (g[key] ?? []).filter((x) => x.id !== id)
    const { [key]: _old, ...others } = g
    return (rest.length ? { ...others, [key]: rest } : others) as typeof g
  })
}
