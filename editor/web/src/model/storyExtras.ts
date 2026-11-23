// Two small story pieces (docs/EVENTS.md "Branching on who acted" and "Sending a unit's items to storage"): their checks and the walk
// over the item ids a `storeItems` keeps. Kept out of storyChecks.ts / storyWalk.ts, which only call these. The loader is
// engine/core/data/loader_story_actor.cpp, loader_story_store.cpp and loader_campaign_extras.cpp.
import { storageMode } from './itemRules'
import { conditionKind, type Command, type Condition, type TriggerKind } from './storyTypes'
import { DROP, type Walk } from './storyWalk'
import type { ProjectDoc } from './types'

/** Triggers that have a unit acting: the one that moved, fell, attacked, talked, promoted, opened. Turn, battle and support events have none. */
const ACTING: TriggerKind[] = ['reach', 'defeated', 'fight', 'talk', 'promoted', 'place']
export const triggerHasActor = (k: TriggerKind | null): boolean => k !== null && ACTING.includes(k)

/** What the loader refuses about a `storeItems` command (a map event's or a scene's), in plain words. Unknown items and units are the walk's. */
export function storeItemsProblems(doc: ProjectDoc, c: Command): string[] {
  const out: string[] = []
  if (storageMode(doc) === 'off') out.push('the game has no shared storage, so nothing can be sent to it. Turn the storage on in the item rules (Items screen) or leave this command out.')
  if (c.keep !== undefined && !Array.isArray(c.keep)) out.push('the items to keep must be a list.')
  return out
}

/** A condition list that has an `actor` test: a hint when the event it belongs to is set off by nobody (it then never holds). */
export function actorWithoutActor(list: Condition[] | undefined): boolean {
  return (list ?? []).some((k) => conditionKind(k) === 'actor')
}

/** The hint for an event with a `actor` test whose “When” has nobody acting. */
export const ACTOR_NOBODY = 'it asks about “The unit that set this off”, but nothing here sets it off with a unit (its “When” is a turn, the battle or a support level), so that is never true.'

/** Does this command (not the ones nested in its then / else / answers) use `{ "actor": true }` as a unit filter anywhere (its own keys or an `if` condition)? */
export function usesActorFilter(c: Command): boolean {
  const look = (v: unknown, depth: number): boolean => {
    if (depth > 6 || v === null || typeof v !== 'object') return false
    if (Array.isArray(v)) return v.some((x) => look(x, depth + 1))
    const o = v as Record<string, unknown>
    if (o.actor === true) return true
    return Object.entries(o).some(([k, x]) => k !== 'then' && k !== 'else' && k !== 'answers' && look(x, depth + 1))
  }
  return look(c, 0)
}

/** Why a trigger's or an objective's filter cannot be the unit that set the event off. */
export const ACTOR_FILTER_ELSEWHERE = 'it names “The unit that set the event off”, which only a command of an event with a unit that sets it off (a unit reaches, falls, fights, talks, is promoted, uses a door) can name; not a trigger, an objective, a battle / turn / support event or a scene.'
/** The hint for a command that names the acting unit in an event whose “When” has nobody acting. */
export const ACTOR_FILTER_NOBODY = 'it names “The unit that set the event off”, but nothing here sets it off with a unit (its “When” is a turn, the battle or a support level), so the game refuses it.'

/** Commands that make no sense in a “before the preparation screen” event (the loader refuses them): the party is not deployed, no turn has started and the screen's choices rebuild the map it runs on. */
export const BEFORE_PREP_REFUSED = ['win', 'lose', 'endTurn', 'choice', 'fight'] as const

/** A campaign scene has no unit that set it off: the loader refuses `actor` there. */
export const SCENE_ACTOR_PROBLEM = 'a scene has no unit that set it off, so “The unit that set this off” cannot be asked here.'

/** The `keep` list of a `storeItems`: each id is followed through the walk; an item that was deleted leaves the list (the command stays). */
export function withKeepList<T extends object>(o: T, w: Walk, where: string): T | typeof DROP {
  const v = (o as Record<string, unknown>).keep
  if (!Array.isArray(v)) return o
  const next: string[] = []
  for (const id of v) {
    if (typeof id !== 'string') { next.push(id as never); continue }
    const r = w.ref({ kind: 'item', id }, where)
    if (r !== DROP) next.push(r)
  }
  const same = next.length === v.length && next.every((x, i) => x === v[i])
  if (same) return o
  return { ...o, keep: next.length ? next : undefined } as T
}
