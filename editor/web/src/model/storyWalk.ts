// One walk over everything a map's events and objectives NAME (units, sides, teams, unit types, items, resources,
// switches, variables). The same walk serves four jobs, so the knowledge of where ids sit is written once:
//   - rename a unit's story id (the callback returns a new id),
//   - clean up after a delete (the callback returns DROP; the smallest thing that cannot work any more is removed:
//     an event, a command, an objective entry, one resource of a gift, one item of an added unit),
//   - list what would be removed ("used by", `dropped` gets a sentence for each),
//   - check that every name exists (the callback gets `where`, a plain place such as `event “ambush”, command 2`).
// A walk returns the very same objects when nothing changed, so callers can tell cheaply.
import { entryId } from './itemGrants'
import type { Command, Condition, Filter, ObjectiveRule, Objectives, StoryEvent, Trigger } from './storyTypes'
import type { ItemEntry, MapFile } from './types'

/** `teamOrSide`: a win / lose names a team or, as a shorthand, a side id. */
/** `chapter` and `map` only occur in a campaign (campaignWalk.ts); `goto` in a scene names a chapter. */
export type RefKind = 'unit' | 'side' | 'team' | 'teamOrSide' | 'type' | 'item' | 'resource' | 'switch' | 'variable' | 'chapter' | 'map' | 'tech' | 'music' | 'sound' | 'skill'
export interface Ref { kind: RefKind; id: string }
export const DROP = Symbol('drop')
export type Dropped = typeof DROP

export interface Dropping { what: 'event' | 'command' | 'rule' | 'resource' | 'item' | 'chapterMap' | 'link' | 'freeBattle' | 'setting'; text: string; where: string }

export interface Walk {
  /** Called for every place that names something. Return the id to keep there (the same, or a new one) or DROP. */
  ref(r: Ref, where: string): string | Dropped
  /** Called for each thing that is removed; `text` is its plain sentence (from `describe`). */
  dropped?(info: Dropping): void
  describe?: { event?(e: StoryEvent): string; command?(c: Command): string; rule?(r: ObjectiveRule): string }
}

/** Copy of `o` with key `k` set, or `o` itself when the value is already that. */
export function put<T extends object>(o: T, k: string, v: unknown): T {
  const rec = o as Record<string, unknown>
  return rec[k] === v ? o : ({ ...rec, [k]: v } as T)
}

function mapFilter(f: Filter | string, w: Walk, where: string): Filter | string | Dropped {
  if (typeof f === 'string') return w.ref({ kind: 'unit', id: f }, where)
  for (const k of ['unit', 'side', 'team', 'type'] as const) {
    const v = f[k]
    if (typeof v !== 'string') continue
    const r = w.ref({ kind: k, id: v }, where)
    return r === DROP ? DROP : put(f, k, r)
  }
  return f
}

/** Maps the filter held under `key` (if any); DROP when it cannot stay. */
function withFilter<T extends object>(o: T, key: string, w: Walk, where: string): T | Dropped {
  const f = (o as Record<string, unknown>)[key] as Filter | string | undefined
  if (f === undefined || f === null || (typeof f !== 'string' && typeof f !== 'object')) return o
  const r = mapFilter(f, w, where)
  return r === DROP ? DROP : put(o, key, r)
}

export function withRef<T extends object>(o: T, key: string, kind: RefKind, w: Walk, where: string): T | Dropped {
  const v = (o as Record<string, unknown>)[key]
  if (typeof v !== 'string') return o
  const r = w.ref({ kind, id: v }, where)
  return r === DROP ? DROP : put(o, key, r)
}

/** Runs the steps in order on `o`; stops at the first DROP. */
export function chain<T extends object>(o: T, steps: Array<(x: T) => T | Dropped>): T | Dropped {
  let cur = o
  for (const s of steps) {
    const r = s(cur)
    if (r === DROP) return DROP
    cur = r
  }
  return cur
}

function mapTrigger(t: Trigger, w: Walk, where: string): Trigger | Dropped {
  return chain(t, [
    (x) => withRef(x, 'side', 'side', w, where),
    (x) => withFilter(x, 'by', w, where),
    (x) => withFilter(x, 'defeated', w, where),
    (x) => withFilter(x, 'fight', w, where),
    (x) => withFilter(x, 'with', w, where),
    (x) => withFilter(x, 'promoted', w, where),
    (x) => withRef(x, 'talk', 'unit', w, where),
    (x) => withRef(x, 'to', 'unit', w, where),
  ])
}

