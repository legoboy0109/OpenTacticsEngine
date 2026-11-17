// What the game loader refuses about a `choice` command beyond the names it points at (engine/core/data/loader_story_choice.cpp and
// loader_campaign_choice.cpp): the question, the number of answers, their words, and a variable that two questions of one map (or
// one scene) both write. Shared by the map events' checks and the campaign's.
import { MAX_ANSWERS, MIN_ANSWERS, type Command } from './storyTypes'
import { forEachCommand } from './storyWalk'

/** How many `choice` commands of these command lists write each variable ("store"); lists are one map's events or one scene. */
export function storeCounts(lists: Array<Command[] | undefined>): Map<string, number> {
  const n = new Map<string, number>()
  for (const l of lists) forEachCommand(l, '', (c) => { if (typeof c.store === 'string' && c.store && Array.isArray(c.answers)) n.set(c.store, (n.get(c.store) ?? 0) + 1) })
  return n
}

export function choiceChecks(c: Command, where: string, stores: Map<string, number>, say: (where: string, text: string) => void, scene = false): void {
  if (!String(c.choice ?? '').trim()) say(where, 'the question is empty.')
  const answers = Array.isArray(c.answers) ? c.answers : []
  if (answers.length < MIN_ANSWERS || answers.length > MAX_ANSWERS) say(where, `it has ${answers.length} ${answers.length === 1 ? 'answer' : 'answers'}; a question needs two to four.`)
  const seen = new Set<string>()
  answers.forEach((a, i) => {
    const text = String(a.text ?? '')
    if (!text.trim()) say(`${where}, answer ${i + 1}`, 'the words of the answer are empty.')
    else if (seen.has(text)) say(`${where}, answer ${i + 1}`, 'it says the same as an earlier answer, so the player could not tell them apart.')
    seen.add(text)
  })
  if (c.store && (stores.get(c.store) ?? 0) > 1) say(where, `another question of this ${scene ? 'scene' : 'map'} also saves its answer in this variable; use a different variable.`)
}
