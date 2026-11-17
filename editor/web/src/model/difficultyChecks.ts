// Checks that mirror what the game loader refuses about difficulties (engine/core/data/loader_difficulty.cpp), plus notes the
// loader does not give, so a maker sees them in the editor before pressing Play. A difficulty that no map or unit refers to is fine
// (it is just a choice for the player): no check says otherwise.
import { MAX_DIFFICULTIES, MAX_FLAT_STAT, MAX_LEVEL_SHIFT, MAX_PERCENT, difficultyById, hasDifficulties } from './difficulty'
import type { DifficultyEnemy } from './difficultyTypes'
import type { MapFile, ProjectDoc } from './types'

export interface DifficultyCheck { level: 'error' | 'hint'; text: string; mapId?: string; cell?: { x: number; y: number } }

const isInt = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

function enemyProblems(doc: ProjectDoc, e: DifficultyEnemy | undefined, say: (t: string) => void): void {
  if (e === undefined) return
  if (e.levels !== undefined && (!isInt(e.levels) || Math.abs(e.levels) > MAX_LEVEL_SHIFT)) say(`the enemy level change must be a whole number from -${MAX_LEVEL_SHIFT} to ${MAX_LEVEL_SHIFT}.`)
  if (e.statsPercent !== undefined && (!isInt(e.statsPercent) || e.statsPercent < 0 || e.statsPercent > MAX_PERCENT)) say(`the stats percent must be a whole number from 0 to ${MAX_PERCENT}.`)
  for (const [stat, v] of Object.entries(e.stats ?? {})) {
    if (!doc.game?.stats.some((s) => s.id === stat)) say(`the stat “${stat}” is not a stat of this game.`)
    else if (!isInt(v) || Math.abs(v) > MAX_FLAT_STAT) say(`the added ${doc.game.stats.find((s) => s.id === stat)?.name ?? stat} must be a whole number from -${MAX_FLAT_STAT} to ${MAX_FLAT_STAT}.`)
  }
}

const percentProblem = (v: unknown, what: string, say: (t: string) => void): void => {
  if (v !== undefined && (!isInt(v) || v < 0 || v > MAX_PERCENT)) say(`${what} must be a whole number from 0 to ${MAX_PERCENT}.`)
}

/** The game's list (Rules screen). */
export function gameDifficultyChecks(doc: ProjectDoc): DifficultyCheck[] {
  const set = doc.game?.difficulties
  if (!set) return []
  const out: DifficultyCheck[] = []
  const err = (text: string) => out.push({ level: 'error', text })
  if (!Array.isArray(set.list) || set.list.length === 0) { err('Difficulties need at least one entry (or remove them all).'); return out }
  if (set.list.length > MAX_DIFFICULTIES) err(`A game can have at most ${MAX_DIFFICULTIES} difficulties.`)
  if (!set.list.some((d) => d.id === set.default)) err(set.default ? `The default difficulty “${set.default}” is not in the list. Pick another default.` : 'There is no default difficulty. Pick one.')
  if (set.change !== undefined && !['never', 'lowerOnly', 'free'].includes(set.change)) err('“Can the player change it later?” must be never, lower only or free.')
  for (const st of set.percentStats ?? []) if (!doc.game?.stats.some((s) => s.id === st)) err(`“Stats percent” names “${st}”, which is not a stat of this game.`)
  const seen = new Set<string>()
  for (const d of set.list) {
    const say = (t: string) => err(`${d.name || d.id}: ${t}`)
    if (seen.has(d.id)) say('another difficulty has the same id.')
    seen.add(d.id)
    if (!d.name?.trim()) say('it needs a name.')
    enemyProblems(doc, d.enemy, say)
    percentProblem(d.xpPercent, 'the experience percent', say)
    percentProblem(d.resourcePercent, 'the resources percent', say)
  }
  if (set.list.length === 1) out.push({ level: 'hint', text: 'With one difficulty the player is never asked which to play; add a second to give a choice.' })
  return out
}

/** Maps: a map's own numbers, a unit's "only on" list. */
export function mapDifficultyChecks(doc: ProjectDoc, mapId: string, map: MapFile): DifficultyCheck[] {
  const out: DifficultyCheck[] = []
  const none = !hasDifficulties(doc)
  for (const [id, o] of Object.entries(map.difficulty ?? {})) {
    const say = (text: string) => out.push({ level: 'error', mapId, text: `This map’s numbers for “${difficultyById(doc, id)?.name ?? id}”: ${text}` })
    if (none) { say('the game has no difficulties (add them on the Rules screen, or remove these numbers).'); continue }
    if (!difficultyById(doc, id)) { say('there is no such difficulty any more. Remove these numbers.'); continue }
    enemyProblems(doc, o.enemy, say)
    percentProblem(o.xpPercent, 'the experience percent', say)
    percentProblem(o.resourcePercent, 'the resources percent', say)
  }
  for (const u of map.units ?? []) {
    if (u.difficulty === undefined) continue
    const who = `${u.name || u.id || u.type || 'A unit'} at ${u.x + 1},${u.y + 1}`
    const at = { x: u.x, y: u.y }
    if (none) { out.push({ level: 'error', mapId, cell: at, text: `${who} is limited to some difficulties, but the game has none (add them on the Rules screen, or clear the limit).` }); continue }
    if (map.sides?.find((s) => s.id === u.side)?.controller !== 'ai') { out.push({ level: 'error', mapId, cell: at, text: `${who} is on a human side, which cannot come and go with a difficulty (the party belongs to the roster). Clear the limit, or make this a unit of a computer side.` }); continue }
    if (!Array.isArray(u.difficulty) || u.difficulty.length === 0) { out.push({ level: 'error', mapId, cell: at, text: `${who}: “Only on” lists no difficulty. Tick some, or clear them all to appear on every difficulty.` }); continue }
    for (const id of u.difficulty)
      if (!difficultyById(doc, id)) out.push({ level: 'error', mapId, cell: at, text: `${who} is limited to the difficulty “${id}”, which does not exist (deleted?). Untick it.` })
    if (u.difficulty.every((id) => !difficultyById(doc, id))) out.push({ level: 'error', mapId, cell: at, text: `${who} appears on no difficulty at all.` })
  }
  return out
}

/** Everything, for a screen that lists it all. */
export const difficultyChecks = (doc: ProjectDoc): DifficultyCheck[] => [
  ...gameDifficultyChecks(doc),
  ...Object.entries(doc.maps).flatMap(([id, m]) => (m ? mapDifficultyChecks(doc, id, m) : [])),
]