function mapCondition(c: Condition, w: Walk, where: string): Condition | Dropped {
  return chain(c, [
    (x) => withRef(x, 'switch', 'switch', w, where),
    (x) => withRef(x, 'variable', 'variable', w, where),
    (x) => withFilter(x, 'alive', w, where),
    (x) => withFilter(x, 'dead', w, where),
    (x) => withFilter(x, 'sees', w, where),
    (x) => withRef(x, 'team', 'teamOrSide', w, where), // only a `sees` condition has one (a side id means that side's team)
    (x) => withRef(x, 'carries', 'item', w, where),
    (x) => withRef(x, 'researched', 'tech', w, where),
    (x) => withRef(x, 'hasSkill', 'skill', w, where),
    (x) => withRef(x, 'side', 'side', w, where), // only a `researched` condition has one
    (x) => withFilter(x, 'unit', w, where),
  ])
}

/** All conditions must survive: a condition that cannot be kept takes the whole list (and its owner) with it. */
function mapConditions(list: Condition[], w: Walk, where: string): Condition[] | Dropped {
  const out: Condition[] = []
  let changed = false
  for (const c of list) {
    const r = mapCondition(c, w, where)
    if (r === DROP) return DROP
    if (r !== c) changed = true
    out.push(r)
  }
  return changed ? out : list
}

function mapRule(r: ObjectiveRule, w: Walk, where: string): ObjectiveRule | Dropped {
  return chain(r, [(x) => withFilter(x, 'defeated', w, where), (x) => withFilter(x, 'by', w, where)])
}

/** `key` is "win" or "lose": a rule that cannot stay is removed from the list on its own. */
function mapRules(list: ObjectiveRule[], w: Walk, where: string): ObjectiveRule[] {
  const out: ObjectiveRule[] = []
  let changed = false
  for (const rule of list) {
    const r = mapRule(rule, w, where)
    if (r === DROP) { changed = true; w.dropped?.({ what: 'rule', text: w.describe?.rule?.(rule) ?? 'An objective', where }); continue }
    if (r !== rule) changed = true
    out.push(r)
  }
  return changed ? out : list
}

/** An emptied win / lose list loses its key: "no rules" is written as no list. */
export function mapObjectives(o: Objectives, w: Walk, where: string): Objectives {
  let out = o
  for (const k of ['win', 'lose'] as const) {
    const list = o[k]
    if (!Array.isArray(list)) continue
    const next = mapRules(list, w, `${where}, “${k === 'win' ? 'Win' : 'Lose'}”`)
    if (next === list) continue
    if (next.length) out = put(out, k, next)
    else { const { [k]: _gone, ...rest } = out; out = rest as Objectives }
  }
  return out
}

/** An added unit's items: the ones that no longer exist leave its list (the unit stays). */
function mapItems(list: ItemEntry[], w: Walk, where: string, owner: Command): ItemEntry[] {
  const kept = list.filter((e) => w.ref({ kind: 'item', id: entryId(e) }, where) !== DROP)
  if (kept.length !== list.length) w.dropped?.({ what: 'item', text: w.describe?.command?.(owner) ?? 'An added unit', where })
  return kept.length === list.length ? list : kept
}

function mapAddUnit(c: Command, w: Walk, where: string): Command | Dropped {
  const u = c.addUnit
  if (!u || typeof u !== 'object') return c
  const stepped = chain(u, [(x) => withRef(x, 'type', 'type', w, where), (x) => withRef(x, 'side', 'side', w, where)])
  if (stepped === DROP) return DROP
  const items = Array.isArray(stepped.items) ? mapItems(stepped.items, w, where, c) : stepped.items
  const next = items === stepped.items ? stepped : { ...stepped, items }
  return next === u ? c : { ...c, addUnit: next }
}

function mapGiveResource(c: Command, w: Walk, where: string): Command | Dropped {
  const amounts = c.giveResource
  if (!amounts || typeof amounts !== 'object') return c
  const kept = Object.entries(amounts).filter(([id]) => w.ref({ kind: 'resource', id }, where) !== DROP)
  if (kept.length === Object.keys(amounts).length) return c
  if (!kept.length) return DROP
  w.dropped?.({ what: 'resource', text: w.describe?.command?.(c) ?? 'A gift', where })
  return { ...c, giveResource: Object.fromEntries(kept) }
}

/** One command, not looking inside `if` branches (mapCommands does that so each inner command can go on its own). */
function mapCommand(c: Command, w: Walk, where: string): Command | Dropped {
  if (c.addUnit !== undefined) return mapAddUnit(c, w, where)
  if (c.giveResource !== undefined) {
    const g = mapGiveResource(c, w, where)
    return g === DROP ? DROP : withRef(g, 'side', 'side', w, where)
  }
  if (c.objective !== undefined && typeof c.objective === 'object') {
    const o = mapObjectives(c.objective, w, where)
    return o === c.objective ? c : { ...c, objective: o }
  }
  if (Array.isArray(c.if)) {
    const conds = mapConditions(c.if, w, where)
    return conds === DROP ? DROP : put(c, 'if', conds)
  }
  return chain(c, [
    (x) => withRef(x, 'speaker', 'unit', w, where),
    (x) => withFilter(x, 'removeUnit', w, where),
    (x) => withRef(x, 'moveUnit', 'unit', w, where),
    (x) => withFilter(x, 'changeSide', w, where),
    (x) => withRef(x, 'promote', 'unit', w, where),
    (x) => (x.promote !== undefined ? withRef(x, 'to', 'type', w, where) : x), // promote's `to` is a unit type id
    (x) => withRef(x, 'teachSkill', 'skill', w, where),
    (x) => withRef(x, 'forgetSkill', 'skill', w, where),
    (x) => withFilter(x, 'from', w, where),
    (x) => withRef(x, 'side', 'side', w, where),
    (x) => withRef(x, 'giveItem', 'item', w, where),
    (x) => withRef(x, 'grantResearch', 'tech', w, where),
    (x) => withRef(x, 'playMusic', 'music', w, where),
    (x) => withRef(x, 'playSound', 'sound', w, where),
    (x) => (typeof x.to === 'object' && x.to !== null ? withFilter(x, 'to', w, where) : x),
    (x) => withRef(x, 'setSwitch', 'switch', w, where),
    (x) => withRef(x, 'setVariable', 'variable', w, where),
    (x) => withRef(x, 'addVariable', 'variable', w, where),
    (x) => withRef(x, 'win', 'teamOrSide', w, where),
    (x) => withRef(x, 'lose', 'teamOrSide', w, where),
    (x) => (typeof x.focus === 'string' ? withRef(x, 'focus', 'unit', w, where) : x),
    (x) => withRef(x, 'goto', 'chapter', w, where),
  ])
}

/** `trail` numbers the commands of a nested list after the `if` they sit in: "3" at the top, "3.1" inside it. */
export function mapCommands(list: Command[], w: Walk, where: string, trail = ''): Command[] {
  const out: Command[] = []
  let changed = false
  list.forEach((c, i) => {
    const num = trail ? `${trail}.${i + 1}` : `${i + 1}`
    const here = `${where}, command ${num}`
    const first = mapCommand(c, w, here)
    if (first === DROP) { changed = true; w.dropped?.({ what: 'command', text: w.describe?.command?.(c) ?? 'A command', where: here }); return }
    let r: Command = first
    for (const branch of ['then', 'else'] as const) {
      const inner: Command[] | undefined = r[branch]
      if (!Array.isArray(inner)) continue
      const next = mapCommands(inner, w, where, num)
      if (next !== inner) r = { ...r, [branch]: next }
    }
    if (r !== c) changed = true
    out.push(r)
  })
  return changed ? out : list
}

function mapEvents(list: StoryEvent[], w: Walk): StoryEvent[] {
  const out: StoryEvent[] = []
  let changed = false
  for (const e of list) {
    const where = `event “${e.id}”`
    const trig = e.when && typeof e.when === 'object' ? mapTrigger(e.when, w, `${where}, “When”`) : e.when
    const conds = Array.isArray(e.if) ? mapConditions(e.if, w, `${where}, “Only if”`) : e.if
    if (trig === DROP || conds === DROP) { changed = true; w.dropped?.({ what: 'event', text: w.describe?.event?.(e) ?? `Event “${e.id}”`, where }); continue }
    const cmds = Array.isArray(e.do) ? mapCommands(e.do, w, where) : e.do
    let next = e
    if (trig !== e.when) next = put(next, 'when', trig)
    if (conds !== e.if) next = put(next, 'if', conds)
    if (cmds !== e.do) next = put(next, 'do', cmds)
    if (next !== e) changed = true
    out.push(next)
  }
  return changed ? out : list
}

/** Walks the map's events and objectives. Returns the same map when nothing changed. */
export function walkStory(map: MapFile, w: Walk): MapFile {
  let out = map
  if (Array.isArray(map.events)) {
    const next = mapEvents(map.events, w)
    if (next !== map.events) {
      if (next.length) out = { ...out, events: next }
      else { const { events: _gone, ...rest } = out; out = rest as MapFile }
    }
  }
  if (map.objectives && typeof map.objectives === 'object') {
    const next = mapObjectives(map.objectives, w, 'the objectives')
    if (next !== map.objectives) {
      if (Object.keys(next).length) out = { ...out, objectives: next }
      else { const { objectives: _gone, ...rest } = out; out = rest as MapFile }
    }
  }
  return out
}

/** Every command of the list including those inside `if` branches, with where it sits. */
export function forEachCommand(list: Command[] | undefined, where: string, fn: (c: Command, where: string) => void, trail = ''): void {
  if (!Array.isArray(list)) return
  list.forEach((c, i) => {
    const num = trail ? `${trail}.${i + 1}` : `${i + 1}`
    fn(c, `${where}, command ${num}`)
    forEachCommand(c.then, where, fn, num)
    forEachCommand(c.else, where, fn, num)
  })
}
